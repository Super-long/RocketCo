//
// Created by lizhaolong on 2020/6/8.
//

#ifndef ROCKETCO_EPOLL_EVENT_RESULT_H
#define ROCKETCO_EPOLL_EVENT_RESULT_H

#include "../Base/nocopyable.h"
#include "epoll_event.h"
#include <stdexcept>

#include <memory>

namespace RocketCo{

    // 用于在Eventloop中指定一次epollwait的最大事件数量
    constexpr const int DefaultEpollEventSize(){
        return 1024*1024;
    }

    class EpollEvent_Result final : public Nocopy{
        friend class Epoll;
        public:
            explicit EpollEvent_Result(int len) :
                    array(new EpollEvent[len]),Available_length(0),All_length(len){}

            size_t size() {return Available_length; }
            EpollEvent& at(size_t i){
                if(i > Available_length){
                    throw std::out_of_range("'EpollEvent_Result : at' : Out of bounds.");
                }else{
                    return array[i];
                }
            }
            const EpollEvent& operator[](size_t i) const{
                if(i > Available_length) throw std::out_of_range("'EpollEvent_Result : []' Out of Bounds.");
                return array[i];
            }
            EpollEvent& operator[](size_t i){
                return const_cast<EpollEvent&>(
                        static_cast<const EpollEvent_Result&>(*this)[i]
                );
            }
            ~EpollEvent_Result(){}
        private:
            std::unique_ptr<EpollEvent[]> array;
            size_t Available_length;
            size_t All_length;
    };
}

#endif //ROCKETCO_EPOLL_EVENT_RESULT_H
