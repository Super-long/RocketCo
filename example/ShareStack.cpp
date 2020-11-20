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
