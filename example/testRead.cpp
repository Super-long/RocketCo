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