cmake_minimum_required(VERSION 3.15)
project(RocketCo)

set(CMAKE_CXX_STANDARD 14)
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=gnu++14")
add_executable(RocketCo main.cpp
        coswap.h coswap.cpp
        Co_Entity.h Co_Routine.h Co_Routine.cpp
        Base/havefd.h Base/copyable.h Base/nocopyable.h
        EpollWrapper/address.h EpollWrapper/address.cpp
        EpollWrapper/epoll_event.h EpollWrapper/epoll_event_result.h EpollWrapper/epoll.h hook.cpp)