#pragma once
#include <mutex>
#include <condition_variable>
#include <atomic>

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

}
