//
// Created by lizhaolong on 2020/6/12.
//

#include "../Co_Routine.h"
#include <iostream>
#include <stdio.h>

void* RoutineFunc(void* args){
    co_enable_hook_sys();
    int* id = (int*)args;
    while(true){
        char sBuff[128] = "hello world!\n";
        printf("from routineid %d stack addr %p.\n", *id,sBuff);
        poll(nullptr, 0, 1000);
    }
    return nullptr;
}

int main(){
    RocketCo::Co_ShareStack* ShareStack = new RocketCo::Co_ShareStack(1, 1024*128);
    RocketCo::Co_Attribute env; 

    env.stack_size = 0;
    env.shareStack = ShareStack;

    RocketCo::Co_Entity* co[3];
    int id[3];
    for (int i = 0; i < 3; ++i) {
        id[i] = i;
        RocketCo::Co_Create(&co[i], &env, RoutineFunc, &id[i]);
        RocketCo::Co_resume(co[i]);
    }
    RocketCo::EventLoop(RocketCo::GetCurrentCoEpoll(), nullptr, nullptr);
    return 0;
}
