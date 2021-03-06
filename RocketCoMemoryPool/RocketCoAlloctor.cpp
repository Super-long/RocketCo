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

#include "RocketCoAlloctor.h"
#include <assert.h>
#include <algorithm>

#include <bits/stdc++.h>
using namespace std;

namespace RocketCo{


// Using GNU C __attribute__ -> http://unixwiz.net/techtips/gnu-c-attributes.html
// https://gcc.gnu.org/onlinedocs/gcc/Variable-Attributes.html
#ifndef __GNUC__

#define  __attribute__(x)  /*NOTHING*/
    
#endif

    RocketCoAlloctor::RocketCoAlloctor() : high_water_callback([](void*){}){
        // 是否需要在构造的时候直接创建一大片内存呢
        startfree = nullptr;
        endfree = nullptr;
        // 初始化指针的值
        for (size_t i = 0; i < 96; i++){
            free_list[i] = nullptr;
        }

        // 这里插入一个{0,0}是因为在RocketCoFree中的匹配算法需要这样，不然在pointer_effective_interval中匹配的值恰好为最后一个时会有问题;
        // 当然把pointer_effective_interval调整为负数也ok;
        pointer_effective_interval.insert(std::make_pair(LONG_MAX,LONG_MAX));
    }

    size_t __attribute__((pure)) RocketCoAlloctor::Index2SlotLength(size_t index){
        assert(index >= 0);

        ++index;
        
        if(index <= 16){
            return 512 * index;
        } else if (index <= 40){
            return FirstSlot + ((index-16) << 10);
        } else if (index <= 96){
            return SecondSlot + ((index-40) << 11);
        } else {
            exit(1);    // 不应该出现这种情况，退出是最正确的选择
        }
    }

    // 对齐以后找到所属的free_list链表下标
    size_t __attribute__((pure)) RocketCoAlloctor::FindFreeListIndex(size_t index){
        if(index <= FirstSlot){
            index >>= 9;        // 除以512
        } else if (index <= SecondSlot){
            index >>= 10;       // 第二阶段的槽大小为1KB
            index += 8;
        } else if (index <= ThirdSlot){
            index >>= 11;       // 第三阶段的槽大小为2KB   
            index += 24;
        }

        // 槽数以开始计算
        return index - 1;
    }

    // 必须在对齐以后使用才有意义
    size_t __attribute__((pure)) RocketCoAlloctor::DataAlignment(size_t index){
        if(index <= FirstSlot){
            if(index & 0b111111111){
                index &= ~0b111111111;
                index += 0b1000000000;
            }
        } else if(index <= SecondSlot){
            if(index & 0b1111111111){
                index &= ~0b1111111111;
                index += 0b10000000000;
            } 
        } else if(index <= ThirdSlot){
            if(index & 0b11111111111){
                index &= ~0b11111111111;
                index += 0b100000000000;
            } 
        }

        return index;
    }

    size_t __attribute__((pure)) RocketCoAlloctor::Data2Alignment(size_t length){
        assert(length < 0);

        if(length < FirstSlot){
            return 512;
        } else if (length < SecondSlot){
            return 1024;
        } else if (length < ThirdSlot){
            return 2048;
        } else {
            exit;
        }
    }

/*    
    // 错误的想法！
    // @notes: BeAssigned的作用就是给标记数据块的最后一个字节，被标记的则视为已经被分配。MarkAssigned则为检查某个数据块是否被标记。

    bool RocketCoAlloctor::BeAssigned(char* ptr, size_t length){
        return *(ptr + (length - 1)) & 1;
    }
    
    
    void RocketCoAlloctor::MarkAssigned(char* ptr, size_t length){
        *(ptr + (length-1)) = CHAR_MAX;
    } */

    // 函数返回时，函数返回的指针P不能别名任何其他有效指针，而且在P寻址的任何存储中都没有指向有效对象的指针。
    // 告诉编译器这个函数类似malloc，在大多数情况下不会返回null，使用此属性可以优化代码;
    char* __attribute__((malloc)) RocketCoAlloctor::RocketCoMalloc(size_t index){
        if(index > ThirdSlot){
            // TODO 可能分配失败，后面加上失败时的回调，free的时候需要查看有效区间，不存在的话free
            return static_cast<char*>(malloc(index));
        }

        FreeListNode* result;

        //my_free_list = free_list[FindFreeListIndex(DataAlignment(index))];
        size_t CompleteData = DataAlignment(index);

        size_t DataIndex = FindFreeListIndex(CompleteData);
        result = free_list[DataIndex];

        if(result == 0){
            char* res = Refill(CompleteData);
            return res;
        }

        free_list[DataIndex] = result->next;

        if(free_list[DataIndex] != nullptr){
            free_list[DataIndex]->prev = nullptr; // 头指针指向nullptr
        }

        char* pointer = reinterpret_cast<char*>(result);
        
        // 标记两个区间，这两个点一定是有效的;
        mark_allocated_block[pointer] = CompleteData;
        mark_allocated_block[pointer + (CompleteData - 1)] = CompleteData;

        return pointer;
    }

    /*
     * @param: 第二个参数是必要的，不然无法判断放到哪一个空闲链表中。
     * @notes: 如果要释放的指针不在有效区间内的话就认为是malloc产生的，直接free就可以了。这样可能导致free一个不是RocketCoAlloctor分配的内存，这是用户的问题。
     * 还有一点，就是这个分配器其实是专属此项目的栈拷贝的，所以在分配以后会立即使用，这也是正确合并的前提。
     * nonnull -> 参数为空指针发出警告
     */
    void __attribute__((nonnull (1))) RocketCoAlloctor::RocketCoFree(char* ptr, size_t len){
        // 用户使用分配时的大小就可以了
        len = DataAlignment(len);

        auto item = pointer_effective_interval.lower_bound(std::make_pair(ptr - (char*)0, __LONG_MAX__));
        if(item == pointer_effective_interval.end()){
            // 可能释放的是未分配的指针，此时为ub。其实可以维护一个哈希set，就可以知道这个指针是否是我们分配的了;
            // cout << "error in RocketCoFree, out of free range\n";
            free(static_cast<void*>(ptr));
            return;
        } else if (item != pointer_effective_interval.begin()){ // 等于begin的话不需要操作
            --item;
        }
        
        // 此时已经没有什么用了
        mark_allocated_block.erase(ptr);
        mark_allocated_block.erase(ptr + (len - 1));

        // 此时得到一个ptr的合并空间区间，查看相邻的链表是否存在free_list中。
        // 因为可能相邻的数据块已经分配给别人，那么此时如何判断是否进行合并呢？我的选择是维护一个哈希表，以此保证绝对的安全。
        
        // 目前只支持合并两个块，因为太懒了;
        bool flag = false;

        // 首先合并低地址
        if(ptr - (char*)0 != item->first) {
            if(mark_allocated_block.find(ptr - 1) == mark_allocated_block.end()) { // 未被分配，直接在free_list中查找即可
                // 显然block后面的结构体只需要长度; 
                FreeListNode* node = reinterpret_cast<FreeListNode*>(ptr - sizeof(FreeListNode));
                flag = MergeData(ptr - node->item_length, node->item_length, ptr, len, LeftNeedModified);
/*                 if(!flag){
                    cerr << "Merge data error.\n";
                } */
            } // 已经被分配
        }// else{} // 左区间不执行合并，什么也不做
        
        // 再合并高地址
        if(!flag && ptr + (len - 1) - (char*)0 != item->second){ // 显然不可能大于item->second
            // 上面判断了ptr的右区间不等于全部数据的右区间，那么它们之间一定有空隙
            if(mark_allocated_block.find(ptr + len) == mark_allocated_block.end()){ // 且后面block没有被分配
                // 下一位的起始地址上就是FreeListNode;
                FreeListNode* node = reinterpret_cast<FreeListNode*>(ptr + len);
                flag = MergeData(ptr, len, ptr + len, node->item_length, RightNeedModified);
            }
        }

        // 前后都无法合并，就把自己放到free_list中
        if(!flag){
            SetBlock2FreeList(reinterpret_cast<FreeListNode*>(ptr), len);
        }
    }

    bool __attribute__((pure)) RocketCoAlloctor::IsMerge(size_t len){
        if(len < 0 && len > ThirdSlot){
            return false;
        } else if (len < FirstSlot){
            return !(len&511);
        } else if (len < SecondSlot){
            return !(len&1023);
        } else if (len < ThirdSlot) {
            return !(len&2047);
        } else {
            // 不可能出现的情况
            return false;
        }
    } 

    /*
     * @notes: 把两个block合并以后放到free_list中;值得注意的是可能合并这两个无法合并放到free_list的一项里面
     * @param: 当flag为mergeright的时候必须把;需要保证lhs地址低于rhs
     */
    bool RocketCoAlloctor::MergeData(char* lhs, size_t lenA, char* rhs, size_t lenB, LeftOrRightMod flag){
        size_t total_len = lenA + lenB;
        // step1：验证合并后是否可以放到一个链表中
        if(!IsMerge(total_len)){
            return false;
        }

        // step2: 把其中一个要执行合并的数据块从原链表中去除
        if(flag == LeftNeedModified){
            MoveToFreeList(reinterpret_cast<FreeListNode*>(lhs), lenA);
        } else if (flag == RightNeedModified){
            MoveToFreeList(reinterpret_cast<FreeListNode*>(rhs), lenB);
        } else {
            // 不可能的情况
            std::cerr << "Error in MergeData.\n";
            exit(1);
        }

        // step3：放置到free_list中
        SetBlock2FreeList(reinterpret_cast<FreeListNode*>(lhs), total_len);

        return true;
    }

    /*
     * @notes: 把ptr从所属的free_list中移除;
     */
    void __attribute__((hot)) RocketCoAlloctor::MoveToFreeList(FreeListNode* ptr, size_t len){
        size_t index = FindFreeListIndex(len);

        if(ptr->prev == nullptr){ // 本身是头节点
            free_list[index] = ptr->next;
            if(free_list[index] != nullptr){
                // 要使用prev为空判断头节点
                free_list[index]->prev = nullptr;
            }
        } else {
            ptr->prev->next = ptr->next;
            ptr->next->prev = ptr->prev;
        }
    }

    /*
     * @notes: 只有len，不需要对齐;头插;
     */
    void RocketCoAlloctor::SetBlock2FreeList(FreeListNode* ptr, size_t len){

        size_t index = FindFreeListIndex(len);

        FreeListNode* my_free_list = free_list[index];
        ptr->prev = nullptr;
        ptr->next = my_free_list;
        if(my_free_list != nullptr){

            my_free_list->prev = ptr;
        }
        free_list[index] = ptr;
        ptr->slot = index;
        ptr->item_length = len;

        MarkBlock(reinterpret_cast<char*>(ptr), len);
    }

    // https://docs.microsoft.com/en-us/cpp/cpp/fastcall?view=msvc-160
    // 性能分析中调用最多的函数
    void __attribute__((__fastcall)) __attribute__((hot)) RocketCoAlloctor::MarkBlock(char* ptr, size_t len){
        FreeListNode* node = reinterpret_cast<FreeListNode*>(ptr + (len - sizeof(FreeListNode)));
        node->item_length = len;
    }     

    /*
     * @notes: 这个函数可能会抛错，注意
     */
    char* RocketCoAlloctor::Refill(size_t length){
        int number = 20; // 一次分配20个length长度的块
        char* chunk = ChunkAlloc(length, number);

        // 在malloc失败且高水位回调调用完成以后返回nullptr
        if(chunk == nullptr) {
            cerr << "malloc error!\n";
            throw std::bad_alloc();
        }

        FreeListNode* next_obj, * current_obj;
        char* result;

        if(1 == number) {   // 目前存在的数据块仅能分配一个块
            return chunk;
        }

        size_t refill_index = FindFreeListIndex(length);

        // 这条链表是空的，且我们已经得到了一块新的空间
        result = chunk;  // 需要返回给用户

        // 初始化链表起始点，并初始化next_obj

        free_list[refill_index] = next_obj = (FreeListNode*)(chunk + length);  // 把链表赋值给free_list

        next_obj->item_length = length;
        next_obj->slot = refill_index;
        next_obj->prev = nullptr; // 链表头部prev为空
        next_obj->next = nullptr;

        MarkBlock(reinterpret_cast<char*>(next_obj), length);
        
        for (size_t i = 1; i < number - 1 ; i++){
            current_obj = next_obj;
            next_obj = (FreeListNode*)((char*)next_obj + length);

            current_obj->next = next_obj;

            next_obj->prev = current_obj;
            next_obj->next = nullptr;

            next_obj->slot = refill_index;
            next_obj->item_length = length;
            MarkBlock(reinterpret_cast<char*>(next_obj), length);
        }
        
        return result;
    }

    // 也可能返回nullptr，调用方需要检测
    char* RocketCoAlloctor::ChunkAlloc(size_t length, int& number){
        char* result = nullptr;
        size_t total_bytes = length * number;   // 需要分配的内存总数
        ptrdiff_t Remaining = endfree - startfree;

        if(Remaining > total_bytes) {
            result = startfree;
            std::advance(startfree, total_bytes);
            return result;
        } else if (Remaining > length) {// 不够分配number个，但是可以分配大于等于1个
            number = Remaining / length;    // 最大分配的个数
            result = startfree;
            std::advance(startfree, length * number);
            return result;
        } else {
            // 重新分配内存
            size_t Bytes_to_get = 2 * total_bytes;
            if(Remaining > 0){  // 利用剩余的内存
                // 空余的空间最小为512字节，所以这里一定可以释放一片空间，也可能需要释放两片 TODO 需要修改 把全部的碎片都利用到
                size_t target = DataAlignment(Remaining);
                size_t interval = target - Remaining;

                size_t list_index = INT64_MAX;
                // 只有这两种小碎片
                if(interval == 512){
                    list_index = 0;
                } else if(interval == 1024){
                    list_index = 1;
                }
                Remaining -= interval;
                size_t head_index = FindFreeListIndex(Remaining);

                UseRemainingSpace(head_index, Remaining);
                std::advance(startfree, Remaining); // 后面可能存在小碎片


                // TODO 代码可以复用
                if(list_index != INT64_MAX){    // 证明还有一个小碎片
                    UseRemainingSpace(list_index, list_index == 0 ? 512 : 1024);
                }
            }
            // 这里的策略是否可以修改，即先从更大的free链表查找，找不到再分配
            startfree = static_cast<char*>(malloc(Bytes_to_get));
            resource_pointers.push_back(startfree);
            pointer_effective_interval.emplace(std::make_pair(startfree - (char*)0, (startfree + Bytes_to_get) - (char*)0));  // 用于链表合并

            if(nullptr == startfree){ // 分配失败，从更大的free链表中查找;
                // 这里的length已经对齐过了;第FindFreeListIndex(length)不存在才请求重新分配的
                for (size_t i = FindFreeListIndex(length) + 1; i < 96; i++){
                    FreeListNode* my_free_list = free_list[i];  // 得到一条空闲链表
                    if(nullptr != my_free_list){
                        free_list[i] = my_free_list->next;
                        startfree = (char*)(my_free_list);
                        endfree = startfree + Index2SlotLength(i);
                        return ChunkAlloc(length, number);
                    }
                }
                endfree = nullptr;
                high_water_callback(&Bytes_to_get);  // 默认的高水位回调没有什么意义
                // 调用方检测到nullptr的时候会抛错
                return nullptr;
            } else {
                endfree = startfree + Bytes_to_get;
                return ChunkAlloc(length, number);
            }
        }
    }

    void RocketCoAlloctor::UseRemainingSpace(size_t head_index, size_t remaining){
        FreeListNode* my_free_list = free_list[head_index];
        FreeListNode* head = (FreeListNode*)startfree;
        head->prev = nullptr;
        head->next = my_free_list;
        if(my_free_list != nullptr){    // 可能第一次插入
            my_free_list->prev = head;
        }
        free_list[head_index] = head;   // 更新头指针

        head->slot = head_index;
        head->item_length = remaining;

        MarkBlock(reinterpret_cast<char*>(head), remaining);
    }

}
