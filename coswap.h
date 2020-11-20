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

#ifndef ROCKETCO_COSWAP_H
#define ROCKETCO_COSWAP_H

#include <stdlib.h> // size_t

namespace RocketCo {
    //using co_runningFunction = std::function<void*(void*, void*)>;
    typedef void* (*co_runningFunction)( void* s, void* s2 );
    // 需要在eip中赋予函数运行的其实地址,用std::function太麻烦,还得用target转一下,直接函数指针最方便
    // 这个function实际上是对协程实际执行函数的一个包装,为了在协程执行完以后切换执行权
    // 当然改为function就可以简单的支持闭包了;这样做
    /*
    std::function<void(void*)> fun(fun3);
    void (*const* ptr)(void*) = fun.target<void(*)(void*)>();
    (*ptr)(nullptr);

    // 此时两者的地址是一样的;
    printf("%p\n", *ptr);
    printf("%p\n", fun3);

    */

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