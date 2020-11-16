# RocketCo

## 压力测试机环境
| 环境名称 | 值 | 
:-----:|:-----:|
系统|5.9.6-arch1-1|
处理器|4 x Intel® Core i5-7200U CPU @ 2.50GHz |
L1 Cache|32KB|
逻辑核数|4
系统负载|2.17, 2.19, 2.24|
内存占用|5271MB(used)/2131MB(cached)/337MB(free)
Cmake版本|3.18.4|
GCC版本|10.2.0|
Golang版本|1.15.4 |

所有测试数据均为5次的平均值：

| 不同的协程实现| 协程数：1000 创建开销 | 协程数1000 切换开销| 协程数：5000 创建开销 | 协程数5000 切换开销|  
:-----:|:-----:|:-----:|:-----:|:-----:|
|C++20 Coroutinue - GCC|2649ns|173ns|2694ns|134ns|
|Goroutinue|298ns|462ns| 246ns|542ns|
|libgo 2020版本|20904ns|90ns|10062ns|104ns|
|libco静态栈|7714ns| 223ns| 1385ns| 278ns|
|libco共享栈|15554ns|169ns| 8377ns|199ns|
|RocketCo静态栈|5317ns|242ns|1088ns|313ns|
|RocketCo共享栈|7873ns|193ns|2828ns|187ns|


### C++20Coroutinue

测试代码位于/PerformanceTest/Cpp20Coroutinue.cpp,运行该代码请确保您的GCC版本大于等于10.2.0。运行测试代码需要执行以下命令：
```
g++ Cpp20Coroutinue.cpp  -std=c++20 -O0 -g -ggdb -fcoroutines -pthread ; ./a.out 1000 1000
```

第一个参数为协程数;第二个每个协程的切换次数。

ps：不知道是GCC实现的原因还是我测试的原因，C++20的Corutinue在我的机器上测出来的数据远远逊色于网上其他朋友使用Clang和MSVC编译的版本。

### Goroutinue

测试代码位于/PerformanceTest/Goroutinue.go，运行该代码请确保您的GOROOT与GOPATH设置正确。

运行测试代码需要执行以下命令：
```
go run Goroutinue.go  1000 1000 
```

第一个参数为协程数;第二个每个协程的切换次数。

### libco

libco版本为cc46d60e6fa4e69c33a7280b1a94532a87bd787e;

测试代码位于/PerformanceTest/libco.cpp，这个代码并不能直接运行，需要把libco拉到本地以后放到libco中执行，

运行测试代码需要执行以下命令：
```
git clone https://github.com/Tencent/libco.git;
cd libco;
git revert cc46d60e6fa4e69c33a7280b1a94532a87bd787e;
mkdir build && cd build;
cmake ..;
make;
cd ..;
g++ -O2 -g -DNDEBUG -ggdb -Wall -Werror -fPIC libco.cpp build/libcolib.a -o TestLibco -Ilibco -lpthread -lz -lm -ldl ;

# static stack
./TestLibco 1000 1000 128
./TestLibco 5000 1000 128

# shared stack
./TestLibco 1000 1000 128 32
./TestLibco 5000 1000 128 32
```
第一个参数为协程数;第二个每个协程的切换次数;第三个参数为栈的大小;第四个栈为打开共享栈选项，共享栈数量为协程数。

### libgo
测试代码位于/PerformanceTest/libgo.cpp，这个代码并不能直接运行，需要把libgo拉到本地以后[编译安装](https://gitee.com/yyzybb537/libgo)以后执行以下指令：
```
mkdir build && cd build;
cmake ..;
make;
cd ..;

g++ -O2 -g -DNDEBUG -ggdb -Wall -Werror -fPIC libgo.cpp -o libgo  -Ilibgo -Ilibgo/libgo -Ilibgo/libgo/linux -Lbuild -llibgo -lrt -lpthread -ldl;

./libgo 1000 1000 128;
./libgo 5000 100 128;
```
