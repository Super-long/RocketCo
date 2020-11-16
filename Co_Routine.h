//
// Created by lizhaolong on 2020/6/9.
//

#ifndef ROCKETCO_CO_ROUTINE_H
#define ROCKETCO_CO_ROUTINE_H

#include <functional>
#include <poll.h>
#include <vector>
#include "Co_Entity.h" // 协程的实体
#include <string>

namespace RocketCo{

    // 协程实际执行的函数类型
    using Poll_fun = std::function<int(struct pollfd fds[], nfds_t nfds, int timeout)>; // 用于Co_poll_inner

    using Co_RealFun = std::function<void*(void*)>;
    // 时间循环每一次循环后可能需要的函数,自选,可用作统计,终止主协程等作用
    //using Co_EventLoopFun = std::function<void*(void*)>;
    typedef int(*Co_EventLoopFun)(void*); // 需要判空,function需要转换一下,太麻烦

    // 多栈可以使得我们在进程切换的时候减少拷贝次数
    // 写成结构体+函数相比与class来说还有一个优点,就是不用把类的定义放在.h中
    struct Co_ShareStack{
        unsigned int alloc_idx;		    // stack_array中我们在下一次调用中应该使用的那个共享栈的index
        int stack_size;				    // 共享栈的大小，这里的大小指的是一个stStackMem_t*的大小
        int count;					    // 共享栈的个数，共享栈可以为多个，所以以下为共享栈的数组
        Co_Stack_Member** stack_array;	// 栈的内容，这里是个数组，元素是stStackMem_t*

        static constexpr const int DefaultStackSize = 128 * 1024;

        explicit Co_ShareStack(int Count, int Stack_size = DefaultStackSize);
        ~Co_ShareStack();
    };

    struct Co_Attribute{
        static constexpr const int Default_StackSize = 128 * 1024;      // 128KB
        static constexpr const int Maximum_StackSize = 8 * 1024 * 1024; // 8MB
        int stack_size = Default_StackSize;              // 规定栈的大小
        Co_ShareStack* shareStack = nullptr;    // 默认为不共享
    };

    struct Co_Rountinue_Env;
    struct Co_Entity;
    struct Co_Epoll;
    struct ConditionVariableLink;

    // 函数声明

    void Co_Create(Co_Entity** Co, Co_Attribute* attr, Co_RealFun fun, void* arg);
    void FreeCo_Entity(Co_Entity* Co);
    void Co_resume(Co_Entity* Co);
    void Co_yeild(Co_Rountinue_Env* env);
    void Co_yeild();

    Co_Entity* GetCurrentCoEntity();
    Co_Epoll* GetCurrentCoEpoll();
    bool GetCurrentCoIsHook();

    int Co_poll_inner(Co_Epoll* Epoll_, struct pollfd fds[], nfds_t nfds, int timeout, Poll_fun pollfunc);

    void EventLoop(Co_Epoll* Epoll_, const Co_EventLoopFun& fun, void* args);

    ConditionVariableLink* ConditionVariableInit();
    void ConditionVariableFree(ConditionVariableLink* CV);

    int ConditionVariableWait(ConditionVariableLink* CV, int TimeOut); 
    int ConditionVariableSignal(ConditionVariableLink* CV);
    int ConditionVariableBroadcast(ConditionVariableLink* CV);

}
// --------------------------- 特殊的函数
void InitEnv(std::vector<std::pair<std::string,std::string> >* para);
void co_enable_hook_sys();

#endif //ROCKETCO_CO_ROUTINE_H
