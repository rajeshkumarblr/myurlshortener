#pragma once
#include <string>
#include <cstdint>

namespace Base62 {
    // Optimized base62 encoding - no heap allocation, builds right-to-left
    void encode(uint64_t value, char result[8], size_t width = 7);
    
    // Convenience function that returns a string
    std::string encodeToString(uint64_t value, size_t width = 7);
    
    // Optimized version for the common case of width=7
    std::string encodeFast7(uint64_t value);
}