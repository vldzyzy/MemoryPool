#pragma once
#include "Common.h"

namespace memoryPool {

class ThreadCache {
public:
    // 单例模式, 每个线程一个实例
    static ThreadCache* getInstance() {
        static thread_local ThreadCache instance;
        return &instance;
    }

    void* allocate(size_t size);
    void deallocate(void* ptr, size_t size);

private:
    ThreadCache() {
        freeList_.fill(nullptr);
        freeListSize_.fill(0);
    };
    // 从中心缓存获取内存
    void* fetchFromCentralCache(size_t index);
    // 归还内存到中心缓存
    void returnToCentralCache(void* start, size_t size);

    size_t getBatchNum(size_t size);

    bool shouldReturnToCentralCache(size_t index);

private:
    // 每个线程的空闲链表数组
    std::array<void*, FREE_LIST_SIZE> freeList_;
    std::array<size_t, FREE_LIST_SIZE> freeListSize_;  // 空闲链表大小统计
};

}