//
// Created by lizhaolong on 2020/6/8.
//

#include "Co_Routine.h"
#include "Co_Entity.h" // 协程的实体
#include "EpollWrapper/epoll.h"

#include <string.h>

#include <chrono>
#include <new> // bad_alloc
#include <functional>
#include <iostream> // cerr

namespace RocketCo {
    // -------------------  一些宏和别名
    using Poll_fun = std::function<int(struct pollfd fds[], nfds_t nfds, int timeout)>; // 用于Co_poll_inner

    // -------------------  结构体声明

    struct CoEventItemIink;
    struct Co_Rountinue_Env;

    // -------------------  函数声明

    Co_Rountinue_Env* get_thisThread_Env();
    void init_thisThread_Env();
    extern "C"
    {
        extern void coctx_swap( co_swap_t *,co_swap_t* ) asm("coctx_swap");
    };

    // -------------------  全局变量

    // 使用__thread效率上优于thread_local
    static __thread Co_Rountinue_Env* CurrentThread_CoEnv = nullptr;

    // -------------------

    // 多栈可以使得我们在进程切换的时候减少拷贝次数
    struct Co_ShareStack{
        unsigned int alloc_idx;		    // stack_array中我们在下一次调用中应该使用的那个共享栈的index
        int stack_size;				    // 共享栈的大小，这里的大小指的是一个stStackMem_t*的大小
        int count;					    // 共享栈的个数，共享栈可以为多个，所以以下为共享栈的数组
        Co_Stack_Member** stack_array;	// 栈的内容，这里是个数组，元素是stStackMem_t*
    };

    struct CoEventItem{         // 用于在epoll中存储事件
        CoEventItem* Prev;      // 前一个元素
        CoEventItem* Next;      // 后一个元素

        CoEventItemIink* link;  // 此item所属链表

        std::function<void(CoEventItem*, struct epoll_event&,CoEventItemIink*)> CoPrepare; //epoll中执行的云处理函数
        std::function<void(CoEventItem*)> CoPrecoss;        //epoll中执行的回调函数

        Co_Entity* ItemCo;   // 当前项所属的协程
        bool IsTimeout; // 标记这个事件是否已经超时
    };

    struct CoEventItemIink{
        CoEventItem *head;
        CoEventItem *tail;
    };

    struct Stpoll_t;
    struct StPollItem : public CoEventItem{
        struct pollfd *pSelf;       // 原本的的poll事件结构
        Stpoll_t* Stpoll;           // 所属的Stpoll
        struct ::epoll_event Event; // 对应epoll的事件
    };

    struct Stpoll_t{
        int epfd;                   // epoll fd
        int IsHandle;               // 所有的事件是否已经被处理的
        int RaisedNumber;           // 已经被触发的事件数
        nfds_t nfds;                // 描述的事件个数

        struct pollfd *fds;         // 指向poll传进来的全部事件
        StPollItem* WillJoinEpoll;  // 将会传递给epoll的事件集

        std::function<void(CoEventItem*)> CoPrecoss;
        void* ItemCo;
        //https://en.cppreference.com/w/cpp/chrono/time_point
        std::chrono::system_clock::time_point ExpireTime;   // 记录超时时间

        Stpoll_t(nfds_t nfds_, int epfd_, struct pollfd *fds_)
                :epfd(epfd_), nfds(nfds_), fds(fds_), IsHandle(0), RaisedNumber(0){
        }
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
        // 对调用栈中协程实体的一个拷贝,为了减少共享栈上数据的拷贝
        // 如果不加的话,我们需要O(N)的时间复杂度分清楚
        // Callback中current上一个共享栈的协程实体(可能共享栈与默认模式混合)
        Co_Entity* using_Co;    // 正在使用的协程
        Co_Entity* prev_Co;     // 调用栈中的上一个协程
    };

    struct Co_Attribute{
        static constexpr const int Default_StackSize = 128 * 1024;      // 128KB
        static constexpr const int Maximum_StackSize = 8 * 1024 * 1024; // 8MB
        int stack_size = Default_StackSize;              // 规定栈的大小
        Co_ShareStack* shareStack = nullptr;    // 默认为不共享
    };

    // 给协程实体分配内存并初始化
    // 线程局部的属性;协程属性;协程执行函数;函数参数;
    // 可能会抛出错误 std:bad_alloc
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
        Co_Entity* Co = new Co_Entity();
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
            Temp_StackMem = new Co_Stack_Member();

            Temp_StackMem->current_co = nullptr;    // TODO 没有用到这个元素
            Temp_StackMem->stack_size = attr->stack_size;
            Temp_StackMem->stack_buffer = new char[attr->stack_size];
            Temp_StackMem->stack_bp = Temp_StackMem->stack_buffer + Temp_StackMem->stack_size; // ebp
        }

        Co->Csm = Temp_StackMem;    //栈指针赋值
        Co->cst.ss_sp = Temp_StackMem->stack_buffer;    // 协程栈基址
        Co->cst.ss_size = Temp_StackMem->stack_size;    // 与前者相加为esp

        Co->ISstart = false;
        Co->IsMain = false;
        Co->IsEnd = false;
        Co->IsHook = false;
        Co->IsShareStack = attr->shareStack != nullptr;

        // 共享栈相关,可以暂时不用初始化,在执行协程切换的时候进行赋值
        Co->Used_Stack = nullptr;

        return Co;
    }


    // 创建一个协程的主体 与pthread_create类似
    // 协程实体;协程属性;协程执行函数;函数参数;
    void Co_Create(Co_Entity** Co, Co_Attribute* attr, Co_RealFun fun, void* arg){
        try{
            if(get_thisThread_Env() == nullptr){
                init_thisThread_Env();
            }

            Co_Entity* Temp = CreatAndAlloction(get_thisThread_Env(), attr, fun, arg);
            *Co = Temp;
        }catch (const std::bad_alloc& e){ // 可能分配内存失败
            std::cerr << "Error : Co_Create(). " << e.what() << std::endl;
        }
    }

    Co_Rountinue_Env* get_thisThread_Env(){
        return CurrentThread_CoEnv;
    }

    // 一个线程只能被调用一次，但不是只能被调用一次，所以没有写成单例
    void init_thisThread_Env(){
        CurrentThread_CoEnv = new Co_Rountinue_Env();
        Co_Rountinue_Env* Temp = CurrentThread_CoEnv;

        Temp->Co_ESP = 0;   // "调用栈"顶指针

        Co_Entity* Main = CreatAndAlloction(Temp, nullptr, nullptr, nullptr);
        Main->IsMain = true;// 调用init_thisThread_Env的协程一定是主协程

        Temp->prev_Co = nullptr;
        Temp->using_Co = nullptr;

        co_swap_init(&Main->cst);   // 初始化此协程实体上下文
        Temp->Co_CallBack[Temp->Co_ESP++] = Main;   // 放入线程独有环境中

        // 初始化环境变量中的Epoll
        Temp->Epoll_->epoll_t = new Epoll();
        Temp->Epoll_->TimeoutLink = new CoEventItemIink();
        Temp->Epoll_->ActiveLink  = new CoEventItemIink();
    }

    // 保存这个协程实体的栈空间
    void Copy_stackMember(Co_Entity* occupy){
        Co_Stack_Member* stackMember = occupy->Csm; // 获取当前协程的栈
        int UsedLength = std::distance(occupy->ESP, stackMember->stack_bp); // 求出当前栈上的有效栈空间
        if(occupy->Used_Stack){
            delete [] occupy->Used_Stack;
            occupy->Used_Stack = nullptr;
        }

        occupy->Used_Stack = new char[UsedLength];
        occupy->Used_Stack_size = UsedLength;

        // |     Ebp      |------------------|
        // |     Used     |这个区间是我们需要的  |
        // |     Esp      |------------------|
        // |   Not Used   |
        // | Stack_Buffer |
        memcpy(occupy->Used_Stack, occupy->ESP, UsedLength);
    }

    // pending为要切换到的协程,current为当前协程
    void co_swap_Co(Co_Entity* pending,Co_Entity* current){
        Co_Rountinue_Env *env = get_thisThread_Env(); // 执行这个函数的时候意味着init一定已经被调用

        // 一个保存ESP指针极其巧妙的方法
        char c;
        current->ESP = &c;

        if(! pending->IsShareStack){ // 非共享栈, 不需要更新
            env->using_Co = nullptr;
            env->prev_Co  = nullptr;
        } else {
            // 当前正在使用的协程
            env->using_Co = pending;

            // 与pending使用相同栈的上一个协程
            Co_Entity* occupy_Co = pending->Csm->current_co;
            env->prev_Co = occupy_Co;
            // 当此栈的上一个执行者
            if(occupy_Co && occupy_Co != pending){
                Copy_stackMember(occupy_Co);
            }
        }
        // coctx_swap函数一执行完就切换到另一个协程了
        coctx_swap(&(current->cst), &(pending->cst));
        // 到了pending协程了,pending协程执行完才会回到这里

        // 上面函数执行完到这里可能buffer已经经过了许多改变,可能env中的前后顺序也变了,所以需要重新获取
        Co_Rountinue_Env* cur_env = get_thisThread_Env();
        Co_Entity* restore_curr = cur_env->using_Co;
        Co_Entity* restore_prev = cur_env->prev_Co;

        if(restore_curr && restore_prev && restore_curr != restore_prev){
            if(restore_curr->Used_Stack && restore_curr->Used_Stack_size > 0){
                memcpy(restore_curr->ESP, restore_curr->Used_Stack, restore_curr->Used_Stack_size);
            }
        }
    }

    void Co_yeild(Co_Rountinue_Env* env){
        Co_Entity* pending = env->Co_CallBack[env->Co_ESP - 2];
        Co_Entity* current = env->Co_CallBack[env->Co_ESP - 1];

        // 既然当前协程已经执行完毕或者让出CPU,那么调用栈当然要减法1喽
        env->Co_ESP -= 1;

        // 交换两个协程的上下文,并对其中状态和栈做一些操作.
        co_swap_Co(pending, current);
    }

    // 对协程本来持有运行函数的一个封装,也就是协程实际运行的函数
    // TODO 这个地方的args和返回值可以省略,可以直接修改co_runningFunction定义
    static int Co_realRunningFun(Co_Entity* Co, void* args){
        if(Co->fun){
            Co->fun(Co->arg);
        }
        Co->IsEnd = true;       // 用户指定的函数已经运行完成.

        Co_yeild(Co->env);      // 把执行权让给env调用栈中上一个协程.

        return 0;
    };

    // 是当前线程的CPU执行权从当前正在运行的协程转移到
    void Co_resume(Co_Entity* Co) {
        Co_Rountinue_Env *env = Co->env;

        // 获取当前正在进行的协程主体,方便进行转换.ESP指向了当前的“调用栈”顶
        Co_Entity *current = env->Co_CallBack[env->Co_ESP - 1];
        if (!Co->ISstart) { // 还未执行过,我们需要获取寄存器的值.
            co_swap_make(&Co->cst,(co_runningFunction)Co_realRunningFun ,Co, 0);
            Co->ISstart = true;
        }
        env->Co_CallBack[env->Co_ESP++] = Co;
        co_swap_Co(Co, current);
    }

    Co_Epoll* GetCurrentCoEpoll(){
        if( !get_thisThread_Env()){
            init_thisThread_Env();
        }
        return get_thisThread_Env()->Epoll_;
    }

    Co_Entity *GetCurrCo(Co_Rountinue_Env *env ){
        return env->Co_CallBack[env->Co_ESP - 1];
    }

    Co_Entity* GetCurrentCoEntity(){
        if( !get_thisThread_Env()){
            init_thisThread_Env();
        }
        return GetCurrCo(get_thisThread_Env());
    }

    bool GetCurrentCoIsHook(){
        return GetCurrentCoEntity()->IsHook;
    }

    uint32_t EventPoll2Epoll(short events){
        uint32_t res;
        if( events & POLLIN ) 	  res |= EPOLLIN;
        if( events & POLLOUT )    res |= EPOLLOUT;
        if( events & POLLHUP ) 	  res |= EPOLLHUP;
        if( events & EPOLLRDHUP)  res |= EPOLLRDHUP;
        if( events & POLLERR )	  res |= EPOLLERR;
        if( events & POLLRDNORM ) res |= EPOLLRDNORM;
        if( events & POLLWRNORM ) res |= EPOLLWRNORM;
        // 还有一些事件是epoll有poll没有,就没啥必要写了
        return res;
    }

    constexpr const int ShortItemOptimization = 2;
    int Co_poll_inner(Co_Epoll* Epoll_, struct pollfd fds[], nfds_t nfds, int timeout, Poll_fun pollfunc){
        // TODO 获取epoll fd, 不一定有必要
        int epfd = Epoll_->epoll_t->fd();
        Co_Entity* self = GetCurrentCoEntity(); // 获取当前运行的协程

        auto Temp = new pollfd[nfds];
        Stpoll_t* poll_ = new Stpoll_t(nfds, epfd, Temp);

        // 数据量小的时候的一个优化,模仿std::string
        StPollItem array[ShortItemOptimization];
        // TODO 是否需要判断目前是否为共享栈模型
        if(nfds < ShortItemOptimization) {
            poll_->WillJoinEpoll = array;
        } else {
            poll_->WillJoinEpoll = new StPollItem[nfds];
        }
        bzero(poll_->WillJoinEpoll, sizeof(StPollItem) * nfds);
        poll_->ItemCo = GetCurrCo(get_thisThread_Env()); // 把此项所属的协程存起来,方便在事件完成的时候进行唤醒
        // 当事件就绪或超时时设置的回调,用于唤醒这个协程继续处理
        poll_->CoPrecoss = [](CoEventItem* para){Co_resume(para->ItemCo);};

        // 把poll事件添加到epoll中
        for(int i = 0; i < nfds; ++i){
            poll_->WillJoinEpoll[i].pSelf = poll_->fds + i;
            poll_->WillJoinEpoll[i].Stpoll = poll_;
            poll_->WillJoinEpoll[i].CoPrepare;

            struct ::epoll_event& ev = poll_->WillJoinEpoll[i].Event;

            if(fds[i].fd > -1){ // 如果套接字有效的话
                ev.data.ptr = poll_->WillJoinEpoll + i; // 在事件发生的时候使用
                ev.events = EventPoll2Epoll(fds[i].events);
                // 把此事件插入epoll
                int ret = Epoll_->epoll_t->Add({fds[i].fd, static_cast<EpollEventType>(ev.events)});

                if(ret < 0 && errno != EAGAIN && nfds <= ShortItemOptimization && pollfunc != nullptr){
                    // 显然小于等于ShortItemOptimization的时候不会给poll_->WillJoinEpoll分配内存
                    delete [] Temp;
                    delete poll_;
                    return pollfunc(fds, nfds, timeout);
                }
            }
            // nfds比较多而且失败的失败的话就触发超时,这里其实可以对超时事件做一个把控
        }

        // 添加到定时器中

        auto Now = std::chrono::system_clock::now(); // 获取当前事件
        // std::ratio基本单位为秒
        std::chrono::duration<int, std::ratio<1,1> > TimeOut(timeout);
        poll_->ExpireTime = Now + TimeOut;

        // TODO 6.10的任务是定时器

    }

    // TODO 等完成poll的一系列操作以后再来写下面两个函数
    void EveryLoopPrepare(CoEventItem* item){
        if(item->CoPrepare){

        }
    }

    // 主协程最终会运行到这里
    void EventLoop(Co_Epoll* Epoll_, const Co_EventLoopFun& fun, void* args){

        // epoll的事件结果集
        EpollEvent_Result Event_Reault(DefaultEpollEventSize());

        while(true){
            Epoll_->epoll_t->Epoll_Wait(Event_Reault);

            CoEventItemIink* active  = Epoll_->ActiveLink;
            CoEventItemIink* timeout = Epoll_->TimeoutLink;

            for(int i = 0; i < Event_Reault.size(); ++i) {
                CoEventItem* item = (CoEventItem*)Event_Reault[i].Return_Pointer()->data.ptr;
                EveryLoopPrepare(item);

                // TODO 先完成时间轮和poll的hook

            }
        }

    }

    // --------------------------
}