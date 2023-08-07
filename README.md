# shared_mutex
Implement for `std::shared_mutex` (minimum required c++ standard: C++11)  
实现`std::shared_mutex` (要求的最低c++标准: C++11)  
The usage is the same as `std::shared_mutex`  
使用方式与`std::shared_mutex`一样  
```cpp
#include "shared_mutex.hpp"

auto main() ->int {
    ccat::writer_first_shared_mutex wfsmtx; // 独占模式优先 | Exclusive mode takes precedence
    ccat::reader_first_shared_mutex rfsmtx; // 共享模式优先 | Shared mode takes precedence
    // ccat::shared_mutex是ccat::writer_first_shared_mutex的类型别名 | `ccat::shared_mutex` is a type alias of `ccat::writer_first_shared_mutex`
    return 0;
}
```
