# RocketCo
# 1.介绍
RocketCo是基于x86/x86_64架构，运行于Linux操作系统的一个非对称，有栈的协程库，采用C++实现。


1. 支持静态栈和共享栈，供用户在不同场合选择不同的策略以获得更优的性能。 
2. 仿POSIX线程接口的协程编程接口，用户使用难度低。
3. 对于fd属性的管理采用享元模式，最大程度减少对象的构造，以提升性能。
4. 超时事件通知提供单轮时间轮与多轮时间轮的实现，供用户在不同场合选择不同的策略以获得更优的性能。
5. 借助epoll存储数据，调度器异步调用回调，时间复杂度为O(1)，意味着就本线程来说用户态调度将比内核的调度策略更优。
6. 同一线程创建的协程无数据竞争，意味着设计没有问题的前提下可以在逻辑处理中去除锁，原子操作，内存序等同步手段，以提升性能。
7. 采用高性能内存池优化内存分配，提升共享栈下协程切换速度。

# 2.使用
编译RocketCo需要引入[gperftools](https://github.com/gperftools/gperftools)工具。
```
./autogen.sh
./configure
make -j8
sudo make install
ldconfig 刷新动态库文件
```
arch玩家直接pacman即可安装成功。

这里安装完成以后会出现一个问题，就是gperftools产生的动态库在/usr/local/lib中，而这并不在系统动态库的默认查找路线。一种可行的方法是修改/etc/ld.so.conf，在文件尾部加入/usr/local/lib。

最好先写个测试文件看看是否安装成功。


gperftools安装完成以后进行以下步骤即可执行测试样例。

```
mkdir build && cd build;
cmake ..;
make;
```

在example提供多个测试样例，默认生成的测试样例为example/ProductConsumers.cpp，想切换样例的时候修改CMakeLists.txt第18行的样例文件名即可。

## 3. 内存池相关 
代码存放在/RocketCoMemoryPool中，其中RocketMemoryPoolPerformance.pdf为性能分析图。

RocketCo的内存池是我参考std::allocator,在其之上针对于协程栈切换的数据分布进行了特殊的数据块设计，并参考buddy system引入了相邻内存块合并，后者是目前市面上所看到用户级分配器都不支持的。
但是因为实现的原因，在只malloc而不free的情况下RocketCo内存池的分配速度仅有标准库的百分之六十，这其实是不太理想的。

经过性能分析以后发现性能开销的百分之八十都集中在对于合并的操作上，这里不仅仅指free中的合并，还有malloc时对合并做出的一些前提操作。而这些地方基本在不去除功能的情况下基本没有修改的余地，所以要实现一个高性能的内存池就现在看来最好不要引入合并功能。

所以再三抉择下引入google的tcmalloc作为RocketCo的内存池。

## 4. 测试相关

### 压力测试机环境
| 环境名称 | 值 | 
:-----:|:-----:|
系统|5.9.6-arch1-1|
处理器|4 x Intel® Core i5-7200U CPU @ 2.50GHz |
L1 Cache|32KB|
逻辑核数|4|
系统负载|2.17, 2.19, 2.24|
内存占用|5271MB(used)/2131MB(cached)/337MB(free)
Cmake版本|3.18.4|
GCC版本|10.2.0|
Golang版本|1.15.4 |

### 测试数据

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

其实这个结果基本在意料之中。

首先是网上流传libco的切换是goroutinue的三倍，这个在我机器上看来其实是符合预期的。

其次是C++20的Coroutinue，采用无栈协程实现，理论来说切换应该是最快的，但是就测试看来其实低于libgo的切换速度，不知是否是因为编译器的原因，因为网上其他的测试基本采用Clang和MSVC，GCC是今年，即2020年7月23日发布的10.2.0版本才支持的，目前我还没有看到其他人使用GCC测试C++20Coroutinue。

然后就是libgo，这个我不暂时不太了解其实现原理，后面再补充。

其次就是libco与RocketCo的差异，因为RocketCo本就是基于libco实现的。所以就数据看来基本符合预期，RocketCo在创建上甩开了libco三条街，且在多协程共享栈模式时在切换上有优势;其它时候RocketCo稍逊色与libco，这个可能是我实现上的问题，因为在很多地方的逻辑进行了重写，且加入了一些判断，所以出现这种情况是正常的。

#### C++20Coroutinue

测试代码位于/PerformanceTest/Cpp20Coroutinue.cpp,运行该代码请确保您的GCC版本大于等于10.2.0。运行测试代码需要执行以下命令：
```
g++ Cpp20Coroutinue.cpp  -std=c++20 -O0 -g -ggdb -fcoroutines -pthread ; ./a.out 1000 1000
```

第一个参数为协程数;第二个每个协程的切换次数。

ps：不知道是GCC实现的原因还是我测试的原因，C++20的Corutinue在我的机器上测出来的数据远远逊色于网上其他朋友使用Clang和MSVC编译的版本。

#### Goroutinue

测试代码位于/PerformanceTest/Goroutinue.go，运行该代码请确保您的GOROOT与GOPATH设置正确。

运行测试代码需要执行以下命令：
```
go run Goroutinue.go  1000 1000 
```

第一个参数为协程数;第二个每个协程的切换次数。

#### libco

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

#### libgo
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