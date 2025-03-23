#include "ThreadCache.h"
#include "CentralCache.h"

namespace memoryPool {

void* ThreadCache::allocate(size_t size) {
    // 处理0大小的分配请求
    if(size == 0) {
        size = ALIGNMENT;
    }

    if(size > MAX_BYTES) {
        // 大对象直接从系统分配
        return malloc(size);
    }

    size_t index = SizeClass::getIndex(size);

    // 检查线程本地自由链表
    // 如果 freeList_[index] 不为空, 表示该链表中有可用的内存块
    if(void* ptr = freeList_[index]) {
        // 将freeList_[index] 指向内存块的下一个内存块地址
        freeList_[index] = *reinterpret_cast<void**>(ptr);
        freeListSize_[index]--;
        return ptr;
    }

    // 如果线程本地自由链表为空, 则从中心缓存获取一批内存
    return fetchFromCentralCache(index);
}

void ThreadCache::deallocate(void* ptr, size_t size) {
    if(size > MAX_BYTES) {
        free(ptr);
        return;
    }
    size_t index = SizeClass::getIndex(size);

    // 插入到线程本地自由链表
    *reinterpret_cast<void**>(ptr) = freeList_[index];
    freeList_[index] = ptr;

    // 更新自由链表大小
    freeListSize_[index]++;

    // 判断是否需要将部分内存回收给中心缓存
    if(shouldReturnToCentralCache(index)) {
        returnToCentralCache(freeList_[index], size);
    }
}

// 判断是否需要将内存回收给中心缓存
bool ThreadCache::shouldReturnToCentralCache(size_t index) {
    // 设定阈值
    size_t threshold = 64;
    return (freeListSize_[index] > threshold);
}

void* ThreadCache::fetchFromCentralCache(size_t index) {
    size_t size = (index + 1) * ALIGNMENT;
    // 根据对象内存大小计算批量获取的数量
    size_t batchNum = getBatchNum(size);
    // 从中心缓存批量获取内存
    void* start = CentralCache::getInstance().fetchRange(index, batchNum);
    if(!start) return nullptr;

    freeListSize_[index] += batchNum - 1;

    if(batchNum > 1) {
        freeList_[index] = *reinterpret_cast<void**>(start);
    }
    *reinterpret_cast<void**>(start) = nullptr;
    return start;

}

void ThreadCache::returnToCentralCache(void* start, size_t size) {
    size_t index = SizeClass::getIndex(size);
    size_t alignedSize = SizeClass::roundUp(size);

    size_t batchNum = freeListSize_[index];
    if(batchNum <= 1) return;

    size_t keepNum = std::max(batchNum / 4, size_t(1));
    size_t returnNum = batchNum - keepNum;

    void* splitNode = start;

    for(size_t i = 0; i < keepNum - 1; ++i) {
        splitNode = *reinterpret_cast<void**>(splitNode);
    }
    if(splitNode != nullptr) {
        void* nextNode = *reinterpret_cast<void**>(splitNode);
        *reinterpret_cast<void**>(splitNode) = nullptr;

        freeListSize_[index] = keepNum;
        if(returnNum > 0 && nextNode != nullptr) {
            CentralCache::getInstance().returnRange(nextNode, returnNum * alignedSize, index);
        }
    }
}


// 计算批量获取内存的数量
size_t ThreadCache::getBatchNum(size_t size) {
    // 基准：每次批量获取不超过4KB内存
    constexpr size_t MAX_BATCH_SIZE = 4 * 1024;
    // 根据对象大小设置合理的基准批量数
    size_t baseNum;
    if(size <= 32) baseNum = 64;
    else if(size <= 64) baseNum = 32;
    else if (size <= 128) baseNum = 16; // 16 * 128 = 2KB
    else if (size <= 256) baseNum = 8;  // 8 * 256 = 2KB
    else if (size <= 512) baseNum = 4;  // 4 * 512 = 2KB
    else if (size <= 1024) baseNum = 2; // 2 * 1024 = 2KB
    else baseNum = 1;

    // 最大批量数
    size_t maxNum = std::max(size_t(1), MAX_BATCH_SIZE / size);

    // 取最小值，但确保至少返回1
    return std::max(size_t(1), std::min(maxNum, baseNum));
}

}