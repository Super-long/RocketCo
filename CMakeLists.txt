cmake_minimum_required(VERSION 3.11)
project(RocketCo)

set(CMAKE_CXX_STANDARD 14)
 
enable_language(C ASM)
link_libraries(-ldl) # 防止dlsym出现未定义的引用
link_libraries(-pthread)
SET(CMAKE_C_FLAGS ${CMAKE_CXX_FLAGS} -g -fno-strict-aliasing -O2 -std=gnu9x -Wall -export-dynamic -pipe  -D_GNU_SOURCE -D_REENTRANT -fPIC -Wno-deprecated -m64)
#SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2")
#SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=gnu++14")

#SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-builtin-read")
#SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-builtin-write")

add_executable(RocketCo example/SimpleRoutineExample.cpp
        hook.cpp
        coswap.h coswap.cpp coctx_swap.S
        Co_Entity.h Co_Routine.h Co_Routine.cpp
        Base/havefd.h Base/copyable.h Base/nocopyable.h
        EpollWrapper/address.h EpollWrapper/address.cpp
        EpollWrapper/epoll_event.h EpollWrapper/epoll_event_result.h EpollWrapper/epoll.h)