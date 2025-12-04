#include "utils.h"

namespace Base62 {
    std::string encodeFast7(uint64_t value) {
        const char chars[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
        std::string result(7, '0');
        
        int pos = 6;
        while (value > 0 && pos >= 0) {
            result[pos--] = chars[value % 62];
            value /= 62;
        }
        
        return result;
    }
}