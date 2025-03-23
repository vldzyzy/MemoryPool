#include "MemoryPool.h"
#include <iostream>
#include <vector>
#include <thread>
#include <cassert>
#include <random>
#include <algorithm>
#include <atomic>
#include <stdio.h>
#include <string.h>

using namespace memoryPool;

// 基础分配测试
void testBasicAllocation() {
    std::cout << "Running basic allocation test..." << std::endl;

    // 测试小内存分配
    void* ptr1 = MemoryPool::allocate(8);
    assert(ptr1 != nullptr);
    MemoryPool::deallocate(ptr1, 8);

    // 测试中等大小内存分配
    void* ptr2 = MemoryPool::allocate(1024);
    assert(ptr2 != nullptr);
    MemoryPool::deallocate(ptr2, 1024);

    // 测试大内存分配
    void* ptr3 = MemoryPool::allocate(1024 * 1024);
    assert(ptr3 != nullptr);
    MemoryPool::deallocate(ptr3, 1024 * 1024);

    std::cout << "Basic allocation test passed!" << std::endl;
}

// 内存写入测试
void testMemoryWriting() {
    std::cout << "Running memory writing test..." << std::endl;

    // 分配并写入数据
    const size_t size = 128;
    char* ptr = static_cast<char*>(MemoryPool::allocate(size));
    assert(ptr != nullptr);

    // 写入数据
    for(size_t i = 0; i < size; ++i) {
        ptr[i] = static_cast<char>(i % 256);
    }

    // 验证数据
    for(size_t i = 0; i < size; ++i) {
        assert(ptr[i] == static_cast<char>(i % 256));
    }

    MemoryPool::deallocate(ptr, size);
    std::cout << "Memory writing test passed!" << std::endl;
}

// 多线程测试
void testMultiThreading() {
    std::cout << "Running multi-threading test..." << std::endl;
    
    constexpr int NUM_THREADS = 4;
    constexpr int ALLOCS_PER_THREAD = 1000;

    std::atomic<bool> has_error(false);

    auto threadFunc = [&has_error] () {
        try {
            std::vector<std::pair<void*, size_t>> allocations;
            allocations.reserve(ALLOCS_PER_THREAD);

            for(int i = 0; i < ALLOCS_PER_THREAD && !has_error; ++i) {
                size_t size = (rand() % 256 + 1) * 8;
                void* ptr = MemoryPool::allocate(size);
                if(!ptr) {
                    std::cerr << "Allocation failed for size: " << size << '\n';
                    has_error = true;
                    break;
                }
                allocations.push_back({ptr, size});

                // 50% 概率随机释放一个已分配内存
                if(rand() % 2 && !allocations.empty()) {
                    size_t index = rand() % allocations.size();
                    MemoryPool::deallocate(allocations[index].first, allocations[index].second);
                    allocations.erase(allocations.begin() + index);
                }
            }

            // 内存清理
            for(const auto& alloc : allocations) {
                MemoryPool::deallocate(alloc.first, alloc.second);
            }
        }
        catch (const std::exception& e) {
            std::cerr << "Thread exception: " << e.what() << std::endl;
            has_error = true;
        }
    };

    std::vector<std::thread> threads;
    for(int i = 0; i < NUM_THREADS; ++i) {
        threads.emplace_back(threadFunc);
    }
    for(auto& thread : threads) {
        thread.join();
    }
    std::cout << (has_error ? "Test failed!" : "Multi-threading test passed!") << std::endl;
}

// 边界测试
void testEdgeCases() {
    std::cout << "Running edge cases test..." << std::endl;

    // 测试0大小分配
    void* ptr1 = MemoryPool::allocate(0);
    assert(ptr1 != nullptr);
    MemoryPool::deallocate(ptr1, 0);

    // 测试最小对齐大小
    void* ptr2 = MemoryPool::allocate(1);
    assert(ptr2 != nullptr);
    assert((reinterpret_cast<uintptr_t>(ptr2) & (ALIGNMENT - 1)) == 0);
    MemoryPool::deallocate(ptr2, 1);

    // 测试最大边界
    void* ptr3 = MemoryPool::allocate(MAX_BYTES);
    assert(ptr3 != nullptr);
    MemoryPool::deallocate(ptr3, MAX_BYTES);

    // 测试超过最大
    void* ptr4 = MemoryPool::allocate(MAX_BYTES + 1);
    assert(ptr4 != nullptr);
    MemoryPool::deallocate(ptr4, MAX_BYTES + 1);

    std::cout << "Edge cases test passed!" << std::endl;
}

// 压力测试
void testStress() {
    std::cout << "Running stress tset..." << std::endl;

    constexpr int NUM_ITERATIONS = 10000;
    std::vector<std::pair<void*, size_t>> allocations;
    allocations.reserve(NUM_ITERATIONS);

    // 每次分配大小随机（8-8192字节，8字节对齐）
    for(int i = 0; i < NUM_ITERATIONS; ++i) {
        size_t size = (rand() % 1024 + 1) * 8;
        void* ptr = MemoryPool::allocate(size);
        assert(ptr != nullptr);
        allocations.push_back({ptr, size});
    }

    // 初始化随机数生成器
    std::random_device rd;
    std::mt19937 g(rd());

    // 洗牌算法打乱分配记录顺序（复杂度O(n)）
    std::shuffle(allocations.begin(), allocations.end(), g);

    // 按随机顺序释放所有内存块
    for(const auto& alloc : allocations) {
        MemoryPool::deallocate(alloc.first, alloc.second);
    }

    std::cout << "Stress test passed!" << std::endl;
}



// int main() 
// {
//     try 
//     {
//         std::cout << "Starting memory pool tests..." << std::endl;

//         testBasicAllocation();
//         testMemoryWriting();
//         testMultiThreading();
//         testEdgeCases();
//         testStress();

//         std::cout << "All tests passed successfully!" << std::endl;
//         return 0;
//     }
//     catch (const std::exception& e) 
//     {
//         std::cerr << "Test failed with exception: " << e.what() << std::endl;
//         return 1;
//     }
// }

// 只记录最近一次分配的内存块
static struct {
    void* addr;
    size_t size;
    unsigned char data[32]; // 记录前32字节
} last_block;

// 分配并记录内存
void* debug_alloc(size_t size) {
    void* ptr = MemoryPool::allocate(size);
    if (ptr) {
        last_block.addr = ptr;
        last_block.size = size;
        memcpy(last_block.data, ptr, size < 32 ? size : 32); // 保存数据快照
    }
    return ptr;
}

// 打印内存内容
void print_mem() {
    printf("地址: %p\n大小: %zu字节\n数据: ", last_block.addr, last_block.size);
    for (int i = 0; i < (last_block.size < 32 ? last_block.size : 32); i++) {
        printf("%02X ", last_block.data[i]); // 十六进制
    }
    printf("\n");
}

// 释放内存
void debug_free(void* ptr, size_t size) {
    MemoryPool::deallocate(ptr, size);
    memset(&last_block, 0, sizeof(last_block)); // 清空记录
}

int main() {
    // 分配32字节内存
    void* p = debug_alloc(32);
    
    // 写入字符串
    strcpy((char*)p, "Hello");
    
    // 查看内存内容
    print_mem(); // 输出：数据包含 "Hello" 的ASCII码
    
    // 释放
    debug_free(p, 32);
    return 0;
}