#include "RocketCoAlloctor.h"
#include <assert.h>
#include <algorithm>

#include <bits/stdc++.h>
using namespace std;

namespace RocketCo{

    RocketCoAlloctor::RocketCoAlloctor() : high_water_callback([](void*){}){
        // 是否需要在构造的时候直接创建一大片内存呢
        startfree = nullptr;
        endfree = nullptr;
        // 初始化指针的值
        for (size_t i = 0; i < 96; i++){
            free_list[i] = nullptr;
        }
    }

    size_t RocketCoAlloctor::Index2SlotLength(size_t index){
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
    size_t RocketCoAlloctor::FindFreeListIndex(size_t index){
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
    size_t RocketCoAlloctor::DataAlignment(size_t index){
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

    size_t RocketCoAlloctor::Data2Alignment(size_t length){
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
     * @notes: BeAssigned的作用就是给标记数据块的最后一个字节，被标记的则视为已经被分配。MarkAssigned则为检查某个数据块是否被标记。
     * */
    bool RocketCoAlloctor::BeAssigned(char* ptr, size_t length){
        return *(ptr + (length - 1)) & 1;
    }
    
    
    void RocketCoAlloctor::MarkAssigned(char* ptr, size_t length){
        *(ptr + (length-1)) = CHAR_MAX;
    }


    char* RocketCoAlloctor::RocketCoMalloc(size_t index){
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
            // 出现的概率比较低，所以是否需要在上面搞一个临时变量呢
            char* res = Refill(CompleteData);
            return res;
        }

        free_list[DataIndex] = result->next;

        if(free_list[DataIndex] != nullptr){
            free_list[DataIndex]->prev = nullptr; // 头指针指向nullptr
        }

        return reinterpret_cast<char*>(result);
    }

    /*
     * @param: 第二个参数是必要的，不然无法判断放到哪一个空闲链表中。
     * @notes: 如果要释放的指针不在有效区间内的话就认为是malloc产生的，直接free就可以了。这样可能导致free一个不是RocketCoAlloctor分配的内存，这是用户的问题。
     * 还有一点，就是这个分配器其实是专属此项目的栈拷贝的，所以在分配以后会立即使用，这也是正确合并的前提。
     */
    void RocketCoAlloctor::RocketCoFree(char* ptr, size_t len){
        
        auto item = pointer_effective_interval.lower_bound(std::make_pair(ptr - (char*)0, __LONG_MAX__));
        if(item == pointer_effective_interval.end()){
            // 可能释放的是未分配的指针，此时为ub。其实可以维护一个哈希set，就可以知道这个指针是否是我们分配的了
            free(static_cast<void*>(ptr));
            return;
        } else if (item != pointer_effective_interval.begin()){ // 等于begin的话不需要操作
            --item;
        }
        // 此时得到一个ptr的所属空间，查看相邻的链表是否存在free_list中。
        // 因为可能相邻的数据块已经分配给别人，那么此时如何判断是否进行合并呢？我的选择是在FreeListNode结构体偏移以后100字节内不存在1则认为此结构体未被分配。显然是有风险的。

    }

    /*
     * @notes: 这个函数可能会抛错，注意
     */
    char* RocketCoAlloctor::Refill(size_t length){
        int number = 20; // 一次分配20个length长度的块
        char* chunk = ChunkAlloc(length, number);

        // 在malloc失败且高水位回调调用完成以后返回nullptr
        if(chunk == nullptr){
            cerr << "malloc error!\n";
            throw std::bad_alloc();
        }

        FreeListNode* next_obj, * current_obj;
        char* result;

        if(1 == number) {   // 目前存在的数据块仅能分配一个块
            return chunk;
        }

        cout << "number : " << number << endl;

        size_t refill_index = FindFreeListIndex(length);

        //cout << "refill_index : " << refill_index << endl;
        // 这条链表是空的，且我们已经得到了一块新的空间
        result = chunk;  // 需要返回给用户

        // 初始化链表起始点，并初始化next_obj
        size_t slot_length = Index2SlotLength(refill_index);    // 这是所属槽的数据大小，可能为512,1KB，2KB
        //cout << "slot_length: " << slot_length << endl;
        free_list[refill_index] = next_obj = (FreeListNode*)(chunk + slot_length);  // 把链表赋值给free_list

        next_obj->item_length = slot_length;
        next_obj->slot = refill_index;
        next_obj->prev = nullptr; // 链表头部prev为空
        next_obj->next = nullptr;
        
        for (size_t i = 1; i < number - 1 ; i++){
            current_obj = next_obj;
            next_obj = (FreeListNode*)((char*)next_obj + slot_length);

            current_obj->next = next_obj;

            next_obj->prev = current_obj;
            next_obj->next = nullptr;

            next_obj->slot = refill_index;
            next_obj->item_length = slot_length;
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
                //cout << "target : " << target << "------ Remaining : " << Remaining << endl;
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
    }

}
