#ifndef ROCKETCO_ALLOCTOR_H
#define ROCKETCO_ALLOCTOR_H

/**
 * Copyright lizhaolong(https://github.com/Super-long)
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/* Code comment are all encoded in UTF-8.*/

#include <stdlib.h>
#include <memory>
#include <vector>
#include <unordered_map>
#include <functional>
#include <set>

namespace RocketCo{

    // 每个线程存在一个RocketCoAlloctor，不考虑数据竞争，最大化效率。
    class RocketCoAlloctor {
        public:
            // 仅仅允许默认构造;
            RocketCoAlloctor();

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
            enum LeftOrRightMod{LeftNeedModified = 0, RightNeedModified};

            static const constexpr size_t FirstSlot  = 8*1024;
            static const constexpr size_t SecondSlot = 32*1024;
            static const constexpr size_t ThirdSlot  = 128*1024;    // 默认最大128KB，再多就会直接调用malloc

            static size_t FindFreeListIndex(size_t length);         // 根据传入的块大小返回free_list的下标
            static size_t DataAlignment(size_t length);
            static size_t Index2SlotLength(size_t length);
            static size_t Data2Alignment(size_t length);            // 根据传入的length得到对应区间的对齐长度
            static bool IsMerge(size_t len);                               // 判断合并后的block大小是否可以放入free_list

            struct FreeListNode;

            char* Refill(size_t length);
            char* ChunkAlloc(size_t length, int& number);
            void UseRemainingSpace(size_t head_index, size_t remaining);
/*             bool BeAssigned(char* ptr, size_t length);                   // 判断此数据块是否已经被分配
            void MarkAssigned(char* ptr, size_t length);                    // 给此数据块加上标记 */
            bool MergeData(char* lhs, size_t lenA, char* rhs, size_t lenB, LeftOrRightMod flag); // 用于合并两个数据块，保证传入的lhs起始地址小于lhs
            void MarkBlock(char* ptr, size_t len);                  // 用于标记block的后面sizeof(FreeListNode)个字节，用于与前半段合并
            void SetBlock2FreeList(FreeListNode* ptr, size_t len);          // 把起始地址ptr，长度为len的block放置到free_list中
            void MoveToFreeList(FreeListNode* ptr, size_t len);

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
            std::unordered_map<char*, size_t> mark_allocated_block;         // 用于在分配一个数据块以后标记其已经被分配与存储这个指针所代表的数据大小;百般思考下选择了这样写
    };

};

#endif //ROCKETCO_ALLOCTOR_H