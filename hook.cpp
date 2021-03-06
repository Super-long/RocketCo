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

#include "Co_Routine.h"
#include "Co_Entity.h"


#include <dlfcn.h>
#include <poll.h>
#include <fcntl.h> 
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <cstdarg>  // 可变函数参数相关
#include <errno.h>

#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>

#include <iostream>

    // ------------------------------------ 
    // hook前的函数声明
    // 格式为{name##_t}
    typedef int(*poll_t)(struct pollfd fds[], nfds_t nfds, int timeout);

    typedef ssize_t (*read_t)(int fd, void *buf, size_t nbyte);
    typedef ssize_t (*write_t)(int fd, const void *buf, size_t nbyte);

    typedef int (*socket_t)(int domain, int type, int protocol);
    typedef int (*connect_t)(int fd, const struct sockaddr *address, socklen_t address_len);
    typedef int (*close_t)(int fd);

    typedef int (*fcntl_t)(int fd, int cmd, ...);   // 最后一个参数是可变参

    typedef int (*setenv_t)(const char *name, const char *value, int overwrite);
    typedef int (*unsetenv_t)(const char *name);
    typedef char *(*getenv_t)(const char *name);

    // sendto, recvfrom与UDP相关
    typedef ssize_t (*sendto_t)(int fd, const void *message, size_t length,
                                int flags, const struct sockaddr *dest_addr,
                                socklen_t dest_len);

    typedef ssize_t (*recvfrom_t)(int fd, void *buffer, size_t length,
                                  int flags, struct sockaddr *address,
                                  socklen_t *address_len);

    typedef ssize_t (*send_t)(int fd, const void *buffer, size_t length, int flags);
    typedef ssize_t (*recv_t)(int fd, void *buffer, size_t length, int flags);
    // ------------------------------------

    // ------------------------------------
    // 对函数进行hook,并存储hook前的函数,因为在封装以后还是要调用
    // 函数dlsym的参数RTLD_NEXT可以在对函数实现所在动态库名称未知的情况下完成对库函数的替代
    // 格式为{Hook_##name##_t}

    static read_t Hook_read_t = (read_t)dlsym(RTLD_NEXT, "read");
    static write_t Hook_write_t = (write_t)dlsym(RTLD_NEXT, "write");
    static poll_t Hook_poll_t = (poll_t)dlsym(RTLD_NEXT, "poll");

    static socket_t Hook_socket_t = (socket_t)dlsym(RTLD_NEXT, "socket");
    static connect_t Hook_connect_t = (connect_t)dlsym(RTLD_NEXT, "connect");
    static close_t Hook_close_t = (close_t)dlsym(RTLD_NEXT, "close");

    static fcntl_t Hook_fcntl_t = (fcntl_t)dlsym(RTLD_NEXT, "fcntl");

    static setenv_t Hook_setenv_t = (setenv_t)dlsym(RTLD_NEXT, "setenv");
    static unsetenv_t Hook_unsetenv_t = (unsetenv_t)dlsym(RTLD_NEXT, "unsetenv");
    static getenv_t Hook_getenv_t = (getenv_t)dlsym(RTLD_NEXT, "getenv");

    static sendto_t Hook_sendto_t = (sendto_t)dlsym(RTLD_NEXT, "sendto");
    static recvfrom_t Hook_recvfrom_t = (recvfrom_t)dlsym(RTLD_NEXT, "recvfrom");
    static send_t Hook_send_t = (send_t)dlsym(RTLD_NEXT, "send_t");
    static recv_t Hook_recv_t = (recv_t)dlsym(RTLD_NEXT, "recv_t");

    // ------------------------------------
    // 必要的结构体,为了少一次系统调用,且记录用户原本注册的事件

    struct FdAttributes{
        struct sockaddr_in addr;    // 套接字目标地址
        int domain;                 // AF_LOCAL->域套接字 , AF_INET->IP
        int fdFlag;                 // 记录套接字状态
        std::int64_t read_timeout;  // 读超时时间
        std::int64_t wirte_timeout; // 写超时时间
    };

    // cat /proc/PID/limists | grep “Max open files”
    static constexpr const int AttributesLength = 8196;
    static FdAttributes* Fd2Attributes[AttributesLength];

    // 用于释放FdAttributes，有可能后面其析构函数中会进行一些数据的保存，所以这样是必不可少的;
    class FreeFd2Attributes{
        public:
            constexpr FreeFd2Attributes() = default;
            FreeFd2Attributes(const FreeFd2Attributes&) = delete;
            FreeFd2Attributes(FreeFd2Attributes&&) = delete;
            FreeFd2Attributes& operator=(const FreeFd2Attributes&) = delete;
            FreeFd2Attributes& operator=(FreeFd2Attributes&&) = delete;
            ~FreeFd2Attributes(){
                for (int i = 0; i <AttributesLength; ++i){
                    if(Fd2Attributes[i] != nullptr){
                        delete Fd2Attributes[i];
                    }
                }
            }
    };

    static FreeFd2Attributes StaticFreeAttribute;


    // ------------------------------------
    // 对于hook函数的封装

    #define HOOK_SYS_FUNC(name) if( !Hook_##name##_t ) { Hook_##name##_t = (name##_t)dlsym(RTLD_NEXT,#name); }

int poll(struct pollfd fds[], nfds_t nfds, int timeout){
        // 只定义了一些函数,如果没hook的话,这里hook一下,使用的方法和一般的一样.宏会被展开
        // 文件头定义一些常用的,减少开销,不想麻烦的话文件前面都可以不用写.
        HOOK_SYS_FUNC(poll);

        // 要么这个协程没有被hook, 要么这个poll本来就是非阻塞的
        if(!RocketCo::GetCurrentCoIsHook() || !timeout){ // 等于零的时候当做非阻塞
            return Hook_poll_t(fds, nfds, timeout);
        }

        if(timeout < 0){ // 当小于零的时候视为无限延迟 timeout的情况就都是有意义的了
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
        if(nfds == index || nfds < 2){ // 没有执行合并
            //std::cout << "进入 Co_poll_inner\n";
            Readly_size = Co_poll_inner(RocketCo::GetCurrentCoEpoll(), fds, nfds, timeout, Hook_poll_t);
        } else {
            Readly_size = Co_poll_inner(RocketCo::GetCurrentCoEpoll(), merge, index, timeout, Hook_poll_t);
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

    static inline FdAttributes* GetFdAttributesByFd(int fd){
        if(fd >= -1 && fd < AttributesLength){  // fd有效
            return Fd2Attributes[fd];           // 当然这里也有可能返回nullptr
        }
        return nullptr;
    }

    static inline void DeleteAttributes(int fd){
        if(fd >= -1 && fd < AttributesLength){
            if(Fd2Attributes[fd]){
/*                 delete Fd2Attributes[fd];
                Fd2Attributes[fd] = nullptr; */
                Fd2Attributes[fd]->fdFlag = 0;
                Fd2Attributes[fd]->domain = 0;
                bzero(&(Fd2Attributes[fd]->addr), sizeof(struct sockaddr_in));
            }
        }
    }


    static inline FdAttributes* CreateAFdAttributes(int fd){
        if(fd < 0 && fd >= AttributesLength) return nullptr;
        
        FdAttributes* Temp = nullptr;

        if(Fd2Attributes[fd] != nullptr){
            Temp = Fd2Attributes[fd];
        } else {
            Temp = new FdAttributes();
            memset(Temp, 0, sizeof(FdAttributes));
            Temp->read_timeout  = 1000; // 设定默认超时时间为1s
            Temp->wirte_timeout = 1000;
            Fd2Attributes[fd] = Temp;
        }

        return Temp;
    }

   int socket(int domain, int type, int protocol){
        HOOK_SYS_FUNC(socket);
        std::cout << "进入socket\n";
        if(!RocketCo::GetCurrentCoIsHook()){
            return Hook_socket_t(domain, type, protocol);
        }
        int fd = Hook_socket_t(domain, type, protocol);
        if(fd < 0){
            std::cerr << "ERROR: error in create a socket in socket().\n";
            return fd;
        }
        FdAttributes* Attributes = CreateAFdAttributes(fd);
        Attributes->domain = domain;

        // TODO 套接字状态和地址还未设定

        // F_SETFL会把套接字设置成非阻塞的
        fcntl( fd, F_SETFL, Hook_fcntl_t(fd, F_GETFL,0 ));

        return fd;
   }

   ssize_t read(int fd, void *buf, size_t nbyte){
        HOOK_SYS_FUNC(read);
        std::cout << "进入read\n";
        if(!RocketCo::GetCurrentCoIsHook()){
            return Hook_read_t(fd, buf, nbyte);
        }
        
        FdAttributes* Attributes = GetFdAttributesByFd(fd);

        // 证明套接字非阻塞,直接调用;这是用户希望的非阻塞，当然不需要加入事件循环啦
        if(!Attributes ||  Attributes->fdFlag & O_NONBLOCK){
            return Hook_read_t(fd, buf, nbyte);
        }

        std::int64_t timeout = Attributes->read_timeout;
        struct pollfd pollEvent;
        bzero(&pollEvent, sizeof(pollfd));
        pollEvent.fd = fd;
        pollEvent.events = ( POLLIN | POLLERR | POLLHUP );

        // hook后的pool CPU在这里切换使用权
        poll(&pollEvent, 1, timeout); // 返回值不重要,肯定是1
        std::cout <<"----------------------" << errno << std::endl;
        ssize_t res = Hook_read_t(fd, buf, nbyte);

        if(res < 0){ // 超时
            std::cerr << "ERROR: Hooked Read timeout!\n";
        }
        return res; // 返回实际读取的字节数
   }

    int fcntl(int fd, int cmd, ...){
        HOOK_SYS_FUNC(fcntl);

        if(fd < 0){ // 当函数返回非零值的时候都是非正常返回
            return __LINE__;
        }

        va_list arg_list;
        va_start( arg_list,cmd );
        FdAttributes* Attributes = CreateAFdAttributes(fd);

        int ret = 0;
        // https://man7.org/linux/man-pages/man2/fcntl.2.html
        // https://blog.csdn.net/bical/article/details/3014731?ops_request_misc=&request_id=&biz_id=102&utm_term=fcntl%E7%AC%AC%E4%BA%8C%E4%B8%AA%E5%8F%82%E6%95%B0&utm_medium=distribute.pc_search_result.none-task-blog-2~all~sobaiduweb~default-0-3014731
        switch (cmd){
            case F_GETFD:   // 忽略第三个参数
            {
                ret = Hook_fcntl_t(fd, cmd);
                // 用户创建的套接字如果不包含O_NONBLOCK的话,返回值也不包含
                if(Attributes && !(Attributes->fdFlag & O_NONBLOCK)){
                    ret = ret & (~O_NONBLOCK);
                }
                break;
            }
            case F_SETFD:
            {
                int param = va_arg(arg_list,int);
                Hook_fcntl_t(fd, cmd, param);
                break;
            }
            case F_GETFL:
            {
                ret = Hook_fcntl_t(fd, cmd);
                break;
            }
            case F_SETFL:   // 忽略第三个参数
            {   // 只允许O_APPEND、O_NONBLOCK和O_ASYNC位的改变
                int param = va_arg(arg_list,int);
                int flag = param;
                if(RocketCo::GetCurrentCoIsHook() && Attributes){
                    flag |= O_NONBLOCK;
                }
                ret = Hook_fcntl_t(fd, cmd, flag);  // 一定是非阻塞的
                // fcntl正常返回0
                if(!ret && Attributes){
                    Attributes->fdFlag = param;     // Attributes中存储原来的信息
                }
                break;
            }
            case F_GETLK:
            {
                struct flock *param = va_arg(arg_list,struct flock *);
                ret = Hook_fcntl_t(fd, cmd, param);
                break;
            }
            case F_SETLK:
            {
                struct flock *param = va_arg(arg_list,struct flock *);
                ret = Hook_fcntl_t(fd, cmd, param);
                break;
            }
            case F_SETLKW:
            {
                struct flock *param = va_arg(arg_list,struct flock *);
                ret = Hook_fcntl_t(fd, cmd, param);
                break;
            }
            case F_DUPFD: // 查找大于或等于参数arg的最小且仍未使用的文件描述词
            {
                int param = va_arg(arg_list,int); // 指向arg_list当前元素的下一个
                ret = Hook_fcntl_t(fd , cmd, param);
                break;
            }
        }
        va_end( arg_list );
        return ret;
    }

    int AcceptFd2Attributes( int fd, struct sockaddr *addr, socklen_t *len ){
        int cli = accept( fd,addr,len );
        if( cli < 0 )
        {
            return cli;
        }
        CreateAFdAttributes(cli);
        return cli;
    }

    // 返回值大于零时为已写入字节数
    // 小于等于零时为出现错误

    ssize_t write(int fd, const void *buf, size_t nbyte){
        std::cout << "进入write\n";
        HOOK_SYS_FUNC(write);
        if(!RocketCo::GetCurrentCoIsHook()) {
            return Hook_write_t(fd, buf, nbyte);
        }
        FdAttributes* Attributes = CreateAFdAttributes(fd);
        //
        if(!Attributes || Attributes->fdFlag & O_NONBLOCK){
            return Hook_write_t(fd, buf, nbyte);
        }
        std::int64_t timeout = Attributes->wirte_timeout;

        // 目前已写大小
        size_t wrotelen = 0;

        // 因为TCP协议的原因,有时可能因为接收方窗口的原因无法发满

        int ret = Hook_write_t(fd, buf, nbyte);
        if(ret <= 0){
            return ret;
        }
        wrotelen += ret;

        while(wrotelen < nbyte){ // 一般退出时为等于
            struct pollfd pf = { 0 };
            pf.fd = fd;
            pf.events = ( POLLOUT | POLLERR | POLLHUP );
            poll( &pf,1,timeout );
            ret = Hook_write_t(fd, (const char*)buf + wrotelen,nbyte - wrotelen);

            if(ret <= 0) break;

            wrotelen += ret;
        }
        if(ret <= 0 && !wrotelen){
            return ret;
        }
        return wrotelen;
    }

    int connect(int fd, const struct sockaddr *address, socklen_t address_len){
        HOOK_SYS_FUNC(connect);
        if(!RocketCo::GetCurrentCoIsHook()) {
            return Hook_connect_t(fd, address, address_len);
        }

        int ret = Hook_connect_t(fd, address, address_len);

        FdAttributes* Attributes = GetFdAttributesByFd(fd);
        //printf("fdFlag 0x%X ; O_NONBLOCK :0x%X\n", Attributes->fdFlag,O_NONBLOCK);
        if(!Attributes || Attributes->fdFlag & O_NONBLOCK)
            return ret; // 不是由hook的socket创建的,或者本来就是非阻塞
        if(!(ret < 0 && errno == EINPROGRESS)) // errno == 115 Operation now in progress
            return ret;

        if( sizeof(Attributes->addr) >= address_len ){
            memcpy(&(Attributes->addr),address,(int)address_len);
        }
        int res = 0;
        struct pollfd pollEvent = {0};
        int RetransmissionInterval = 2000; // ms
        for (int i = 0; i < 5; ++i) {
            bzero(&pollEvent, sizeof(struct pollfd));
            pollEvent.fd = fd;
            pollEvent.events = ( POLLOUT | POLLERR | POLLHUP );
            res = poll(&pollEvent, 1, RetransmissionInterval);  // 切换CPU执行权

            if(1 == res && pollEvent.revents & POLLOUT){ // 此返回事件不是超时事件
                break;
            }
            RetransmissionInterval *= 2; // 超时时间二进制倍增
        }
        // 当然这里一定要牢记,可写不一定连接成功,需要使用getsockopt确认一下,muduo中也提到了这个
        if(pollEvent.revents & POLLOUT){ // 可能五次全部超时
            int err = 0;
            socklen_t errlen = sizeof(err);
            ret = getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &errlen);
            if (ret < 0) {
                return ret;
            } else if (err != 0) {
                errno = err;
                return -1;
            }
            errno = 0;
            return 0;
        }
        if(RetransmissionInterval != 2000){
            errno = ETIMEDOUT; // 连接超时
        }
        return ret;
    }

    int close(int fd){
        std::cout << "进入close\n";
        HOOK_SYS_FUNC(close);
        if(!RocketCo::GetCurrentCoIsHook()) {
            return Hook_close_t(fd);
        }
        DeleteAttributes(fd);   // 在Fd2Attributes中销毁对象
        return Hook_close_t(fd);
    }

    struct Sysenv{
        // {name, data}
        std::unordered_map<std::string, std::string> env;
        //std::vector<std::pair<std::string,std::string>> env;
    };

    static Sysenv co_sysenv; // 全局变量,默认填充0

    void InitEnv(std::vector<std::pair<std::string,std::string>>* para){ //顶层const,可修改值
        // 仅初始化一次
        if(co_sysenv.env.size()){
            return;
        }
        for(const auto& x : *para){  // 把传入的参数进行拷贝
            co_sysenv.env[x.first] = x.second;
        }
        // 哈希表是一个更优的选择
/*        if(co_sysenv.env.size() > 1){   // 对传入的数据排序,增加检索时的效率
            std::sort(co_sysenv.env.begin(), co_sysenv.env.end());
        }
        // 去重
        co_sysenv.env.erase(std::unique(co_sysenv.env.begin(), co_sysenv.env.end()), co_sysenv.env.end());*/
        return;
    }

    int setenv(const char *n, const char *value, int overwrite){
        HOOK_SYS_FUNC(setenv);
        // 只有函数已经被hook且调用过initEnv才会使用用户态的env
        if(RocketCo::GetCurrentCoIsHook() && co_sysenv.env.size()){
            RocketCo::Co_Entity* Co = RocketCo::GetCurrentCoEntity();
            if(Co != nullptr){  // 防止主线程调用含有enable_hook的函数
                if(!Co->Env){
                    // 没有拷贝,还是使用一个指针,即所有协程共享一个全局列表
                    // 想改的话分配一份内存拷贝一下就可以了,不过需要注意内存的释放
                    Co->Env = static_cast<void*>(&co_sysenv);
                }
                std::string Name(n);
                std::string Value(value);
                if(co_sysenv.env.find(Name) != co_sysenv.env.end()){
                    if(co_sysenv.env[Name] == "" || overwrite){
                        co_sysenv.env[Name] = std::move(Value);
                    }
                    return 0;
                }
            }
        }
        return Hook_setenv_t(n, value, overwrite);
    }

    int unsetenv(const char *n){
        HOOK_SYS_FUNC(unsetenv);
        if(RocketCo::GetCurrentCoIsHook() && co_sysenv.env.size()){
            RocketCo::Co_Entity* Co = RocketCo::GetCurrentCoEntity();
            if(Co != nullptr){
                if(!Co->Env){
                    Co->Env = static_cast<void*>(&co_sysenv);
                }
                std::string name(n);
                if(co_sysenv.env.find(name) != co_sysenv.env.end()){
                    co_sysenv.env[name] = "";
                    return 0;
                }
            }
        }
        return Hook_unsetenv_t(n);
    }

    char *getenv( const char *n ){
        HOOK_SYS_FUNC(getenv);

        if(RocketCo::GetCurrentCoIsHook() && co_sysenv.env.size()){
            RocketCo::Co_Entity* Co = RocketCo::GetCurrentCoEntity();
            if(Co != nullptr){
                if(!Co->Env){
                    Co->Env = static_cast<void*>(&co_sysenv);
                }
                std::string name(n);
                if(co_sysenv.env.find(name) != co_sysenv.env.end()){
                    // 算是一个弥补性的做法了
                    return const_cast<char*>(co_sysenv.env[name].data());
                    //return co_sysenv.env[name].data();
                }
            }
        }
        return Hook_getenv_t(n);
    }

    ssize_t sendto(int fd, const void *message, size_t length,
                   int flags, const struct sockaddr *dest_addr,
                   socklen_t dest_len){
        HOOK_SYS_FUNC(sendto);
        if(!RocketCo::GetCurrentCoIsHook()){
            return Hook_sendto_t(fd, message, length, flags, dest_addr, dest_len);
        }
        FdAttributes* Attributes = GetFdAttributesByFd(fd);
        if(!Attributes ||  Attributes->fdFlag & O_NONBLOCK){ // 证明套接字非阻塞,直接调用
            return Hook_sendto_t(fd, message, length, flags, dest_addr, dest_len);
        }
        ssize_t res = Hook_sendto_t(fd, message, length, flags, dest_addr, dest_len);
        // 因为套接字非阻塞, 检测下参数什么的是否可以接收报文
        if(res < 0 && errno == EAGAIN){
            int timeout = Attributes->wirte_timeout;
            struct pollfd event = {0};
            event.fd = fd;
            event.events = POLLOUT | POLLERR | POLLHUP;
            // 要发送的地址比较远可以把超时时间延长
            poll(&event, 1, timeout);
            res = Hook_sendto_t(fd, message, length, flags, dest_addr, dest_len);
        }
        return res;
    }

    ssize_t recvfrom(int fd, void *buffer, size_t length,
                      int flags, struct sockaddr *address,
                      socklen_t *address_len){
        HOOK_SYS_FUNC(recvfrom);
        if(!RocketCo::GetCurrentCoIsHook()){
            return Hook_recvfrom_t(fd, buffer, length, flags, address, address_len);
        }
        FdAttributes* Attributes = GetFdAttributesByFd(fd);
        if(!Attributes ||  Attributes->fdFlag & O_NONBLOCK){ // 证明套接字非阻塞,直接调用
            return Hook_recvfrom_t(fd, buffer, length, flags, address, address_len);
        }
        ssize_t res = Hook_recvfrom_t(fd, buffer, length, flags, address, address_len);
        int timeout = Attributes->wirte_timeout;
        struct pollfd event = {0};
        event.fd = fd;
        event.events = POLLIN | POLLERR | POLLHUP;
        poll(&event, 1, timeout);
        res = Hook_recvfrom_t(fd, buffer, length, flags, address, address_len);
        return res;
    }

    ssize_t send(int fd, const void *buffer, size_t length, int flags){
        HOOK_SYS_FUNC(send);
        if(!RocketCo::GetCurrentCoIsHook()){
            return Hook_send_t(fd, buffer, length, flags);
        }
        FdAttributes* Attributes = GetFdAttributesByFd(fd);
        if(!Attributes ||  Attributes->fdFlag & O_NONBLOCK){ // 证明套接字非阻塞,直接调用
            return Hook_send_t(fd, buffer, length, flags);
        }
        size_t wrotelen = 0;
        ssize_t writeret = Hook_send_t(fd, buffer, length, flags);
        // socket突然断开会出现返回0
        if (writeret == 0){
            return writeret;
        }
        if( writeret > 0 ){
            wrotelen += writeret;
        }
        while( wrotelen < length ){

            struct pollfd event = { 0 };
            event.fd = fd;
            event.events = ( POLLOUT | POLLERR | POLLHUP );
            poll( &event, 1, Attributes->wirte_timeout);

            writeret = Hook_send_t(fd,(const char*)buffer + wrotelen,length - wrotelen,flags );

            if( writeret <= 0 ) break;

            wrotelen += writeret ;
        }
        if (writeret <= 0 && wrotelen == 0){
            return writeret;
        }
        return wrotelen;
    }

    ssize_t recv(int fd, void *buffer, size_t length, int flags){
        HOOK_SYS_FUNC(recv);
        if(!RocketCo::GetCurrentCoIsHook()){
            return Hook_recv_t(fd, buffer, length, flags);
        }
        FdAttributes* Attributes = GetFdAttributesByFd(fd);
        if(!Attributes ||  Attributes->fdFlag & O_NONBLOCK){ // 证明套接字非阻塞,直接调用
            return Hook_recv_t(fd, buffer, length, flags);
        }
        struct pollfd event = { 0 };
        event.fd = fd;
        event.events = ( POLLOUT | POLLERR | POLLHUP );
        poll( &event, 1, Attributes->wirte_timeout);

        ssize_t res = Hook_recv_t(fd, buffer, length, flags);
        if(res < 0){
            std::cerr << "ERROR : error in recv, return value is lower than 0.\n";
        }
        return res;
    }

    // 包含了这个函数才会把hook.cpp中的内容链接到源程序中,从而完成hook
    void co_enable_hook_sys(){
        RocketCo::Co_Entity* Co = RocketCo::GetCurrentCoEntity();
        if(Co){
            Co->IsHook = true;
        }

    }