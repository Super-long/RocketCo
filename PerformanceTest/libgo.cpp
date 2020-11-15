#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <inttypes.h>
#include <stdint.h>
#include <vector>
#include <memory>

#include "libgo/coroutine.h"

#include <chrono>
#define CALC_CLOCK_T std::chrono::system_clock::time_point
#define CALC_CLOCK_NOW() std::chrono::system_clock::now()
#define CALC_MS_CLOCK(x) static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(x).count())
#define CALC_NS_AVG_CLOCK(x, y) static_cast<long long>(std::chrono::duration_cast<std::chrono::nanoseconds>(x).count() / (y ? y : 1))

int switch_count = 100;
int max_coroutine_number = 100000; // 协程数量

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

    time_t       begin_time  = time(NULL);
    CALC_CLOCK_T begin_clock = CALC_CLOCK_NOW();

    // create coroutines
    int finish_count = 0;
    for (int i = 0; i < max_coroutine_number; ++ i) {
        go_stack(stack_size) [&finish_count]{
            int left_count = switch_count;
            while (left_count -- > 0) {
                co_yield;
            }
            
            ++ finish_count;
            if (finish_count >= max_coroutine_number) {
                co_sched.Stop();
            }
        };
    }

    time_t       end_time  = time(NULL);
    CALC_CLOCK_T end_clock = CALC_CLOCK_NOW();
    printf("create %d coroutine, cost time: %d s, clock time: %d ms, avg: %lld ns\n", max_coroutine_number,
           static_cast<int>(end_time - begin_time), CALC_MS_CLOCK(end_clock - begin_clock),
           CALC_NS_AVG_CLOCK(end_clock - begin_clock, max_coroutine_number));

    begin_time  = end_time;
    begin_clock = end_clock;

    // yield & resume from runner
    co_sched.Start();   // libgo中的协程创建以后不执行，需要通过调度器执行
    long long real_switch_times = max_coroutine_number * static_cast<long long>(switch_count);

    end_time  = time(NULL);
    end_clock = CALC_CLOCK_NOW();
    printf("switch %d coroutine contest %lld times, cost time: %d s, clock time: %d ms, avg: %lld ns\n", max_coroutine_number,
           real_switch_times, static_cast<int>(end_time - begin_time), CALC_MS_CLOCK(end_clock - begin_clock),
           CALC_NS_AVG_CLOCK(end_clock - begin_clock, real_switch_times));

    begin_time  = end_time;
    begin_clock = end_clock;

    end_time  = time(NULL);
    end_clock = CALC_CLOCK_NOW();
    printf("remove %d coroutine, cost time: %d s, clock time: %d ms, avg: %lld ns\n", max_coroutine_number,
           static_cast<int>(end_time - begin_time), CALC_MS_CLOCK(end_clock - begin_clock),
           CALC_NS_AVG_CLOCK(end_clock - begin_clock, max_coroutine_number));

    return 0;
}