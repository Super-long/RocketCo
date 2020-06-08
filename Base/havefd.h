//
// Created by lizhaolong on 2020/6/8.
//

#ifndef ROCKETCO_HAVEFD_H
#define ROCKETCO_HAVEFD_H

namespace RocketCo{
    class Havefd{
    public:
        virtual int fd() const = 0;

        bool operator==(const Havefd& para){
            return fd() == para.fd();
        }
    };
}

#endif //ROCKETCO_HAVEFD_H