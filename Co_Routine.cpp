//
// Created by lizhaolong on 2020/6/8.
//

#include "Co_Routine.h"
#include "Co_Entity.h" // 协程的实体
#include "EpollWrapper/epoll.h"

#include <string.h>
#include <assert.h>
#include <chrono>
#include <vector>
#include <new> // bad_alloc
#include <functional>
#include <iostream> // cerr

namespace RocketCo {
    // -------------------  一些宏和别名

    // -------------------  结构体声明

    struct CoEventItemIink;
    struct Co_Rountinue_Env;

    // -------------------  函数声明

    Co_Rountinue_Env* get_thisThread_Env();
    void init_thisThread_Env();
    void Free_Co_Rountinue_Env(Co_Rountinue_Env* env);

    extern "C"
    {
        extern void coctx_swap( co_swap_t* ,co_swap_t* ) asm("coctx_swap");
    };

    // -------------------  全局变量



    // 使用__thread效率上优于thread_local
    __thread Co_Rountinue_Env* CurrentThread_CoEnv = nullptr;
    // 不支持thread_local
    thread_local std::unique_ptr<Co_Rountinue_Env, decltype(Free_Co_Rountinue_Env)*>
                DeleteThread_CoEnv(CurrentThread_CoEnv, Free_Co_Rountinue_Env);

    // -------------------

    Co_ShareStack::Co_ShareStack(int Count, int Stack_size)
    : count(Count), stack_size(Stack_size), alloc_idx(0){
        stack_array = (Co_Stack_Member**)calloc(count, sizeof(Co_Stack_Member*));
        for (int i = 0; i < count; ++i) {
            Co_Stack_Member* Temp = new Co_Stack_Member();
            Temp->current_co = nullptr;
            Temp->stack_size = this->stack_size;
            Temp->stack_buffer = new char[this->stack_size];
            // Temp->stack_buffer实际上是堆,地址从小到大,而程序运行过程中ebp指针由大向小移动
            Temp->stack_bp = Temp->stack_buffer + Temp->stack_size;
            stack_array[i] = Temp;
        }
    }

    Co_ShareStack::~Co_ShareStack(){
        for (int i = 0; i < count; ++i) {
            delete [] stack_array[i]->stack_buffer;
            delete stack_array[i];
        }
        free(stack_array);
    }

    struct CoEventItem{         // 用于在epoll中存储事件
        CoEventItem* Prev;      // 前一个元素
        CoEventItem* Next;      // 后一个元素

        CoEventItemIink* Link;  // 此item所属链表

        std::function<void(CoEventItem*, struct epoll_event*,CoEventItemIink*)> CoPrepare; //epoll中执行的云处理函数
        std::function<void(CoEventItem*)> CoPrecoss;        //epoll中执行的回调函数

        //https://en.cppreference.com/w/cpp/chrono/time_point
        std::uint64_t ExpireTime;   // 记录超时时间 eventloop中使用

        // TODO 很重要的一点,
        void* ItemCo;   // 当前项所属的协程
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

    struct Stpoll_t : public CoEventItem{
        int epfd;                   // epoll fd
        int IsHandle;               // 所有的事件是否已经被处理的
        int RaisedNumber;           // 已经被触发的事件数
        nfds_t nfds;                // 描述的事件个数

        struct pollfd *fds;         // 指向poll传进来的全部事件
        StPollItem* WillJoinEpoll;  // 将会传递给epoll的事件集

        Stpoll_t(nfds_t nfds_, int epfd_, struct pollfd *fds_)
                :epfd(epfd_), nfds(nfds_), fds(fds_), IsHandle(0), RaisedNumber(0){
            // 指针的初始化,非常关键,不加的话在addtail的条件判断中会出现问题
            WillJoinEpoll = nullptr;

            Prev = nullptr;
            Next = nullptr;
            Link = nullptr;
            ItemCo = nullptr;
            IsTimeout = false;
            ExpireTime = 0;
        }
    };

    std::uint64_t get_mill_time_stamp() {
        std::chrono::time_point<std::chrono::system_clock, std::chrono::milliseconds> tp = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now());
        auto tmp = std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch());
        return tmp.count();
    }

    // 链表中删除一个元素
    template <class T,class TLink>
    void RemoveFromWheel(T *ap){
        TLink *lst = ap->Link;
        if(!lst) return ;
        assert( lst->head && lst->tail );

        if( ap == lst->head ){
            lst->head = ap->Next;
            if(lst->head){
                lst->head->Prev = nullptr;
            }
        } else {
            if(ap->Prev){
                ap->Prev->Next = ap->Next;
            }
        }

        if( ap == lst->tail ){
            lst->tail = ap->Prev;
            if(lst->tail){
                lst->tail->Next = nullptr;
            }
        } else {
            ap->Next->Prev = ap->Prev;
        }

        // 设置为nullptr,保证可重入
        ap->Prev = ap->Next = nullptr;
        ap->Link = nullptr;
    }

    // 尾插
    template <class TNode,class TLink>
    void inline AddTail(TLink*apLink,TNode *ap){
        if( ap->Link ){
            return ;
        }
        if(apLink->tail){
            apLink->tail->Next = (TNode*)ap;
            ap->Next = nullptr;
            ap->Prev = apLink->tail;
            apLink->tail = ap;
        } else {
            apLink->head = apLink->tail = ap;
            ap->Next = ap->Prev = nullptr;
        }
        ap->Link = apLink;
    }

    // 合并链表 把apOther插入apLink
    template <class TNode,class TLink>
    void inline Join( TLink*apLink,TLink *apOther,int index )
    {
        if( !apOther->head ){
            return ;
        }
        TNode *lp = apOther->head;
        while( lp ){
            lp->Link = apLink;
            lp = lp->Next;
        }
        lp = apOther->head;
        if(apLink->tail){
            apLink->tail->Next = (TNode*)lp;
            lp->Prev = apLink->tail;
            apLink->tail = apOther->tail;
        } else {
            apLink->head = apOther->head;
            apLink->tail = apOther->tail;
        }

        apOther->head = apOther->tail = NULL;
    }

    template <class TNode,class TLink>
    void inline PopHead( TLink*apLink )
    {
        if( !apLink->head ){
            return ;
        }
        TNode *lp = apLink->head;
        if( apLink->head == apLink->tail ){
            apLink->head = apLink->tail = NULL;
        } else {//CoEventItem, CoEventItemIink
            apLink->head = apLink->head->Next;
        }

        lp->Prev = lp->Next = NULL;
        lp->Link = NULL;

        if( apLink->head ){
            apLink->head->Prev = NULL;
        }
    }

    // 一个时间轮
    class TimeWheel{
    private:
        std::vector<CoEventItemIink*> Wheel;// 当做一个循环链表使
        std::uint64_t Start;                // 时间轮中目前记录的最早时间,即调用的最早时机
        std::int64_t Index;                 // 最早时间对应的下标
        static constexpr const int DefaultSize = 60 * 60 * 24;// * 1000;
    public:
        explicit TimeWheel(int Length){
            if(Length < 0) Length = 0;
            Wheel.resize(Length);
            for (int i = 0; i < Length; ++i) {
                Wheel[i] = new CoEventItemIink();
            }
        }

        TimeWheel(){
            Wheel.resize(DefaultSize);
            for (int i = 0; i < DefaultSize; ++i) {
                Wheel[i] = new CoEventItemIink();
            }
        }

        /*
         * 1. 此函数可能抛出异常,插入的时候要catch一下
         * 2. 这个函数可以根据返回值来判断函数是否执行成功,0为成功,其他为出现错误
         * */
        int AddTimeOut(CoEventItem* ItemList, std::uint64_t Now){
            if(Start == 0){ // 第一次插入事件
                Start = Now;
                Index = 0;
            }
            if(Now < Start){ // 只可能等于或者大于
                std::cerr << "ERROR : TimeWheel.AddTimeOut Insertion time is less than initial time.\n";
                return __LINE__;
            }

            // 插入的时间已经小于插入时间的超时时间,直接返回即可
            if(ItemList->ExpireTime < Now){
                std::cerr << "ERROR : TimeWheel.AddTimeOut Insertion time is greater than timeout.\n";
                return __LINE__;
            }

            std::int64_t interval = ItemList->ExpireTime - Start; // 还有多长时间超时
            if(interval >= DefaultSize){
                std::cerr << "ERROR : TimeWheel.AddTimeOut Timeout is greater than Timewheel default size.\n";
                return __LINE__;
            }
            AddTail(Wheel[(Index + interval)%Wheel.size()], ItemList);
            return 0;
        }

        // 取出时的时间;把事件取出以后放入TimeOut链表
        void TakeOutTimeout(std::uint64_t Now, CoEventItemIink* TimeOut){
            if(Start == 0){ // 第一次插入事件
                Start = Now;
                Index = 0;
            }
            if(Now < Start){ // 只可能等于或者大于
                std::cerr << "ERROR : TimeWheel.TakeOutTimeout Take out time is less than initial time.\n";
                return;
            }
            // 记录Now到Start时间段区间,因为可能超过了时间轮本身的区间长度,不过一般情况下不会发生
            int interval = Now - Start + 1;
            if(interval > Wheel.size()){
                interval = Wheel.size();
            }
            if(interval <= 0){ // 显然当Now小于Start的时候就是发生了错误，需要退出
                std::cerr << "ERROR : TimeWheel.TakeOutTimeout Take out time is greater than timeout.\n";
                return;
            }
            // 现在Start到Now中间的区间中如果存在数据, 就可以看做超时的
            for (int i = 0; i < interval; ++i) {
                int index = (Index + i) % Wheel.size();
                Join<CoEventItem, CoEventItemIink>(TimeOut, Wheel[index], index);
            }
            // 更新必要的数据
            Start = Now;
            Index += interval - 1;
        }

        ~TimeWheel(){
            for (int i = 0; i < Wheel.size(); ++i) {
                if(Wheel[i] != nullptr){
                    delete Wheel[i];
                }
            }
        }
    };

    #define MaximumNumberOfCoroutinesAllowed 256

    class TimeWheel;
    struct Co_Epoll{
        Epoll* epoll_t;       // 封装的一个epoll

        TimeWheel TW;

        struct CoEventItemIink* ActiveLink;     // 正常的事件链表
        struct CoEventItemIink* TimeoutLink;    // 超时事件链表
    };

    struct Co_Rountinue_Env{
        Co_Entity* Co_CallBack[MaximumNumberOfCoroutinesAllowed];
        int Co_ESP; // 指向现在这个"调用栈"的栈顶

        Co_Epoll* Epoll_;

        // 对调用栈中协程实体的一个拷贝,为了减少共享栈上数据的拷贝
        // 如果不加的话,我们需要O(N)的时间复杂度分清楚
        // Callback中current上一个共享栈的协程实体(可能共享栈与默认模式混合)
        Co_Entity* using_Co;    // 正在使用的协程
        Co_Entity* prev_Co;     // 调用栈中的上一个协程
    };

    void FreeCo_Entity(Co_Entity* Co){
        delete Co;
        return;
    }

    // 传入一个指针数组
    void Free_Co_Entity_Array(Co_Entity* Co[], int len){
        for (int i = 0; i < len; ++i) {
            delete Co[i];
        }
        return;
    }

    // 给协程实体分配内存并初始化
    // 线程局部的属性;协程属性;协程执行函数;函数参数;
    // 可能会抛出错误 std:bad_alloc
    Co_Entity* CreatAndAlloction(Co_Rountinue_Env* env ,Co_Attribute* attr, Co_RealFun fun, void* arg){

        static int flags = 10;

        Co_Attribute Temp_attr; // 默认栈大小为128KB,且不支持共享栈
        if(attr){
            memcpy(&Temp_attr, attr, sizeof(Co_Attribute));
        }
        if(Temp_attr.stack_size <= 0){  // 小于零则为默认大小
            Temp_attr.stack_size = Co_Attribute::Default_StackSize;
        } else if (Temp_attr.stack_size > Co_Attribute::Maximum_StackSize){
            Temp_attr.stack_size = Co_Attribute::Maximum_StackSize;
        }

        // 4KB对齐,也就是说如果对stack_size取余不为零的时候对齐为4KB
        // 例如本来5KB,经过了这里就变为8KB了
        if(Temp_attr.stack_size & 0xFFF){
            Temp_attr.stack_size &= ~0xFFF;
            Temp_attr.stack_size += 0x1000;
        }

        Co_Entity* Co = new Co_Entity();
        bzero(Co, sizeof(Co_Entity));
        Co->env = env;
        Co->fun = fun;
        Co->arg = arg;
        Co->Env = nullptr;  // 命名我得承认有些许的愚蠢

        Co_Stack_Member* Temp_StackMem = nullptr;
        if(Temp_attr.shareStack){   // 共享栈,栈需要用户指定
            int index = Temp_attr.shareStack->alloc_idx % Temp_attr.shareStack->count; // 得到此次使用的共享栈下标
            Temp_attr.shareStack->alloc_idx++;
            Temp_StackMem = Temp_attr.shareStack->stack_array[index]; // 栈的实体
            Temp_attr.stack_size = Temp_attr.shareStack->stack_size;    // 共享栈大小
        } else {                // 每一个协程一个栈,没有了协程切换的拷贝的开销,但是内存碎片可能很大
            Temp_StackMem = new Co_Stack_Member();

            Temp_StackMem->current_co = nullptr;    // TODO 没有用到这个元素
            Temp_StackMem->stack_size = Temp_attr.stack_size;
            Temp_StackMem->stack_buffer = new char[Temp_attr.stack_size];
            Temp_StackMem->stack_bp = Temp_StackMem->stack_buffer + Temp_StackMem->stack_size; // ebp
        }

        //std::cout << "共享栈分配结束\n";

        Co->Csm = Temp_StackMem;    //栈指针赋值
        Co->cst.ss_sp = Temp_StackMem->stack_buffer;    // 协程栈基址
        Co->cst.ss_size = Temp_StackMem->stack_size;    // 与前者相加为esp

        Co->ISstart = false;
        Co->IsMain = false;
        Co->IsEnd = false;
        Co->IsHook = false;
        Co->IsShareStack = (Temp_attr.shareStack != nullptr);

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
        //std::cout << "开始初始化协程\n";

        DeleteThread_CoEnv.reset();
        CurrentThread_CoEnv = new Co_Rountinue_Env();
        DeleteThread_CoEnv.reset(CurrentThread_CoEnv);
        Co_Rountinue_Env* Temp = CurrentThread_CoEnv;

        Temp->Co_ESP = 0;   // "调用栈"顶指针

        Co_Entity* Main = CreatAndAlloction(Temp, nullptr, nullptr, nullptr);
        Main->IsMain = true;// 调用init_thisThread_Env的协程一定是主协程

        Temp->prev_Co = nullptr;
        Temp->using_Co = nullptr;


        co_swap_init(&Main->cst);   // 初始化此协程实体上下文
        Temp->Co_CallBack[Temp->Co_ESP++] = Main;   // 放入线程独有环境中
        // TODO 初始化环境变量中的Epoll,及其中的数据.空间销毁是个问题,
        // 后期如果手动销毁太麻烦的话替换为智能指针,就是损失性能
        Temp->Epoll_ = new Co_Epoll();
        Temp->Epoll_->epoll_t = new Epoll();
        Temp->Epoll_->TimeoutLink = new CoEventItemIink();
        Temp->Epoll_->ActiveLink  = new CoEventItemIink();
    }

    // 用在线程结束的时候 就算搞到析构函数里面对于env的delete还是得放到外面，治标不治本。如此看来智能指针是最优的
    void Free_Co_Rountinue_Env(Co_Rountinue_Env* env){
        // std::cout << "每线程变量删除"\n";
        if(env == nullptr) return;  // 对应与线程中没有调用过协程的情况
        delete env->Epoll_->epoll_t;
        delete env->Epoll_->TimeoutLink;
        delete env->Epoll_->ActiveLink;
        delete env->Epoll_;
        delete env;
        return;
    }

    // 保存这个协程实体的栈空间
    void Copy_stackMember(Co_Entity* occupy){
        Co_Stack_Member* stackMember = occupy->Csm; // 获取当前协程的栈
        //int UsedLength = std::distance(occupy->ESP, stackMember->stack_bp); // 求出当前栈上的有效栈空间
        int UsedLength = stackMember->stack_bp - occupy->ESP;
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

        if(!pending->IsShareStack){ // 非共享栈, 不需要更新
            env->using_Co = nullptr;
            env->prev_Co  = nullptr;
        } else {
            // 当前正在使用的协程
            env->using_Co = pending;

            // 与pending使用相同栈的上一个协程
            Co_Entity* occupy_Co = pending->Csm->current_co;
            pending->Csm->current_co = pending;
            env->prev_Co = occupy_Co;
            // 当此栈的上一个执行者
            if(occupy_Co && occupy_Co != pending){
                Copy_stackMember(occupy_Co);
            }
        }
        // coctx_swap函数一执行完就切换到另一个协程了
        // ------------
        //std::cout << "---------------------------------------\n";
//        std::cout << "current :: " << current->flag<< std::endl;
//        co_swap_t p1 = current->cst;
//        {
//            printf("%d %p\n",p1.ss_size, p1.ss_sp);
//            for (int i = 0; i < 14; ++i) {
//                std::cout << p1.regs[i] << " ";;
//            }
//            putchar('\n');
//        }
//        ttid = (int*)pending->arg;
//        if(pending->arg){
//            std::cout << *ttid << std::endl;
//        }
//        std::cout << "pending :: " << pending->flag  << std::endl;
//        co_swap_t p2 = pending->cst;
//        {
//            printf("%d %p\n",p2.ss_size, p2.ss_sp);
//            for (int i = 0; i < 14; ++i) {
//                std::cout << p2.regs[i] << " ";;
//            }
//            putchar('\n');
//        }
//        std::cout << "---------------------------------------\n";
        // ------------
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

    void Co_yeild(){
        Co_yeild(get_thisThread_Env());
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

/*    Co_Entity* GetCurrentCoEntity(){
        if( !get_thisThread_Env()){
            init_thisThread_Env();
        }
        return GetCurrCo(get_thisThread_Env());
    }*/

    // 为什么不像上面那样写呢,上面的写法会在主线程调用含有co_enable_hook_sys的函数是出现问题
    Co_Entity* GetCurrentCoEntity(){

        if( !get_thisThread_Env()){
            return nullptr;
        }
        return GetCurrCo(get_thisThread_Env());
    }

    bool GetCurrentCoIsHook(){
        Co_Entity* Temp = GetCurrentCoEntity();
        return Temp && GetCurrentCoEntity()->IsHook;
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

    static short EventEpoll2Poll(uint32_t events){
        short res;
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

    void CoPrepare_(CoEventItem* CoItem, struct epoll_event *ev, CoEventItemIink* active){
        StPollItem* Item = static_cast<StPollItem*>(CoItem);

        Item->pSelf->revents = EventPoll2Epoll(ev->events);

        Stpoll_t* poll_ = Item->Stpoll;
        poll_->RaisedNumber++;
        if(!poll_->IsHandle){
            poll_->IsHandle = true;
            RemoveFromWheel< CoEventItem,CoEventItemIink>(poll_); // 其实在回调回去以后也会执行这一步
            //这里加入Item和poll_一样
            AddTail(active, CoItem); // 将poll_加入到active队列中
        }
    }

    // timeout 默认单位为毫秒
    constexpr const int ShortItemOptimization = 2;
    int Co_poll_inner(Co_Epoll* Epoll_, struct pollfd fds[], nfds_t nfds, int timeout, Poll_fun pollfunc){
        int epfd = Epoll_->epoll_t->fd();
        Co_Entity* self = GetCurrentCoEntity(); // 获取当前运行的协程

        auto Temp = new pollfd[nfds];
        //TODO 这里fds是否有必有直接拷贝 我们直接把fds指针赋值一下不行吗
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
        poll_->CoPrecoss = [](CoEventItem* para){Co_resume(static_cast<Co_Entity*>(para->ItemCo));};

        // 把poll事件添加到epoll中
        for(int i = 0; i < nfds; ++i){
            poll_->WillJoinEpoll[i].pSelf     = poll_->fds + i;
            poll_->WillJoinEpoll[i].Stpoll    = poll_;
            poll_->WillJoinEpoll[i].CoPrepare = CoPrepare_;
            // TODO 第一次忘记的地方 用于事件触发时的回调
            poll_->WillJoinEpoll[i].ItemCo = static_cast<void*>(GetCurrentCoEntity());
            poll_->WillJoinEpoll[i].CoPrecoss = [](CoEventItem* para){Co_resume(static_cast<Co_Entity*>(para->ItemCo));};

            struct ::epoll_event& ev = poll_->WillJoinEpoll[i].Event;
            if(fds[i].fd > -1){ // 如果套接字有效的话
                ev.data.ptr = poll_->WillJoinEpoll + i; // 在事件发生的时候使用
                ev.events   = EventPoll2Epoll(fds[i].events);
                // 把此事件插入epoll
                int ret = epoll_ctl( epfd,EPOLL_CTL_ADD, fds[i].fd, &ev );

                if(ret < 0 && errno != EAGAIN && nfds <= ShortItemOptimization && pollfunc != nullptr){
                    // 显然小于等于ShortItemOptimization的时候不会给poll_->WillJoinEpoll分配内存
                    delete [] Temp;
                    delete poll_;
                    return pollfunc(fds, nfds, timeout);
                }
            }
            // nfds比较多而且失败的失败的话就触发超时,这里其实可以对超时事件做一个把控
        }
        //std::cout << "添加到定时器中\n";
        // 添加到定时器中

        // 精度为毫秒
        std::uint64_t Now = get_mill_time_stamp();
        poll_->ExpireTime = Now + timeout;

        // 指定当前时间, 把此次poll中的时间插入到时间轮中,且所有事件超时时间相同
        int ret = Epoll_->TW.AddTimeOut(poll_, Now);
        // std::cout << "时间轮插入完成\n";
        // 返回值为零正常,其他为出现错误
        int RaisedNumber = 0;
        if(ret != 0){
            RaisedNumber = -1;
        } else { // 等于零的话说明上面插入正常,否则的话在ret那一行出现了错误
            // TODO 问题出在这里
            Co_yeild(get_thisThread_Env()); // 切出时间片，因为此时应该阻塞了，当事件ok的时候会执行回调切回来

            RaisedNumber = poll_->RaisedNumber; // 在epoll中触发回调时修改
        }
        // 已经执行完，这里把这些事件从时间轮和epoll中去除。
        RemoveFromWheel<CoEventItem,CoEventItemIink>(poll_);
        for (int i = 0; i < nfds; ++i){
            int fd = fds[i].fd;
            epoll_ctl(epfd, EPOLL_CTL_DEL, fd, &poll_->WillJoinEpoll[i].Event);
            fds[i].revents = poll_->fds[i].revents;
        }
        if(poll_->WillJoinEpoll != array){
            delete [] poll_->WillJoinEpoll;
            poll_->WillJoinEpoll = nullptr;
        }

        delete [] Temp; // poll_->fds
        delete poll_;

        return RaisedNumber; // 返回成功或者超时的事件数
    }

    // TODO 等完成poll的一系列操作以后再来写下面两个函数
    void EveryLoopPrepare(CoEventItem* item){
        if(item->CoPrepare){

        }
    }

    void SetUpTimepoutLable(CoEventItemIink* Timeout){
        CoEventItem* item = Timeout->head;
        while(item){
            item->IsTimeout = true;
            item = item->Next;
        }
    }

    // 主协程最终会运行到这里
    void EventLoop(Co_Epoll* Epoll_, const Co_EventLoopFun& fun, void* args){

        // epoll的事件结果集
        EpollEvent_Result Event_Reault(DefaultEpollEventSize());

        while(true){
            Epoll_->epoll_t->Epoll_Wait(Event_Reault, 1);

            CoEventItemIink* active  = Epoll_->ActiveLink;
            CoEventItemIink* timeout = Epoll_->TimeoutLink;

            for(int i = 0; i < Event_Reault.size(); ++i) {
                CoEventItem* item = (CoEventItem*)Event_Reault[i].Return_Pointer()->data.ptr;
                if(item->CoPrepare){
                    // 从时间轮中去除，并更新标记位
                    item->CoPrepare(item, Event_Reault[i].Return_Pointer(), active);
                } else {
                    AddTail(active, item);
                }
            }
            std::uint64_t Now = get_mill_time_stamp();
            // 把时间轮中的超时事件插入到timeout链表中
            Epoll_->TW.TakeOutTimeout(Now, timeout);
            SetUpTimepoutLable(timeout);

            Join<CoEventItem, CoEventItemIink>(active, timeout,0);

            CoEventItem* item = active->head;
            // 对active链表中的数据做处理,可能为超时事件或者就绪事件,
            while(item){
                //printf("%p %p %d\n", item->ItemCo,item->Next, static_cast<Co_Entity*>(item->ItemCo)->flag);

                // 两个条件不可能同时满足
                if(item->IsTimeout && Now < item->ExpireTime){
                    std::cerr << "ERROR : EventLoop Unexpected error.\n";
                }
                PopHead<CoEventItem, CoEventItemIink>(active);

                //Co_resume(static_cast<Co_Entity*>(item->ItemCo));
               if(item->CoPrecoss){
                    //std::cout << "执行回调\n";
                    // 回调执行Co_resume
                    item->CoPrecoss(item);
                }
                item = active->head;
            }
            // 一遍循环走完active和timeout都空了

            // 用于自定义行为,不设定的话就不会退出了
            if(fun){
                if(fun(args) == -1){
                    break;
                }
            }
        }
    }
    // --------------------------
    // 条件变量相关实现,没有办法直接hook,因为没办法放到epoll中,所以需要实现一下
    struct ConditionVariableLink;
    struct ConditionVariableItem{
        ConditionVariableItem* Prev;
        ConditionVariableItem* Next;
        ConditionVariableLink* Link;

        CoEventItem Item;       // 使得条件变量可以加到epoll中
    };

    struct ConditionVariableLink{
        ConditionVariableItem* head;
        ConditionVariableItem* tail;
    };

    ConditionVariableLink* ConditionVariableInit(){
        ConditionVariableLink* Temp = nullptr;
        try {
            Temp = new ConditionVariableLink();
        }catch (const std::bad_alloc& err){
            std::cerr << err.what() << std::endl;
            return nullptr;
        }
        return Temp;
    }

    void ConditionVariableFree(ConditionVariableLink* CV){
        delete CV;
    }

    // 一个条件变量的实体;超时时间; pthread_cond_wait
    int ConditionVariableWait(ConditionVariableLink* CV, int TimeOut){
        ConditionVariableItem* Item = new ConditionVariableItem();
        Item->Item.ItemCo = GetCurrentCoEntity();
        // 设置epoll中的回调,其实就是切换协程的执行权
        Item->Item.CoPrecoss = [](CoEventItem* Co){Co_resume(static_cast<Co_Entity*>(Co->ItemCo));};

        if(TimeOut > 0){
            std::uint64_t Now = get_mill_time_stamp();
            Item->Item.ExpireTime = Now + TimeOut;

            // 当我们调用完AddTimeOut要根据返回值查看调用是否成功
            int ret = get_thisThread_Env()->Epoll_->TW.AddTimeOut(&(Item->Item), Now);
            if(ret != 0){
                delete Item;
                std::cerr << "ERROR : ConditionVariableWait, Call AddTimeOut error.\n";
                return ret;
            }
        }
        // 相当于timeout为负的话超时时间无限
        AddTail(CV, Item);

        Co_yeild(); // 切换CPU执行权,在epoll中触发peocess回调以后回到这里

        // 这个条件要么被触发,要么已经超时,从条件变量实体中删除
        RemoveFromWheel<ConditionVariableItem, ConditionVariableLink>(Item);

        delete Item;
    }

    ConditionVariableItem* PopFromLink(ConditionVariableLink* CV){
        ConditionVariableItem* Temp = CV->head;
        if(Temp){
            PopHead<ConditionVariableItem, ConditionVariableLink>(CV);
        }
        return Temp;
    }
    // 类似于pthread_cond_signal
    int ConditionVariableSignal(ConditionVariableLink* CV){
        ConditionVariableItem* Head = PopFromLink(CV);
        if(!Head){
            return -1; // 一个非阻塞的函数,当队列中不存在事件的时候返回
        }
        //
        RemoveFromWheel<CoEventItem,CoEventItemIink>(&(Head->Item));

        // 加到active队列中,在下一轮epoll_wait中处理.调用process回调,回到ConditionVariableWait中
        AddTail(get_thisThread_Env()->Epoll_->ActiveLink, &(Head->Item));

        return 0; // 正常返回,返回值为0
    }

    // 类似于pthread_cond_Broadcast,唤醒目前条件变量中的所有事件
    int ConditionVariableBroadcast(ConditionVariableLink* CV){
        while(true){
            ConditionVariableItem* Head = PopFromLink(CV);
            if(Head == nullptr) break;
            RemoveFromWheel<CoEventItem,CoEventItemIink>(&(Head->Item));

            AddTail(get_thisThread_Env()->Epoll_->ActiveLink, &(Head->Item));
        }
        return 0;
    }
    // --------------------------
}