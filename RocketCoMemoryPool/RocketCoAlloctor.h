#ifndef ROCKETCO_ALLOCTOR_H
#define ROCKETCO_ALLOCTOR_H

#include <stdlib.h>
#include <memory>
#include <vector>
#include <functional>
#include <set>

namespace RocketCo{

    // 每个线程存在一个RocketCoAlloctor，不考虑数据竞争，最大化效率。
    class RocketCoAlloctor {
        public:
            RocketCoAlloctor();

            // 仅仅允许默认构造;
            RocketCoAlloctor(const RocketCoAlloctor&) = delete;
            RocketCoAlloctor&  operator=(const RocketCoAlloctor&) = delete;

            RocketCoAlloctor(RocketCoAlloctor&&) = delete;
            RocketCoAlloctor&  operator=(RocketCoAlloctor&&) = delete;
            
            ~RocketCoAlloctor(){
                for(auto x : resource_pointers){
                    free(x);
                }
            }

        public:
            char* RocketCoMalloc(size_t length);
            void RocketCoFree(char* ptr, size_t len);

        private:
            static const constexpr size_t FirstSlot  = 8*1024;
            static const constexpr size_t SecondSlot = 32*1024;
            static const constexpr size_t ThirdSlot  = 128*1024;    // 默认最大128KB，再多就会直接调用malloc

            static size_t FindFreeListIndex(size_t length);         // 根据传入的块大小返回free_list的下标
            static size_t DataAlignment(size_t length);
            static size_t Index2SlotLength(size_t length);
            static size_t Data2Alignment(size_t length);            // 根据传入的length得到对应区间的对齐长度

            char* Refill(size_t length);
            char* ChunkAlloc(size_t length, int& number);
            void UseRemainingSpace(size_t head_index, size_t remaining);
            bool BeAssigned(char* ptr, size_t length);              // 判断此数据块是否已经被分配
            void MarkAssigned(char* ptr, size_t length);            // 给此数据块加上标记

        private:
            char* startfree;        // 当前正在使用的区间
            char* endfree;

            struct FreeListNode{
                FreeListNode* prev;
                FreeListNode* next; 
                size_t slot;        // 属于free_list的哪一个slot中
                int item_length;    // 此slot中每个项的长度，槽设计为不等长是为了减少内存碎片;选择存储而不是free时计算是因为这个内存反正是空闲的，不如记录x一下，省得free时计算
            };
            /*
             * 默认的栈大小为128KB，分配大于128KB的时候使用一般的malloc。
             * [0KB  - 8KB  ]  512
             * [9KB  - 32KB ]  1KB
             * [33KB - 128KB]  2KB
             */
            std::array<FreeListNode*, 96> free_list;                        // 用于存储小的内存块
            std::set<std::pair<ptrdiff_t, ptrdiff_t>> pointer_effective_interval;   // 用于合并时检测指针的上下区间是否有效
            std::vector<void*> resource_pointers;                           // 用于存储RocketCoAlloctoru分配的全部资源的指针，用于析构函数
            std::function<void(void*)> high_water_callback;                 // 用于ChunkAlloc中分配资源失败的时候调用，由用户i指定，默认什么也不做
    };

};

#endif //ROCKETCO_ALLOCTOR_H