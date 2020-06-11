//
// Created by lizhaolong on 2020/6/9.
//

#include "Co_Routine.h"
#include "Co_Entity.h"

#include <poll.h>
#include <dlfcn.h>
#include <iterator>
#include <unordered_map>

namespace RocketCo{

    // ------------------------------------
    // hook前的函数声明
    // 格式为{name##_t}
    typedef int(*poll_t)(struct pollfd fds[], nfds_t nfds, int timeout);

    // ------------------------------------

    // ------------------------------------
    // 对函数进行hook,并存储hook前的函数,因为在封装以后还是要调用
    // 函数dlsym的参数RTLD_NEXT可以在对函数实现所在动态库名称未知的情况下完成对库函数的替代
    // 格式为{Hook_##name##_t}
    static poll_t Hook_poll_t = (poll_t)dlsym(RTLD_NEXT, "poll");

    // ------------------------------------
    // 对于hook函数的封装

    #define HOOK_SYS_FUNC(name) if( !Hook_##name##_t ) { Hook_##name##_t = (name##_t)dlsym(RTLD_NEXT,#name); }

    int poll(struct pollfd fds[], nfds_t nfds, int timeout){
        // 只定义了一些函数,如果没hook的话,这里hook一下,使用的方法和一般的一样.宏会被展开
        // 文件头定义一些常用的,减少开销,不想麻烦的话文件前面都可以不用写.
        HOOK_SYS_FUNC(poll);

        // 要么这个协程没有被hook, 要么这个poll本来就是非阻塞的
        if(!GetCurrentCoIsHook() || !timeout){ // 等于零的时候当做非阻塞
            return Hook_poll_t(fds, nfds, timeout);
        }

        if(timeout < 0){ // 当小于零的时候视为无限延迟 timeout的其他情况就都是有意义的了
            timeout = INT32_MAX;
        }

        // 对poll的参数执行一次合并
        pollfd* merge = nullptr;
        int index = 0;

        std::unordered_map<int, int>::iterator item;
        std::unordered_map<int, int> Fd_ToEvent;

        if(nfds > 1){ // 显然至少为2的时候才有合并的必要
            merge = (pollfd *)malloc(sizeof(pollfd) * nfds);
            if(merge == nullptr){ // 分配可能出现失败
                throw std::bad_alloc();
            }
            for (int i = 0; i < nfds; ++i) {
                if((item = Fd_ToEvent.find(fds[i].fd)) == Fd_ToEvent.end()){
                    merge[index] = fds[i];
                    Fd_ToEvent[fds[i].fd] = index++;
                } else {
                    merge[Fd_ToEvent[fds[i].fd]].events |= fds[i].events;
                }
            }
        }
        // 此时index为合并后的事件数
        int Readly_size = 0;
        if(nfds == index || nfds < 2){
            Readly_size = Co_poll_inner(GetCurrentCoEpoll, fds, nfds, timeout, Hook_poll_t);
        } else {
            Readly_size = Co_poll_inner(GetCurrentCoEpoll(), merge, index, timeout, Hook_poll_t);
            // 把事件还原一下
            if(Readly_size > 0){ // 有事件返回,要么是超时事件,要么是注册的事件被触发
                for (int i = 0; i < nfds; ++i) {
                    item = Fd_ToEvent.find(fds[i].fd);
                    fds[i].revents = merge[item->second].revents & fds[i].events;
                }
            }
        }
        free(merge);
        return Readly_size;
    }


    // 包含了这个函数才会把hook.cpp中的内容链接到源程序中,从而完成hook
    void co_enable_hook_sys(){
        Co_Entity* Co = GetCurrentCoEntity();
        if(Co){
            Co->IsHook = true;
        }
    }
}