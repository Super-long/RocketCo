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

#ifndef ROCKETCO_EPOLL_H
#define ROCKETCO_EPOLL_H


#include "epoll_event_result.h"
#include "../Base/nocopyable.h"
#include "../Base/havefd.h"

#include <sys/epoll.h>

namespace RocketCo{

    class Epoll final : public Nocopy,Havefd{
        public:
            Epoll() : epoll_fd_(epoll_create1(::EPOLL_CLOEXEC)) {}

            int Add(EpollEvent& para){
                return epoll_ctl(epoll_fd_, EPOLL_CTL_ADD,para.Return_fd(),para.Return_Pointer());
            }
            int Add(EpollEvent&& para){
                return epoll_ctl(epoll_fd_, EPOLL_CTL_ADD,para.Return_fd(),para.Return_Pointer());
            }
            int Add(const Havefd& Hf,EpollEventType ETT){
                return Add({Hf,ETT});
            }

            int Modify(EpollEvent& para){
                return epoll_ctl(epoll_fd_, EPOLL_CTL_MOD,para.Return_fd(),para.Return_Pointer());
            }
            int Modify(EpollEvent&& para){
                return epoll_ctl(epoll_fd_, EPOLL_CTL_MOD,para.Return_fd(),para.Return_Pointer());
            }
            int Modify(const Havefd& Hf,EpollEventType ETT){
                return Modify({Hf,ETT});
            }

            int Remove(EpollEvent& para){
                return epoll_ctl(epoll_fd_, EPOLL_CTL_DEL,para.Return_fd(),para.Return_Pointer());
            }
            int Remove(EpollEvent&& para){
                return epoll_ctl(epoll_fd_, EPOLL_CTL_DEL,para.Return_fd(),para.Return_Pointer());
            }
            int Remove(const Havefd& Hf,EpollEventType ETT){
                return Remove({Hf,ETT});
            }

            void Epoll_Wait(EpollEvent_Result& ETT){
                Epoll_Wait(ETT,-1);
            }

            void Epoll_Wait(EpollEvent_Result& ETT,int timeout){
                int Available_Event_Number_ =
                        epoll_wait(epoll_fd_,reinterpret_cast<epoll_event*>(ETT.array.get()),ETT.All_length,timeout);
                ETT.Available_length = Available_Event_Number_;
            }
            int fd() const override {return epoll_fd_; }

        private:
            int epoll_fd_;
    };

}

#endif //ROCKETCO_EPOLL_H