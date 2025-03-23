#pragma once
#include <cstddef>
#include <atomic>
#include <array>

namespace memoryPool {
    // 对齐数和大小定义
    constexpr size_t ALIGNMENT = 8;
    constexpr size_t MAX_BYTES = 256 * 1024;
    constexpr size_t FREE_LIST_SIZE = MAX_BYTES / ALIGNMENT;

    // 内存块头部信息
    struct BlockHeader {
        size_t size;
        bool inUse;
        BlockHeader* next;
    };

    // 大小类管理
    class SizeClass {
    public:
        // 内存对齐
        static size_t roundUp(size_t bytes) {
            return (bytes + ALIGNMENT - 1) & ~(ALIGNMENT - 1);
        }

        // 计算自由链表数组索引
        static size_t getIndex(size_t bytes) {
            // 确保bytes至少为ALIGNMENT
            bytes = std::max(bytes, ALIGNMENT);
            // 向上取整后 -1
            return (bytes + ALIGNMENT - 1) / ALIGNMENT - 1;
        }
    };
}