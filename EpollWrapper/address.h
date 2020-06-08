//
// Created by lizhaolong on 2020/6/8.
//

#ifndef ROCKETCO_ADDRESS_H
#define ROCKETCO_ADDRESS_H

#include "../Base/copyable.h"

#include <arpa/inet.h>

namespace RocketCo{
    class Address : public Copyable{
        public:
            explicit Address(const struct sockaddr_in& para) : addr_(para){}
            Address(const char* IP, int port);
            explicit Address(int port);

            // 这里没办法直接转换,需要切换以下void*
            const sockaddr* Return_Pointer(){return static_cast<const sockaddr*>(static_cast<void*>(&addr_));}
            constexpr size_t Return_length(){return sizeof(struct sockaddr_in);}
        private:
            struct sockaddr_in addr_;
    };
}

#endif //ROCKETCO_ADDRESS_H
