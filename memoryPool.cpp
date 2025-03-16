#include "memoryPool.h"

namespace memoryPool{
MemoryPool::MemoryPool(size_t BlockSize)
    :BlockSize_(BlockSize), SlotSize_(0), firstBlock_(nullptr)
    , curSlot_(nullptr), freeList_(nullptr), lastSlot_(nullptr)
    {}

MemoryPool::~MemoryPool() {
    // 把连续的block删除
    Slot* cur = firstBlock_;
    while(cur) {
        Slot* next = cur->next;
        // 转化为 void 指针，因为 void 类型不需要调用析构函数，只释放空间
        operator delete(reinterpret_cast<void*>(cur));
        cur = next;
    }
}

void MemoryPool::init(size_t slotSize) {
    assert(slotSize > 0);
    SlotSize_ = slotSize;
    firstBlock_ = nullptr;
    curSlot_ = nullptr;
    freeList_ = nullptr;
    lastSlot_ = nullptr;
}

void* MemoryPool::allocate() {
    // 优先使用空闲链表中的内存槽
    if(freeList_ != nullptr) {
        {
            std::lock_guard<std::mutex> lock(mutexForFreeList_);
            if(freeList_ != nullptr) {
                Slot* tmp = freeList_;
                freeList_ = freeList_->next;
                return tmp;
            }
        }
    }

    Slot* tmp;
    {
        std::lock_guard<std::mutex> lock(mutexForBlock_);
        if(curSlot_ >= lastSlot_) {
            // 当前内存块已无内存槽可用，开辟一块新的内存块
            allocateNewBlock();
        }

        tmp = curSlot_;
        curSlot_ += SlotSize_ / sizeof(Slot);  // 指针加1 -> 下8个字节。
    }
    return tmp;
}

void MemoryPool::deallocate(void* ptr) {
    if(ptr) {
        // 回收内存，将内存通过头插法插入到空闲链表中
        std::lock_guard<std::mutex> lock(mutexForFreeList_);
        reinterpret_cast<Slot*>(ptr)->next = freeList_;
        freeList_ = reinterpret_cast<Slot*>(ptr);
    }
}

void MemoryPool::allocateNewBlock() {
    // 头插法插入新的内存block
    void* newBlock = operator new(BlockSize_);
    reinterpret_cast<Slot*>(newBlock)->next = firstBlock_;
    firstBlock_ = reinterpret_cast<Slot*>(newBlock);

    // 分配slot
    char* body = reinterpret_cast<char*>(newBlock) + sizeof(Slot*);
    size_t paddingSize = padPointer(body, SlotSize_);   // 计算对齐需要填充内存的大小
    
    // 从这里才开始分配给slot, 第一个槽大小的内存放一个slot结构体，用于指向下一个block
    curSlot_ = reinterpret_cast<Slot*>(body + paddingSize);

    // 计算最后一个slot的地址
    lastSlot_ = reinterpret_cast<Slot*>(reinterpret_cast<size_t>(newBlock) + BlockSize_ - SlotSize_ + 1);
    // 重置空闲链表
    freeList_ = nullptr;
}

// 让指针对齐到槽大小的倍数位置
size_t MemoryPool::padPointer(char* p, size_t align) {
    // align 是槽大小
    // 返回从p到下一个align倍数所需的字节数
    return (align - reinterpret_cast<size_t>(p)) % align;
}

void HashBucket::initMemoryPool() {
    for(int i = 0; i < MEMORY_POOL_NUM; ++i) {
        getMemoryPool(i).init((i + 1) * SLOT_BASE_SIZE);
    }
}

// 单例模式
MemoryPool& HashBucket::getMemoryPool(int index) {
    static MemoryPool memoryPool[MEMORY_POOL_NUM];
    return memoryPool[index];
}

} // namespace memoryPool