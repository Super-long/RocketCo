# RocketCo

## 压力测试机n环境
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

### C++20Coroutinue

测试代码位于/PerformanceTest/,运行该代码请确保您的GCC版本大于等于10.2.0。运行测试代码需要执行以下命令：
```
g++ Cpp20Coroutinue.cpp  -std=c++20 -O0 -g -ggdb -fcoroutines -pthread ; ./a.out
```

| C++20 Coroutinue - GCC| 协程数：1000 创建开销 | 协程数1000 切换开销| 协程数：5000 创建开销 | 协程数5000 切换开销|  
:-----:|:-----:|:-----:|:-----:|:-----:|
||815ns|251ns|2694ns|146ns


不知道是GCC实现的原因还是我测试的原因，在我的机器上测出来的数据远远逊色于网上其他朋友使用Clang和MSVC编译的版本。

