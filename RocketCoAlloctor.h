#ifndef ROCKETCO_ALLOCTOR_H
#define ROCKETCO_ALLOCTOR_H

#include <memory>

namespace RocketCo{

    class RocketCoAlloctor {
        public:
            RocketCoAlloctor() = default;

            // 仅仅允许默认构造;
            RocketCoAlloctor(const RocketCoAlloctor&) = delete;
            RocketCoAlloctor&  operator=(const RocketCoAlloctor&) = delete;

            RocketCoAlloctor(RocketCoAlloctor&&) = delete;
            RocketCoAlloctor&  operator=(RocketCoAlloctor&&) = delete;
            
            char* CoAlloctorChar(size_t length) {
                return CoAlloctor.allocate(length);
            }

            void CoDestroy(char* ptr){
                CoAlloctor.destroy(ptr);
            }

        private:
            std::allocator<char> CoAlloctor;
    };

}

#endif