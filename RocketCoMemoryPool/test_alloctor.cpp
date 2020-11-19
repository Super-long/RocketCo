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
/*     auto ptr1 = alloctor.RocketCoMalloc(3 * 1024);
    alloctor.RocketCoFree(ptr1, 3*1024);
    auto ptr2 = alloctor.RocketCoMalloc(2 * 1024 + 512);
    auto ptr3 = alloctor.RocketCoMalloc(11 * 1024);
    auto ptr4 = alloctor.RocketCoMalloc(10 * 1024); */

    // 测试free先请求3KB，然后free3KB，此时会合并一个6KB的block
/*     auto ptr1 = alloctor.RocketCoMalloc(3 * 1024);
    alloctor.RocketCoFree(ptr1, 3*1024);    // 此时应该6KB链表上有一个值
    auto ptr2 = alloctor.RocketCoMalloc(6 * 1024);
    alloctor.RocketCoFree(ptr2, 6*1024);
 */

    for(int i = 0; i < 10; ++i){
        cout << i << endl;
        auto ptr1 = alloctor.RocketCoMalloc(3 * 1024);
        alloctor.RocketCoFree(ptr1, 3*1024); 
    }

    for (size_t i = 0; i < 5; i++){
        cout << i << endl;
        auto ptr1 = alloctor.RocketCoMalloc(6 * 1024);
        alloctor.RocketCoFree(ptr1, 6*1024); 
    }
    

    auto start = std::chrono::high_resolution_clock::now(); 

/*     for (size_t i = 0; i < 100000; i++){
        //cout << i << endl;
        auto ptr1 = alloctor.RocketCoMalloc(3 * 1024);
        alloctor.RocketCoFree(ptr1, 3*1024); 

        //auto ptr2 = malloc(3 * 1024);
        //free(ptr2);
    } */
    

    auto end = std::chrono::high_resolution_clock::now();

    std::chrono::duration<double, std::ratio<1,1000>> time_span 
    = std::chrono::duration_cast<std::chrono::duration<double, std::ratio<1,1000>>>(end - start);

    std::cout << time_span.count() << std::endl;
    return 0;
}