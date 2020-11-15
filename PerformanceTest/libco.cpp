#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <inttypes.h>
#include <stdint.h>
#include <vector>
#include <memory>

#include "co_routine.h"

#include <chrono>
#define CALC_CLOCK_T std::chrono::system_clock::time_point
#define CALC_CLOCK_NOW() std::chrono::system_clock::now()
#define CALC_MS_CLOCK(x) static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(x).count())
#define CALC_NS_AVG_CLOCK(x, y) static_cast<long long>(std::chrono::duration_cast<std::chrono::nanoseconds>(x).count() / (y ? y : 1))

class coroutine_context;

int switch_count = 100;
int max_coroutine_number = 100000; // 协程数量

class coroutine_context {
public:
    coroutine_context(const stCoRoutineAttr_t* libco_attr, int share_stack) {
        callee_ctx_ = NULL;
        share_stack_ = share_stack;
        // 注意要执行的参数为成员函数中的start_callback
        co_create(&callee_ctx_, libco_attr, &start_callback, this);

        is_in_callback_ = false;
        is_finished_ = false;
    }

    ~coroutine_context() {
        if (NULL != callee_ctx_) {
            co_release(callee_ctx_);
        }
    }

    void resume() {
        if (is_in_callback_) {
            return;
        }
        
        co_resume(callee_ctx_);
        // 能运行到这说明已经不在运行协程的函数了，当然可能还没有运行完
        is_in_callback_ = false;
    }

    void yield() {
        if(!is_in_callback_) {
            return;
        }

        // 把CPU执行权切换到调用栈的上一层
        co_yield(callee_ctx_);
        is_in_callback_ = true;
    }

    static void* start_callback(void* arg) {
        coroutine_context* this_coroutine = reinterpret_cast<coroutine_context*>(arg);
        this_coroutine->is_in_callback_ = true;
/* 
        // 可能需要占用一部分空间，测试共享栈的copy性能
        void* stack_buffer = NULL;
        if (this_coroutine->share_stack_ > 0) { // 共享栈选项打开
            stack_buffer = alloca(static_cast<size_t>(this_coroutine->share_stack_));
            memset(stack_buffer, 0, static_cast<size_t>(this_coroutine->share_stack_));
            memcpy(stack_buffer, this_coroutine, sizeof(coroutine_context));
            memcpy(static_cast<char*>(stack_buffer) + static_cast<size_t>(this_coroutine->share_stack_) - sizeof(coroutine_context), 
                this_coroutine, sizeof(coroutine_context));
        }
 */
        int count = switch_count; // 每个协程N次切换
        while (count-- > 0) {
            this_coroutine->yield();
        }

        this_coroutine->is_finished_ = true;
        this_coroutine->yield();

        return nullptr;
    }

    inline bool is_finished() const { return is_finished_; }

private:
    stCoRoutine_t* callee_ctx_;
    int share_stack_;
    bool is_in_callback_;
    bool is_finished_;
};

int main(int argc, char *argv[]) {
    puts("###################### ucontext coroutine ###################");
    printf("########## Cmd:");
    for (int i = 0; i < argc; ++i) {
        printf(" %s", argv[i]);
    }
    puts("");

    if (argc > 1) {
        max_coroutine_number = atoi(argv[1]);
    }

    if (argc > 2) {
        switch_count = atoi(argv[2]);
    }

    size_t stack_size = 16 * 1024;
    if (argc > 3) {
        stack_size = atoi(argv[3]) * 1024;
    }

    int enable_share_stack = 0;
    if (argc > 4) {
        enable_share_stack = atoi(argv[4]) * 1024;
    }

    stCoRoutineAttr_t libco_attr;
    libco_attr.stack_size = static_cast<int>(stack_size);
    if (0 != enable_share_stack) {
        libco_attr.share_stack = co_alloc_sharestack(1, libco_attr.stack_size);
    } else {
        // 协程数量个共享栈，最大程度减少协程切换时的开销
        libco_attr.share_stack = co_alloc_sharestack(max_coroutine_number, libco_attr.stack_size);
    }

    // create coroutines
    std::vector<std::unique_ptr<coroutine_context> > co_arr;
    co_arr.resize(static_cast<size_t>(max_coroutine_number));

    time_t       begin_time  = time(NULL);
    CALC_CLOCK_T begin_clock = CALC_CLOCK_NOW();

    // 降低局部性，且最大支持2^16个协程
    for (size_t i = 0; i < 64; ++ i) {
        for (size_t j = 0; i + j * 64 < co_arr.size(); ++ j) {
            co_arr[i + j * 64].reset(new coroutine_context(&libco_attr, enable_share_stack));
        }
    }

    time_t       end_time  = time(NULL);
    CALC_CLOCK_T end_clock = CALC_CLOCK_NOW();
    printf("create %d coroutine, cost time: %d s, clock time: %d ms, avg: %lld ns\n", max_coroutine_number,
           static_cast<int>(end_time - begin_time), CALC_MS_CLOCK(end_clock - begin_clock),
           CALC_NS_AVG_CLOCK(end_clock - begin_clock, max_coroutine_number));

    begin_time  = end_time;
    begin_clock = end_clock;

    // yield & resume from runner
    bool      continue_flag     = true;
    long long real_switch_times = static_cast<long long>(0);

    while (continue_flag) {
        continue_flag = false;
        for (int i = 0; i < max_coroutine_number; ++i) {
            // 局部性良好
            if (false == co_arr[i]->is_finished()) {
                continue_flag = true;
                ++real_switch_times;
                co_arr[i]->resume();
            }
        }
    }

    end_time  = time(NULL);
    end_clock = CALC_CLOCK_NOW();
    printf("switch %d coroutine contest %lld times, cost time: %d s, clock time: %d ms, avg: %lld ns\n", max_coroutine_number,
           real_switch_times, static_cast<int>(end_time - begin_time), CALC_MS_CLOCK(end_clock - begin_clock),
           CALC_NS_AVG_CLOCK(end_clock - begin_clock, real_switch_times));

    begin_time  = end_time;
    begin_clock = end_clock;

    co_arr.clear();

    end_time  = time(NULL);
    end_clock = CALC_CLOCK_NOW();
    printf("remove %d coroutine, cost time: %d s, clock time: %d ms, avg: %lld ns\n", max_coroutine_number,
           static_cast<int>(end_time - begin_time), CALC_MS_CLOCK(end_clock - begin_clock),
           CALC_NS_AVG_CLOCK(end_clock - begin_clock, max_coroutine_number));

    return 0;
}