//
// Created by lizhaolong on 2020/6/17.
//

#include "../Co_Routine.h"

#include <stdio.h>
#include <stdlib.h>

#include <iostream>

#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <errno.h>
#include <vector>
#include <unordered_set>
#include <unistd.h>

// 测试主线程调用使用带有co_enable_hook_sys的函数是否工作正常

struct task_t
{
    RocketCo::Co_Entity* co;// 协程的主结构体
    int fd;					// fd
    struct sockaddr_in addr;// 目标地址
};

static int SetNonBlock(int iSock)
{
    int iFlags;

    iFlags = fcntl(iSock, F_GETFL, 0);
    iFlags |= O_NONBLOCK;
    iFlags |= O_NDELAY;
    int ret = fcntl(iSock, F_SETFL, iFlags);
    return ret;
}

static void SetAddr(const char *pszIP,const unsigned short shPort,struct sockaddr_in &addr)
{
    bzero(&addr,sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(shPort);
    int nIP = 0;
    if( !pszIP || '\0' == *pszIP
        || 0 == strcmp(pszIP,"0") || 0 == strcmp(pszIP,"0.0.0.0")
        || 0 == strcmp(pszIP,"*")
            )
    {
        nIP = htonl(INADDR_ANY);
    }
    else
    {
        nIP = inet_addr(pszIP);
    }
    addr.sin_addr.s_addr = nIP;

}

static int CreateTcpSocket(const unsigned short shPort  = 0 ,const char *pszIP  = "*" ,bool bReuse  = false )
{
    int fd = socket(AF_INET,SOCK_STREAM, IPPROTO_TCP);
    if( fd >= 0 )
    {
        if(shPort != 0)
        {
            if(bReuse)
            {
                int nReuseAddr = 1;
                setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,&nReuseAddr,sizeof(nReuseAddr));
            }
            struct sockaddr_in addr ;
            SetAddr(pszIP,shPort,addr);
            int ret = bind(fd,(struct sockaddr*)&addr,sizeof(addr));
            if( ret != 0)
            {
                close(fd);
                return -1;
            }
        }
    }
    return fd;
}

void* UsingPoll(void* arg){
    co_enable_hook_sys();

    std::vector<task_t> &v = *(std::vector<task_t>*)arg;
    for(size_t i=0;i<v.size();i++)
    {
        int fd = CreateTcpSocket();	// 创建一个基于TCP,IP的套接字
        SetNonBlock( fd );			// 设置成非阻塞的
        v[i].fd = fd;

        // N个任务分别建立连接
        int ret = connect(fd,(struct sockaddr*)&v[i].addr,sizeof( v[i].addr ));
        printf("Routine %p, connect i %ld ret %d errno %d (%s)\n",
               RocketCo::GetCurrentCoEntity(),i,ret,errno,strerror(errno));
    }
    struct pollfd *pollfdevent = (struct pollfd*)calloc( v.size(),sizeof(struct pollfd));

    for(size_t i=0;i<v.size();i++){
        pollfdevent[i].fd = v[i].fd;
        pollfdevent[i].events = ( POLLOUT | POLLERR | POLLHUP );
    }
    std::unordered_set<int> setRaiseFds;
    size_t EventLength = v.size();

    int ret = poll(pollfdevent, EventLength, 1000);
    printf("Routine %p, poll return %d.\n", RocketCo::GetCurrentCoEntity(), ret);

    for (int i = 0; i < EventLength; ++i) {
        printf("Routine %p, POLLOUT : %d, POLLERR : %d, POLLHUP : %d.\n",
                RocketCo::GetCurrentCoEntity(), ret&POLLOUT, ret&POLLERR, ret&POLLHUP);
/*        printf("co %p fire fd %d revents 0x%X POLLOUT 0x%X POLLERR 0x%X POLLHUP 0x%X\n",
               RocketCo::GetCurrentCoEntity(),
               pollfdevent[i].fd,
               pollfdevent[i].revents,
               POLLOUT,
               POLLERR,
               POLLHUP
        );*/
    }

    for(size_t i = 0; i < v.size(); i++){
        close(v[i].fd);
        v[i].fd = -1;
    }
    return 0;
}

int main(int argc, char* argv[]){
    std::vector<task_t> v1;
    for(int i = 1; i < argc; i += 2){
        task_t task = { 0 };
        SetAddr( argv[i],atoi(argv[i+1]),task.addr );
        v1.push_back( task );
    }
    std::vector<task_t> v2 = v1;

    UsingPoll(&v2);

    for (int j = 0; j < 5; ++j) {
        RocketCo::Co_Entity* Co;
        std::vector<task_t> *v3 = new std::vector<task_t>();
        *v3 = v1;
        RocketCo::Co_Create(&Co, nullptr, UsingPoll, v3);
        RocketCo::Co_resume(Co);
    }

    RocketCo::EventLoop(RocketCo::GetCurrentCoEpoll(), 0, 0);
    return 0;
}
// ./RocketCo 127.0.0.1 12365 127.0.0.1 12222 192.168.1.1 1000 192.168.1.2 1111