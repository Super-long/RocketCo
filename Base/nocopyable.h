//
// Created by lizhaolong on 2020/6/8.
//

#ifndef ROCKETCO_NOCOPYABLE_H
#define ROCKETCO_NOCOPYABLE_H

namespace RocketCo{
    // 继承这个类的类都不允许拷贝
    class Nocopy{
    public:
        Nocopy(const Nocopy&) = delete;
        Nocopy &operator=(const Nocopy& para) = delete;
    protected:
        constexpr Nocopy() = default;
        virtual ~Nocopy() = default;
    };
}

#endif //ROCKETCO_NOCOPYABLE_H
