//
// Created by lizhaolong on 2020/6/9.
//

#ifndef ROCKETCO_CO_ROUTINE_H
#define ROCKETCO_CO_ROUTINE_H

#include <functional>
#include <poll.h>
#include "Co_Entity.h" // 协程的实体

namespace RocketCo{

    // 协程实际执行的函数类型
    using Co_RealFun = std::function<void*(void*)>;
    // 时间循环每一次循环后可能需要的函数,自选,可用作统计,终止主协程等作用
    //using Co_EventLoopFun = std::function<void*(void*)>;
    typedef void*(*Co_EventLoopFun)(void*); // 需要判空,function需要转换一下,太麻烦

    struct Co_ShareStack;
    struct Co_Entity;
    struct Co_Rountinue_Env;
    struct Co_Attribute;

    // 函数声明

    void Co_Create(Co_Entity** Co, Co_Attribute* attr, Co_RealFun fun, void* arg);
    void Co_resume(Co_Entity* Co);
    void Co_yeild(Co_Rountinue_Env* env);

    Co_Entity* GetCurrentCoEntity();
    Co_Epoll* GetCurrentCoEpoll();
    bool GetCurrentCoIsHook();

    int Co_poll_inner(Co_Epoll* Epoll_, struct pollfd fds[], nfds_t nfds, int timeout, Poll_fun pollfunc);

}

#endif //ROCKETCO_CO_ROUTINE_H
