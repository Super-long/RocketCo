//
// Created by lizhaolong on 2020/6/8.
//

#ifndef ROCKETCO_CO_ROUTINE_H
#define ROCKETCO_CO_ROUTINE_H

#include "Co_Entity.h" // 协程的实体
#include "EpollWrapper/epoll.h"

#include <string.h>

#include <chrono>
#include <functional>

namespace RocketCo {
    // -------------------  一些宏和别名

    using Co_RealFun = std::function<void*(void*)>;

    // -------------------  结构体声明

    struct CoEventItemIink;
    struct Co_Rountinue_Env;

    // -------------------  函数声明

    Co_Rountinue_Env* get_thisThread_Env();
    void init_thisThread_Env();

    // -------------------  全局变量

    static thread_local Co_Rountinue_Env* CurrentThread_CoEnv = nullptr;

    // -------------------

    // TODO 为什么需要多栈
    struct Co_ShareStack{
        unsigned int alloc_idx;		    // stack_array中我们在下一次调用中应该使用的那个共享栈的index
        int stack_size;				    // 共享栈的大小，这里的大小指的是一个stStackMem_t*的大小
        int count;					    // 共享栈的个数，共享栈可以为多个，所以以下为共享栈的数组
        Co_Stack_Member** stack_array;	// 栈的内容，这里是个数组，元素是stStackMem_t*
    };

    struct Co_Attribute{
        static constexpr const int Default_StackSize = 128 * 1024;      // 128KB
        static constexpr const int Maximum_StackSize = 8 * 1024 * 1024; // 8MB
        int stack_size = Default_StackSize;              // 规定栈的大小
        Co_ShareStack* shareStack = nullptr;    // 默认为不共享
    };

    struct CoEventItem{         // 用于在epoll中存储事件
        CoEventItem* Prev;      // 前一个元素
        CoEventItem* Next;      // 后一个元素

        CoEventItemIink* link;  // 此item所属链表
        //https://en.cppreference.com/w/cpp/chrono/time_point
        std::chrono::system_clock::time_point ExpireTime;   // 记录超时时间

        std::function<void(CoEventItem*, struct epoll_event&,CoEventItemIink*)> CoPrepare; //epoll中执行的云处理函数
        std::function<void(CoEventItem*)> CoPrecoss;        //epoll中执行的回调函数

        bool IsTimeout; // 标记这个事件是否已经超时
    };

    struct CoEventItemIink{
        CoEventItem *head;
        CoEventItem *tail;
    };

    #define MaximumNumberOfCoroutinesAllowed 256

    struct Co_Epoll{
        Epoll* epoll_t;       // 封装的一个epoll

        struct CoEventItemIink* ActiveLink;     // 正常的事件链表
        struct CoEventItemIink* TimeoutLink;    // 超时事件链表
    };

    struct Co_Rountinue_Env{
        Co_Entity* Co_CallBack[MaximumNumberOfCoroutinesAllowed];
        int Co_ESP; // 指向现在这个"调用栈"的栈顶

        Co_Epoll* Epoll_;

        // TODO 少一个时间轮
        // 对调用栈中协程实体的一个拷贝
        Co_Entity* using_Co;    // 正在使用的协程
        Co_Entity* prev_Co;     // 调用栈中的上一个协程
    };

    // 给协程实体分配内存并初始化
    // 线程局部的属性;协程属性;协程执行函数;函数参数;
    Co_Entity* CreatAndAlloction(Co_Rountinue_Env* env ,Co_Attribute* attr, Co_RealFun fun, void* arg){
        Co_Attribute Temp_attr; // 默认栈大小为128KB,且不支持共享栈
        if(attr){
            memcpy(&Temp_attr, attr, sizeof(Co_Attribute));
        } else {
            if(Temp_attr.stack_size <= 0){  // 小于零则为默认大小
                Temp_attr.stack_size = Co_Attribute::Default_StackSize;
            } else if (Temp_attr.stack_size > Co_Attribute::Maximum_StackSize){
                Temp_attr.stack_size = Co_Attribute::Maximum_StackSize;
            }
        }

        // 4KB对齐,也就是说如果对stack_size取余不为零的时候对齐为4KB
        // 例如本来5KB,经过了这里就变为8KB了
        if(attr->stack_size & 0xFFF){
            attr->stack_size &= ~0xFFF;
            attr->stack_size += 0x1000;
        }

        // 写成new的话和Co_Entity环状依赖了
        Co_Entity* Co = (Co_Entity*)malloc(sizeof(Co_Entity));
        bzero(Co, sizeof(Co_Entity));

        Co->env = env;
        Co->fun = fun;
        Co->arg = arg;

        Co_Stack_Member* Temp_StackMem = nullptr;
        if(attr->shareStack){   // 共享栈,栈需要用户指定
            int index = attr->shareStack->alloc_idx % attr->shareStack->count; // 得到此次使用的共享栈下标
            attr->shareStack->alloc_idx++;
            Temp_StackMem = attr->shareStack->stack_array[index]; // 栈的实体
            attr->stack_size = attr->shareStack->stack_size;    // 共享栈大小
        } else {                // 每一个协程一个栈,没有了协程切换的拷贝的开销,但是内存碎片可能很大
            Temp_StackMem = (Co_Stack_Member*)malloc(sizeof(Co_Stack_Member));

            Temp_StackMem->current_co = nullptr;    // TODO 没有用到这个元素
            Temp_StackMem->stack_size = attr->stack_size;
            Temp_StackMem->stack_buffer = (char*)malloc(attr->stack_size); // 分配专属的栈空间
            Temp_StackMem->stack_bp = Temp_StackMem->stack_buffer + Temp_StackMem->stack_size; // ebp
        }

        Co->Csm = Temp_StackMem;    //栈指针赋值
        Co->cst.ss_sp = Temp_StackMem->stack_buffer;    // 协程栈基址
        Co->cst.ss_size = Temp_StackMem->stack_size;    // 与前者相加为esp

        Co->ISstart = false;
        Co->IsMain = false;
        Co->IsShareStack = attr->shareStack != nullptr;

        // 共享栈相关,可以暂时不用初始化,在执行协程切换的时候进行赋值
        Co->Used_Stack = nullptr;

        return Co;
    }


    // 创建一个协程的主体 与pthread_create类似
    // 协程实体;协程属性;协程执行函数;函数参数;
    // TODO malloc改为new try catch一下
    void Co_Create(Co_Entity** Co, Co_Attribute* attr, Co_RealFun fun, void* arg){
        if(get_thisThread_Env() == nullptr){
            init_thisThread_Env();
        }

        Co_Entity* Temp = CreatAndAlloction(get_thisThread_Env(), attr, fun, arg);
        *Co = Temp;
    }

    Co_Rountinue_Env* get_thisThread_Env(){
        return CurrentThread_CoEnv;
    }

    // 一个线程只能被调用一次，但不是只能被调用一次，所以没有写成单例
    void init_thisThread_Env(){
        CurrentThread_CoEnv = (Co_Rountinue_Env*)calloc(1, sizeof(Co_Rountinue_Env));
        Co_Rountinue_Env* Temp = CurrentThread_CoEnv;

        Temp->Co_ESP = 0;   // "调用栈"顶指针

        Co_Entity* Main = CreatAndAlloction(Temp, nullptr, nullptr, nullptr);
        Main->IsMain = true;// 调用init_thisThread_Env的协程一定是主协程

        // TODO 感觉可有可无,可以在Co_Rountinue_Env写一个成员函数达到同样功能
        Temp->prev_Co = nullptr;
        Temp->using_Co = nullptr;

        co_swap_init(&Main->cst);   // 初始化此协程实体上下文
        Temp->Co_CallBack[Temp->Co_ESP++] = Main;   // 放入线程独有环境中

        // 初始化环境变量中的Epoll
        Temp->Epoll_->epoll_t = new Epoll();
        Temp->Epoll_->TimeoutLink = (CoEventItemIink*)malloc(sizeof(CoEventItemIink));
        Temp->Epoll_->ActiveLink  = (CoEventItemIink*)malloc(sizeof(CoEventItemIink));
    }
}

#endif //ROCKETCO_CO_ROUTINE_H
