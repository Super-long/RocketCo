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
        poll(nullptr, 0, 1000);
        break; // TODO
    }
    return nullptr;
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
    RocketCo::EventLoop(RocketCo::GetCurrentCoEpoll(), [](void*){return -1;}, 0);

    for (int j = 0; j < 10; ++j) {
        RocketCo::FreeCo_Entity(args[j].Co);
    }
    return 0;
}

int main(){
    pthread_t tid[5];
    int args[5];
    for(int i = 0; i < 2; i++){
        args[i] = i;
        pthread_create( tid + i, NULL, Routine,args + i);
    }
    for(size_t i = 0; i < 2; i++){
        pthread_join(tid[i], nullptr);
    }
    //for(;;){
        sleep(1);
    //}
    return 0;
}