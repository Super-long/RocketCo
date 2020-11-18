#include <bits/stdc++.h>
using namespace std;

#include "RocketCoAlloctor.h"

/*
 * 测试ChunkAlloc中的使用内存碎片。先请求3KB，再请求2.5KB，最后请求11KB
 */

int main(){
    RocketCo::RocketCoAlloctor alloctor;

    size_t length = 300;
    size_t loop = 20;

    string str(length, 'i');

/*     for (size_t i = 0; i < loop; i++){
        char* ptr1 = alloctor.RocketCoMalloc(length);
        cout << ptr1 << endl;
        memcpy(ptr1, str.c_str(), str.size());
        ptr1[length - 1] = '\0';
        cout << ptr1 << endl;
    }
    
    length = 900;

    for (size_t i = 0; i < loop; i++){
        char* ptr1 = alloctor.RocketCoMalloc(length);
        memcpy(ptr1, str.c_str(), str.size());
        ptr1[length - 1] = '\0';
        cout << ptr1 << endl;
    } */

    // 测试ChunkAlloc中的使用内存碎片。先请求3KB，再请求2.5KB，最后请求11KB,10KB就会放到a空闲链表中，此时去请求不必再分配20个
    auto ptr1 = alloctor.RocketCoMalloc(3 * 1024);
    auto ptr2 = alloctor.RocketCoMalloc(2 * 1024 + 512);
    auto ptr3 = alloctor.RocketCoMalloc(11 * 1024);
    auto ptr4 = alloctor.RocketCoMalloc(10 * 1024);

    return 0;
}