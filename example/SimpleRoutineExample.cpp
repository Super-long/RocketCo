//
// Created by lizhaolong on 2020/6/18.
//

#include <pthread.h>
#include <unistd.h>
#include <bits/stdc++.h>
#include "../Co_Routine.h"

// 一个简单的调用过程

struct task_t{
    RocketCo::Co_Entity* Co;
    int Rounine_id;
    int Thread_id;
};

void* Running(void* para){
    co_enable_hook_sys();
    task_t* arg = (task_t*)para;
    while(true){
        printf("This is No.%d thread. No.%d Routine.\n", arg->Thread_id, arg->Rounine_id);
        // only sleep.
        poll(nullptr, 0, 500);
    }
    return nullptr;
}

int loop(void*){
    return 0;
}

void* Routine(void* para){
    int index = *((int*)para);
    task_t args[10];
    memset(args, 0, sizeof(args));
    for (int i = 0; i < 10; ++i) {
        args[i].Rounine_id = i;
        args[i].Thread_id = index;
        RocketCo::Co_Create(&args[i].Co, nullptr, Running, args + i);
        RocketCo::Co_resume(args[i].Co);
    }
    RocketCo::EventLoop(RocketCo::GetCurrentCoEpoll(), loop, 0);
}

int main(){
    pthread_t tid[5];
    int args[5];
    for(int i = 0; i < 5; i++){
        args[i] = i;
        pthread_create( tid + i, NULL, Routine,args + i);
    }
    for(;;){
        sleep(1);
    }
    return 0;
}