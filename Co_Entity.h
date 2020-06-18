//
// Created by lizhaolong on 2020/6/8.
//

#ifndef ROCKETCO_CO_ENTITY_H
#define ROCKETCO_CO_ENTITY_H

#include "coswap.h"
#include "Co_Routine.h" //

namespace RocketCo {
    struct Co_Entity;

    struct Co_Stack_Member{     // 描述每一个协程的栈空间
        Co_Entity* current_co;  // 目前执行中使用这个栈的协程,因为支持共享栈,所以这个值会改变
        int stack_size;         // 当前栈上未使用的空间
        char* stack_bp;         // ebp = stack_buffer + stack_size
        char* stack_buffer;     // 栈的起始地址,当然对于主协程来说这是堆上的空间
    };

    struct Co_Entity{ // 协程实体
        struct Co_Rountinue_Env* env;  // 协程的执行环境,运行在同一个线程上的各协程是共享该结构,每个线程只有一个

        void* arg;              // 函数参数
        //Co_RealFun fun;       // 协程实际执行的函数,当然是经过包装的,因为在执行完成以后需要切换协程
        std::function<void*(void*)> fun;

        co_swap_t cst;          // 协程上下文实体

        bool ISstart;           // 协程现在是否执行resume
        bool IsEnd;             // 标记当前是否执行完了用户指定的函数
        bool IsShareStack;      // 标记共享栈模式还是每个协程一个栈
        bool IsMain;            // 是否是主协程
        bool IsHook;            // 此协程的函数是否被hook,这牵扯到执行hook后的函数还是之前的函数
        char filling[3];        // 字节对齐,还需要加什么状态位把这个删了就可以了

        void* Env;              // 环境变量,不把此结构设置成Sysenv*的原因是hook没有.h

        Co_Stack_Member* Csm;   // 协程的栈空间

        /* 当我们标记协程为共享栈的时候需要用到的数据结构,此时仅凭借Co_Stack_Member描述不了 */
        char* ESP;              // 执行此协程的esp指针
        char* Used_Stack;       // 保存未使用的栈内存
        size_t Used_Stack_size; // 栈中已使用的字节数 栈基址+Used_Stack==ESP

    };

}

#endif //ROCKETCO_CO_ENTITY_H