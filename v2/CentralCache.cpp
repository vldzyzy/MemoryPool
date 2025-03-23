#include "CentralCache.h"
#include "PageCache.h"
#include <cassert>
#include <thread>

namespace memoryPool {

// 每次从PageCache获取的span有多少页
static constexpr size_t SPAN_PAGES = 8;

void* CentralCache::fetchRange(size_t index, size_t batchNum) {
    if(index >= FREE_LIST_SIZE || batchNum == 0) return nullptr;

    while(locks_[index].test_and_set(std::memory_order_acquire)) {}
    
    void* result = nullptr;
    try {
        result = centralFreeList_[index].load(std::memory_order_relaxed);
        // 空闲链表空了
        if(!result) {
            size_t size = (index + 1) * ALIGNMENT;
            result = fetchFromPageCache(size);

            if(!result) {
                locks_[index].clear(std::memory_order_release);
                return nullptr;
            }

            // 如果申请的内存比较小，小于页缓存返回的最小值SPAN_PAGES, 进行切分
            char* start = static_cast<char*>(result);
            size_t totalBlocks = (SPAN_PAGES * PageCache::PAGE_SIZE) / size;
            size_t allocBlocks = std::min(batchNum, totalBlocks);

            // 构建返回给ThreadCache的内存块链表
            if(allocBlocks > 1) {
                for(size_t i = 1; i < allocBlocks; ++i) {
                    void* current = start + (i - 1) * size;
                    void* next = start + i * size;
                    *reinterpret_cast<void**>(current) = next;
                }
                *reinterpret_cast<void**>(start + (allocBlocks - 1) * size) = nullptr;
            }

            // 构建保留在CentralCache的链表
            if(totalBlocks > allocBlocks) {
                void* remainStart = start + allocBlocks * size;
                for(size_t i = allocBlocks + 1; i < totalBlocks; ++i) {
                    void* current = start + (i - 1) * size;
                    void* next = start + i * size;
                    *reinterpret_cast<void**>(current) = next;
                }
                *reinterpret_cast<void**>(start + (totalBlocks - 1) * size) = nullptr;

                centralFreeList_[index].store(remainStart, std::memory_order_release);
            }
            
        }
        else {
            // 空闲链表中还有
            void* current = result;
            void* prev = nullptr;
            size_t count = 0;
            
            while(current && count < batchNum) {
                prev = current;
                current = *reinterpret_cast<void**>(current);
                count ++;
            }

            if(prev) {
                *reinterpret_cast<void**>(prev) = nullptr;
            }

            centralFreeList_[index].store(current, std::memory_order_release);
        }
    }
    catch (...) 
    {
        locks_[index].clear(std::memory_order_release);
        throw;
    }

    // 释放锁
    locks_[index].clear(std::memory_order_release);
    return result;
}

void CentralCache::returnRange(void* start, size_t size, size_t index) {
    if(!start || index >= FREE_LIST_SIZE) return;
    while(locks_[index].test_and_set(std::memory_order_acquire)) {};
    try {
        void* end = start;
        size_t count = 1;
        while(*reinterpret_cast<void**>(end) != nullptr && count < size) {
            end = *reinterpret_cast<void**>(end);
            count++;
        }

        void* current = centralFreeList_[index].load(std::memory_order_relaxed);
        *reinterpret_cast<void**>(end) = current;
        centralFreeList_[index].store(start, std::memory_order_release);
    }
    catch(...) {
        locks_[index].clear(std::memory_order_release);
        throw;
    }

    locks_[index].clear(std::memory_order_release);
}

void* CentralCache::fetchFromPageCache(size_t size) {
    // 1. 计算实际需要的页数
    size_t numPages = (size + PageCache::PAGE_SIZE - 1) / PageCache::PAGE_SIZE;
    // 2. 根据大小决定分配策略
    if(size <= SPAN_PAGES * PageCache::PAGE_SIZE) {
        return PageCache::getInstance().allocateSpan(SPAN_PAGES);
    }
    else {
        return PageCache::getInstance().allocateSpan(numPages);
    }
}

}  