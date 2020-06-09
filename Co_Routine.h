//
// Created by lizhaolong on 2020/6/9.
//

#ifndef ROCKETCO_CO_ROUTINE_H
#define ROCKETCO_CO_ROUTINE_H

#include <functional>

namespace RocketCo{

    using Co_RealFun = std::function<void*(void*)>;

    struct Co_ShareStack;
    struct Co_Entity;
    struct Co_Rountinue_Env;

    struct Co_Attribute{
        static constexpr const int Default_StackSize = 128 * 1024;      // 128KB
        static constexpr const int Maximum_StackSize = 8 * 1024 * 1024; // 8MB
        int stack_size = Default_StackSize;              // 规定栈的大小
        Co_ShareStack* shareStack = nullptr;    // 默认为不共享
    };

    // 函数声明

    void Co_Create(Co_Entity** Co, Co_Attribute* attr, Co_RealFun fun, void* arg);
    void Co_resume(Co_Entity* Co);
    void Co_yeild(Co_Rountinue_Env* env);

}

#endif //ROCKETCO_CO_ROUTINE_H
