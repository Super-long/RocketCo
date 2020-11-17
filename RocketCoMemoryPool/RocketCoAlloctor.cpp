#include "RocketCoAlloctor.h"

namespace RocketCo{

    RocketCoAlloctor::RocketCoAlloctor(){
        startfree = nullptr;
        endfree = nullptr;
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

    char* RocketCoAlloctor::RocketCoMalloc(size_t index){
        if(index > ThirdSlot){
            // TODO 可能分配失败，后面加上失败时的回调
            return static_cast<char*>(malloc(index));
        }

        FreeListNode* my_free_list;
        FreeListNode* result;

        my_free_list = free_list[FindFreeListIndex(DataAlignment(index))];
        result = my_free_list;
        if(result == nullptr){
            // 出现的概率比较低，所以不需要在上面搞一个临时变量
            char* res = Refill(DataAlignment(index));
            return res;
        }
        my_free_list = result->next;
        return reinterpret_cast<char*>(result);
    }

    void RocketCoAlloctor::RocketCoFree(char* ptr){

    }

    char* RocketCoAlloctor::Refill(size_t length){
        int number = 20; // 一次分配20个length长度的块
        char* chunk = ChunkAlloc(length, number);

        FreeListNode* my_free_list;
        FreeListNode* result;

        if(1 == number) {   // 目前存在的数据块仅能分配一个块
            return chunk;
        }


    }

    char* RocketCoAlloctor::ChunkAlloc(size_t length, int& number){
        char* result = nullptr;
        size_t total_bytes = length * number;   // 需要分配的内存总数
        size_t Remaining = std::distance(startfree, endfree);

        if(Remaining > total_bytes) {
            result = static_cast<char*>(startfree);
            std::advance(startfree, total_bytes);
            return result;
        } else if (Remaining > length) {// 不够分配number个，但是可以分配大于1个
            number = Remaining / length;    // 最大分配的个数
            total_bytes = length * number; 
            result = static_cast<char*>(startfree);
            std::advance(startfree, total_bytes);
            return result;
        } else {
            size_t Bytes_to_get = 2 * total_bytes;
            if(Remaining > 0){  // 利用剩余的内存
                // 空余的空间最小为512字节，所以这里一定可以释放一片空间，也可能需要释放两片 TODO 需要修改 把全部的碎片都利用到
                FreeListNode* my_free_list = free_list[FindFreeListIndex(DataAlignment(Remaining) - 1)];
                FreeListNode* head = (FreeListNode*)startfree;
                head->prev = nullptr;
                head->next = my_free_list;
                my_free_list->prev = head;
            }
            // 这里的策略是否可以修改，即先从更大的free链表查找，找不到再分配
            startfree = static_cast<char*>(malloc(Bytes_to_get));
            if(nullptr == startfree){ // 分配失败，从更大的free链表中查找;
                // 这里的length已经对齐过了;第FindFreeListIndex(length)不存在才请求重新分配的
                for (size_t i = FindFreeListIndex(length) + 1; i < 96; i++){
                    
                }
                
            }
        }
    }

}
