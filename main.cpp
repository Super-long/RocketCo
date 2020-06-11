#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <queue>
#include "Co_Routine.h"
#include "hook.cpp"

using namespace std;

// 条件变量应用

struct TaskItem{
    int id;
};

struct TaskQueue{
    RocketCo::ConditionVariableLink* cond;
    std::queue<TaskItem*> Task_Queue;
};

void* Product(void* en){
    RocketCo::co_enable_hook_sys();
    TaskQueue* env = (TaskQueue*)en;
    int id = 0;
    while(true){
        TaskItem* task = new TaskItem();
        task->id++;
        env->Task_Queue.push(task);
        printf("%s:%d produce task %d\n", __func__, __LINE__, task->id);
        RocketCo::ConditionVariableSignal(env->cond);
        RocketCo::poll(nullptr, 0, 100); // 切换CPU执行
    }
    return nullptr;
}

void* Consumer(void * en){
    RocketCo::co_enable_hook_sys();
    TaskQueue* env = (TaskQueue*)en;
    while(true){
        if(env->Task_Queue.empty()){
            RocketCo::ConditionVariableWait(env->cond, -1);
            continue;
        }
        TaskItem* task = env->Task_Queue.front();
        env->Task_Queue.pop();
        printf("%s:%d consume task %d\n", __func__, __LINE__, task->id);
        delete task;
    }
    return nullptr;
}

int main(){
    TaskQueue* env = new TaskQueue();
    env->cond = RocketCo::ConditionVariableInit();
    RocketCo::Co_Entity* consumer;
    RocketCo::Co_Create(&consumer, nullptr, Consumer, env);
    RocketCo::Co_resume(consumer);

    RocketCo::Co_Entity* product;
    RocketCo::Co_Create(&product, nullptr, Product, env);
    RocketCo::Co_resume(product);

    RocketCo::EventLoop(RocketCo::GetCurrentCoEpoll(), nullptr, nullptr);
    return 0;
}