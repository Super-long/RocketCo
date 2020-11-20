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
#include <bits/stdc++.h>
#include "../Co_Routine.h"

using namespace std;

// {name, value}, value为""的时候就是没有值,当然是有bug的,如果要存储的值就是""就出现问题了
vector<pair<string, string>> env = {
        {"hello", "world"},
        {"lizhaolong", ""}
};

void* usingSev(void* para){
    co_enable_hook_sys();
    int* arg = (int*)para;
    cout << "Routine " << *arg << " is running!\n";
    poll(nullptr, 0, 500);
    string str = "Routine ";
    str += to_string(*arg);
    int ret = setenv("hello", str.data(), 1);
    if(ret){
        std::cerr << "ERROR : error in setenv, " << strerror(errno) << std::endl;
        return nullptr;
    }
    cout << "Routine " << *arg << " value is \" " << str << "\""<< std::endl;
    poll(nullptr, 0, 500);
    char* env = getenv("hello");
    if(!env){
        std::cerr << "ERROR : error in getenv, " << strerror(errno) << std::endl;
    }
    cout << env << endl;
    return nullptr;
}

int main(){
    InitEnv(&env);
    int args[3] = {0};
    for (int i = 0; i < 3; ++i) {
        args[i] = i;
        RocketCo::Co_Entity* Co;
        RocketCo::Co_Create(&Co, nullptr, usingSev, &args[i]);
        RocketCo::Co_resume(Co);
    }
    RocketCo::EventLoop(RocketCo::GetCurrentCoEpoll(),0,0 );
    return 0;
}