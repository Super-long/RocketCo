//
// Created by lizhaolong on 2020/6/8.
//

#ifndef ROCKETCO_COSWAP_H
#define ROCKETCO_COSWAP_H

#include <stdlib.h> // size_t

namespace RocketCo {
    //using co_runningFunction = std::function<void*(void*, void*)>;
    typedef void* (*co_runningFunction)( void* s, void* s2 );
    // 需要在eip中赋予函数运行的其实地址,用std::function太麻烦,还得用target转一下,直接函数指针最方便
    // 这个function实际上是对协程实际执行函数的一个包装,为了在协程执行完以后切换执行权

    struct co_swap_param_t{
        const void* p1;
        const void* p2;
    };

    struct co_swap_t // 一个协程的上下文实体
    {
    #if defined(__i386__)
        void *regs[ 8 ];
    #else
        void *regs[ 14 ];   // 上下文
    #endif // 保存上下文
        size_t ss_size;// 栈的大小
        char *ss_sp; // 栈顶指针esp
    };

    int co_swap_make(co_swap_t* cst, co_runningFunction pfn, const void* s, const void* s1);
    void co_swap_init(co_swap_t* cst);
}

#endif //ROCKETCO_COSWAP_H