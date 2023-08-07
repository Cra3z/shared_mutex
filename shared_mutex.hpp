#pragma once
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <type_traits>
#include <stdexcept>

namespace ccat {

class writer_first_shared_mutex {
public:
    writer_first_shared_mutex() = default;
    writer_first_shared_mutex(const writer_first_shared_mutex&) = delete;
    writer_first_shared_mutex(writer_first_shared_mutex&&) = delete;
    auto operator= (const writer_first_shared_mutex&) ->writer_first_shared_mutex& = delete;
    auto operator= (writer_first_shared_mutex&&) ->writer_first_shared_mutex& = delete;
    ~writer_first_shared_mutex() = default;

    auto lock() ->void {
        std::unique_lock<std::mutex> ulk{mtx_};
        if (wthread_active_cnt_ > 0 || rthread_active_cnt_ > 0) {
            ++wthread_wait_cnt_;
            write_cv_.wait(ulk, [this]{
                return wthread_active_cnt_ == 0 && rthread_active_cnt_ == 0;
            });
            --wthread_wait_cnt_;
        }
        ++wthread_active_cnt_;
    }
    auto try_lock() ->bool {
        std::unique_lock<std::mutex> ulk{mtx_};
        if (wthread_active_cnt_ > 0 || rthread_active_cnt_ > 0) return false;
        ++wthread_active_cnt_;
        return true;
    }
    auto unlock() ->void {
        std::unique_lock<std::mutex> ulk{mtx_};
        --wthread_active_cnt_;
        if (wthread_wait_cnt_ > 0) {
            write_cv_.notify_one();
        }
        else if (rthread_wait_cnt_ > 0) {
            read_cv_.notify_all();
        }
    }
    auto lock_shared() ->void {
        std::unique_lock<std::mutex> ulk{mtx_};
        if (wthread_active_cnt_ > 0 || wthread_wait_cnt_ > 0) {
            ++rthread_wait_cnt_;
            read_cv_.wait(ulk, [this] {
                return wthread_active_cnt_ == 0 && wthread_wait_cnt_ == 0;
            });
            --rthread_wait_cnt_;
        }
        ++rthread_active_cnt_;
    }
    auto try_lock_shared() ->bool {
        std::unique_lock<std::mutex> ulk{mtx_};
        if (wthread_active_cnt_ > 0 || wthread_wait_cnt_ > 0) return false;
        ++rthread_active_cnt_;
        return true;
    }
    auto unlock_shared() ->void {
        std::unique_lock<std::mutex> ulk{mtx_};
        --rthread_active_cnt_;
        if (wthread_wait_cnt_ > 0 && rthread_active_cnt_ == 0) {
            write_cv_.notify_one();
        }
    }
private:
    std::mutex              mtx_;
    std::condition_variable read_cv_;
    std::condition_variable write_cv_;
    size_t rthread_active_cnt_{}, wthread_active_cnt_{}, rthread_wait_cnt_{}, wthread_wait_cnt_{}; 
};

class reader_first_shared_mutex {
public:
    reader_first_shared_mutex() = default;
    reader_first_shared_mutex(const reader_first_shared_mutex&) = delete;
    reader_first_shared_mutex(reader_first_shared_mutex&&) = delete;
    auto operator= (const reader_first_shared_mutex&) ->reader_first_shared_mutex& = delete; 
    auto operator= (reader_first_shared_mutex&&) ->reader_first_shared_mutex& = delete;
    ~reader_first_shared_mutex() = default;

    auto lock() ->void {
        std::unique_lock<std::mutex> ulk{mtx_};
        if (wthread_active_cnt_ > 0 || rthread_active_cnt_ > 0) {
            ++wthread_wait_cnt_;
            write_cv_.wait(ulk, [this]{
                return wthread_active_cnt_ == 0 && rthread_active_cnt_ == 0;
            });
            --wthread_wait_cnt_;
        }
        ++wthread_active_cnt_;
    }
    auto try_lock() ->bool {
        std::unique_lock<std::mutex> ulk{mtx_};
        if (wthread_active_cnt_ > 0 || rthread_active_cnt_ > 0) return false;
        ++wthread_active_cnt_;
        return true;
    }
    auto unlock() ->void {
        std::unique_lock<std::mutex> ulk{mtx_};
        --wthread_active_cnt_;
        if (rthread_wait_cnt_ > 0) {
            read_cv_.notify_all();
        }
        else if (wthread_wait_cnt_ > 0) {
            write_cv_.notify_one();
        }
    }
    auto lock_shared() ->void {
        std::unique_lock<std::mutex> ulk{mtx_};
        if (wthread_active_cnt_ > 0) {
            ++rthread_wait_cnt_;
            read_cv_.wait(ulk, [this]{
                return wthread_active_cnt_ == 0;
            });
            --rthread_wait_cnt_;
        }
        ++rthread_active_cnt_;
    }
    auto try_lock_shared() ->bool {
        std::unique_lock<std::mutex> ulk{mtx_};
        if (wthread_active_cnt_ > 0) return false;
        ++rthread_active_cnt_;
        return true;
    }
    auto unlock_shared() ->void {
        std::unique_lock<std::mutex> ulk{mtx_};
        --rthread_active_cnt_;
        if (rthread_active_cnt_ == 0 && wthread_wait_cnt_ > 0) {
            write_cv_.notify_one();
        }
    }
private:
    std::mutex              mtx_;
    std::condition_variable read_cv_;
    std::condition_variable write_cv_;
    size_t rthread_active_cnt_{}, wthread_active_cnt_{}, rthread_wait_cnt_{}, wthread_wait_cnt_{}; 
};

using shared_mutex = writer_first_shared_mutex;


namespace detail {

template<bool cond, typename T = void>
using enable_if_t = typename std::enable_if<cond, T>::type;

template<typename...>
using void_t = void;

template<typename Mtx, typename = void>
struct enable_locked_and_unlocked : std::false_type {};

template<typename Mtx> 
struct enable_locked_and_unlocked<Mtx, void_t<decltype(std::declval<Mtx&>().lock(), std::declval<Mtx&>().try_lock(), std::declval<Mtx&>().unlock())>> : std::true_type {};

template<typename Mtx, typename = void>
struct is_shared_mutex : std::false_type {};

template<typename Mtx>
struct is_shared_mutex<Mtx, void_t<enable_if_t<enable_locked_and_unlocked<Mtx>::value>, decltype(std::declval<Mtx&>().lock_shared(), std::declval<Mtx&>().try_lock_shared(), std::declval<Mtx&>().unlock_shared())>> : std::true_type {};

}

template<typename Mtx>
class shared_lock final {
    static_assert(!std::is_reference<Mtx>::value, "type `Mtx` can't be a reference type");
    static_assert(detail::is_shared_mutex<Mtx>::value, "type `shared_lock` must be able to `lock`, `unlock`, `try_lock`, `lock_shared`, `unlock_shared` and `try_lock_shared`");
public:
    using mutex_type = Mtx;
public:
    shared_lock(Mtx& mtx_) noexcept : pmtx_(std::addressof(mtx_)), own_(false) {
        pmtx_->lock();
        own_ = true;
    }
    shared_lock(Mtx& mtx_, std::defer_lock_t) noexcept : pmtx_(std::addressof(mtx_)), own_(false) {}
    shared_lock(Mtx& mtx_, std::adopt_lock_t) noexcept : pmtx_(std::addressof(mtx_)), own_(true) {}
    shared_lock(Mtx& mtx_, std::try_to_lock_t) noexcept : pmtx_(std::addressof(mtx_)), own_(pmtx_->try_lock_shared()) {}
    shared_lock(const shared_lock&) = delete;
    shared_lock(shared_lock&& other) noexcept : pmtx_(other.pmtx_), own_(other.own_) {
        other.pmtx_ = nullptr;
        other.own_ = false;
    }
    ~shared_lock() {
        if (pmtx_ && own_) pmtx_->unlock_shared();
    }
    auto operator= (const shared_lock&) ->shared_lock& = delete;
    auto operator= (shared_lock&& other) noexcept ->shared_lock& {
        if (this == std::addressof(other)) return *this;
        this->pmtx_ = other.pmtx_;
        this->own_ = other.own_;
        other.pmtx_ = nullptr;
        other.own_ = false;
        return *this;
    }
    auto lock() ->void {
        check_valid();
        pmtx_->lock_shared();
        own_ = true;
    }

    auto try_lock() ->bool {
        check_valid();
        own_ = pmtx_->try_lock_shared();
        return own_;
    }

    auto unlock() ->void {
        if (pmtx_ == nullptr || !own_) {
            throw std::system_error{std::make_error_code(std::errc::operation_not_permitted)};
        }
        pmtx_->unlock_shared();
        own_ = false;
    }

    explicit operator bool() noexcept {
        return own_;
    }
    auto owns_lock() noexcept ->bool {
        return own_;
    }
    auto mutex() noexcept ->mutex_type* {
        return pmtx_;
    }
    auto release() noexcept ->mutex_type* {
        auto ret_ = pmtx_;
        pmtx_ = nullptr;
        own_ = false;
        return ret_;
    }
private:
    auto check_valid() ->void {
        if (pmtx_ == nullptr) {
            throw std::system_error{std::make_error_code(std::errc::operation_not_permitted)};
        }

        if (own_) {
            throw std::system_error{std::make_error_code(std::errc::resource_deadlock_would_occur)};
        }
    }
    Mtx* pmtx_;
    bool own_;
};

}
