//
// Created by lizhaolong on 2020/6/15.
//

#include "../Co_Routine.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>

#include <bits/stdc++.h>
using namespace std;

int ServerFd = -1;

struct task_t
{
    RocketCo::Co_Entity* co;
    int fd;
};

stack<task_t*> g_readwrite;

void *readwrite_routine( void *arg ){
    co_enable_hook_sys();

    task_t *co = (task_t*)arg;
    char buf[ 1024 * 16 ];
    for(;;)
    {
        if( -1 == co->fd ) // 显然第一次循环的时候都会运行这里
        {
            g_readwrite.push( co );
            RocketCo::Co_yeild();
            continue;
        }

        int fd = co->fd;
        co->fd = -1;

        for(;;)
        {
            struct pollfd pf = { 0 };
            pf.fd = fd;
            pf.events = (POLLIN|POLLERR|POLLHUP);
            RocketCo::Co_poll_inner( RocketCo::GetCurrentCoEpoll(),&pf,1,1000);

            int ret = read( fd,buf,sizeof(buf) );
            if( ret > 0 ) // 一个回显的过程,把接收到的东西再写回去,当然零拷贝是一个更优的选择
            {
                ret = write( fd,buf,ret );
            }
            if( ret > 0 || ( -1 == ret && EAGAIN == errno ) )
            {
                continue;
            }
            close( fd );
            break;
        }

    }
    return 0;
}

int SetNonBlock(int iSock);
int AcceptFd2Attributes( int fd, struct sockaddr *addr, socklen_t *len );
void *accept_routine( void * )
{
    co_enable_hook_sys();
    printf("accept_routine\n");
    fflush(stdout);
    for(;;)
    {
        //printf("pid %ld g_readwrite.size %ld\n",getpid(),g_readwrite.size());
        if( g_readwrite.empty() )
        {
            printf("empty\n"); //sleep
            struct pollfd pf = { 0 };
            pf.fd = -1;
            poll( &pf,1,1000); // 转让CPU执行1s

            continue;
        }
        struct sockaddr_in addr; //maybe sockaddr_un;
        memset( &addr,0,sizeof(addr) );
        socklen_t len = sizeof(addr);

        // 接收连接
        int fd = AcceptFd2Attributes(ServerFd, (struct sockaddr *)&addr, &len);
        if( fd < 0 )
        {
            struct pollfd pf = { 0 };
            pf.fd = ServerFd;
            pf.events = (POLLIN|POLLERR|POLLHUP);
            RocketCo::Co_poll_inner( RocketCo::GetCurrentCoEpoll(),&pf,1,1000 );
            continue;
        }
        if( g_readwrite.empty() )
        {
            close( fd );
            continue;
        }
        SetNonBlock( fd );
        // 接收成功的话唤醒目前此线程内正在等待的协程去处理这个事件
        task_t *co = g_readwrite.top();
        co->fd = fd;
        g_readwrite.pop();
        RocketCo::Co_resume( co->co );
    }
    return 0;
}

int SetNonBlock(int iSock)
{
    int iFlags;

    iFlags = fcntl(iSock, F_GETFL, 0);
    iFlags |= O_NONBLOCK;
    iFlags |= O_NDELAY;
    int ret = fcntl(iSock, F_SETFL, iFlags);
    return ret;
}

void SetAddr(const char *pszIP,const unsigned short shPort,struct sockaddr_in &addr)
{
    bzero(&addr,sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(shPort);
    int nIP = 0;
    if( !pszIP || '\0' == *pszIP
        || 0 == strcmp(pszIP,"0") || 0 == strcmp(pszIP,"0.0.0.0")
        || 0 == strcmp(pszIP,"*")
            ){
        nIP = htonl(INADDR_ANY);
    }
    else{
        nIP = inet_addr(pszIP);
    }
    addr.sin_addr.s_addr = nIP;
}

int CreateASocket(const char* ip, int port, bool IsResue){
    int fd = socket(AF_INET,SOCK_STREAM, IPPROTO_TCP);
    if(fd >= 0){
        if(port != 0){
            if(IsResue){
                int nReuseAddr = 1;
                setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,&nReuseAddr,sizeof(nReuseAddr));
            }
            struct sockaddr_in addr;
            SetAddr(ip,port,addr);
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

// ./echoServer [IP] [PORT] [TASK_COUNT] [PROCESS_COUNT]
int main(int argc, char* argv[]){
    if(argc<5){
        printf("Usage:\n"
               "example_echosvr [IP] [PORT] [TASK_COUNT] [PROCESS_COUNT]\n"
               "example_echosvr [IP] [PORT] [TASK_COUNT] [PROCESS_COUNT] -d   # daemonize mode\n");
        return -1;
    }
    const char *ip = argv[1];		// IP
    int port = atoi( argv[2] );		// 端口
    int cnt = atoi( argv[3] );		// 每个进程中的协程数
    int proccnt = atoi( argv[4] );	// 进程数
    bool deamonize = argc >= 6 && strcmp(argv[5], "-d") == 0;

    ServerFd = CreateASocket(ip, port, true);

    listen(ServerFd, 8096);
    printf("listen %d %s:%d\n",ServerFd,ip,port);
    SetNonBlock(ServerFd);

    for(int i = 0; i < proccnt; ++i) {
        pid_t pid = fork();
        if(pid > 0){ // 父进程
            continue;
        }else if (pid < 0){ // 出现错误
            break;
        }else { //等于0,子进程
            for(int j = 0; j < cnt; j++){
                task_t * task = (task_t*)calloc( 1,sizeof(task_t) );
                task->fd = -1;

                RocketCo::Co_Create(&(task->co),NULL,readwrite_routine,task );
                RocketCo::Co_resume(task->co);
            }
            RocketCo::Co_Entity *accept_co = NULL;
            RocketCo::Co_Create( &accept_co,NULL,accept_routine,0 );
            RocketCo::Co_resume( accept_co );

            RocketCo::EventLoop(RocketCo::GetCurrentCoEpoll(),0,0 );

            printf("end\n"); // 永远无法执行到这里
        }
    }
}