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
