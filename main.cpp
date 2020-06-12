#include <stdio.h>
#include <queue>
#include <iostream>
#include "Co_Routine.h"


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
    //cout << "进入Product\n";
    co_enable_hook_sys();
    TaskQueue* env = (TaskQueue*)en;
    int id = 0;
    while(true){
        TaskItem* task = new TaskItem();
        task->id = id++;
        env->Task_Queue.push(task);
        printf("%s:%d produce task %d\n", __func__, __LINE__, task->id);
        RocketCo::ConditionVariableSignal(env->cond);
        //printf("ConditionVariableSignal success.\n"); // active队列中应该已经有一个值了
        poll(nullptr, 0, 1000); // 切换CPU执行
        //printf("poll sucess.\n");
    }
    return nullptr;
}

void* Consumer(void * en){
    //cout << "进入Consumer\n";
    co_enable_hook_sys();
    TaskQueue* env = (TaskQueue*)en;
    while(true){
        //cout << "进入循环\n";
        if(env->Task_Queue.empty()){
            //cout << "ConditionVariableWait up\n";
            RocketCo::ConditionVariableWait(env->cond, -1);
            //cout << "ConditionVariableWait down\n";
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
    cout << "Start create routinue.\n";
    RocketCo::Co_Create(&consumer, nullptr, Consumer, env);
    cout << "consumer creat sucess. Start resume routine.\n";
    RocketCo::Co_resume(consumer);
    cout << "consumer resume sucess.\n";

    RocketCo::Co_Entity* product;
    RocketCo::Co_Create(&product, nullptr, Product, env);
    RocketCo::Co_resume(product);

    RocketCo::EventLoop(RocketCo::GetCurrentCoEpoll(), nullptr, nullptr);
    return 0;
}