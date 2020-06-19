//
// Created by lizhaolong on 2020/6/17.
//

#include <sys/socket.h>
#include "../Co_Routine.h"
#include <unistd.h>

int main(){
    // TODO 有bug, 为什么read,write,close没有办法hook呢?其他函数都可以啊,而且写的样例也可以hook啊
    co_enable_hook_sys();
    char buffer[10000];
    int fd = socket(PF_INET, SOCK_STREAM, 0);
    read(5, buffer, 10000);
    write(5, buffer, 10000);
    close(fd);
    return 0;
}