#pragma once
#include "Common.h"
#include <map>
#include <mutex>

namespace memoryPool {

// 基本单位：page，一个或多个page组成一块内存，由span结构体进行管理，包含相同page数的span组成链表。
// 根据span包含的page数不同，维护一个map，键为page数，值为span链表

class PageCache {
public:
    static const size_t PAGE_SIZE = 4096;
    static PageCache& getInstance() {
        static PageCache instance;
        return instance;
    }

    // 分配指定页数的span
    void* allocateSpan(size_t numPages);

    // 释放span
    void deallocateSpan(void* ptr, size_t numPages);

private:
    PageCache() = default;

    // 向系统申请内存
    void* systemAlloc(size_t numPages);

private:
    // 内存块, 管理多张页
    struct Span {
        void* pageAddr;   // 页起始地址
        size_t numPages;  // 页数
        Span* next;       // 链表指针
    };

    // 按页数管理空闲span，不同页数对应不同span链表
    std::map<size_t, Span*> freeSpans_;
    // 分配内存地址 pageAddr 到span的映射，用于回收
    std::map<void*, Span*> spanMap_;
    std::mutex mutex_;
};

}