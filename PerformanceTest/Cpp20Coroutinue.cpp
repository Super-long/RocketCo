#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <inttypes.h>
#include <stdint.h>
#include <vector>
#include <memory>
#include <iostream>
#include <unistd.h>

#include <coroutine>

#include <chrono>
#define CALC_CLOCK_T std::chrono::system_clock::time_point
#define CALC_CLOCK_NOW() std::chrono::system_clock::now()
#define CALC_MS_CLOCK(x) static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(x).count())
#define CALC_NS_AVG_CLOCK(x, y) static_cast<long long>(std::chrono::duration_cast<std::chrono::nanoseconds>(x).count() / (y ? y : 1))


static std::vector<std::pair<int*, std::coroutine_handle<> > > g_test_rpc_manager;

struct test_custom_coroutine_data;
struct test_custom_coroutine {
    using data_ptr = std::unique_ptr<test_custom_coroutine_data>;

    struct promise_type {
        data_ptr refer_data;
        char fake_cache_miss_[64 - sizeof(test_custom_coroutine_data*)];
        promise_type();

        static test_custom_coroutine get_return_object_on_allocation_failure();

        test_custom_coroutine get_return_object();

        std::suspend_always initial_suspend();

        std::suspend_always final_suspend();

        void unhandled_exception();

        // 用以支持 co_return
        void return_void();

        // 用以支持 co_yield
        std::suspend_always yield_value(test_custom_coroutine_data*&);
    };

    // 下面的接入用侵入式的方式支持 co_await test_rpc_generator
    // MSVC 目前支持使用非侵入式的方式实现，但是clang不支持
    bool await_ready() noexcept;

    void await_resume();

    void await_suspend(std::coroutine_handle<promise_type>);

    int resume();
    void set_sum_times(int);
    bool is_done() const;
    test_custom_coroutine_data* data();
private:
    test_custom_coroutine(test_custom_coroutine_data*);
    test_custom_coroutine_data* data_;
    char fake_cache_miss_[64 - sizeof(test_custom_coroutine_data*)];
};

struct test_custom_coroutine_data {
    int sum_times;
    int yield_times;

    std::coroutine_handle<test_custom_coroutine::promise_type> handle;
};

test_custom_coroutine::promise_type::promise_type() {
    refer_data = std::make_unique<test_custom_coroutine_data>();
    refer_data->sum_times = 0;
    refer_data->yield_times = 0;
}
test_custom_coroutine test_custom_coroutine::promise_type::get_return_object_on_allocation_failure() {
    return test_custom_coroutine{ nullptr };
}

test_custom_coroutine test_custom_coroutine::promise_type::get_return_object() {
    return test_custom_coroutine{ refer_data.get() };
}

std::suspend_always test_custom_coroutine::promise_type::initial_suspend() {
    refer_data->handle = std::coroutine_handle<promise_type>::from_promise(*this);
    return std::suspend_always{}; // STL提供了一些自带的awaiter实现，我们其实很多情况下也不需要另外写，直接用STL就好了
}

std::suspend_always test_custom_coroutine::promise_type::final_suspend() {
    return std::suspend_always{}; // 和上面一样，也是STL自带的awaiter实现
}

void test_custom_coroutine::promise_type::unhandled_exception() {
    std::terminate();
}

// 用以支持 co_return
void test_custom_coroutine::promise_type::return_void() {
    refer_data->handle = nullptr;
}

// 用以支持 co_yield
std::suspend_always test_custom_coroutine::promise_type::yield_value(test_custom_coroutine_data*& coro_data) {
    // 每次调用都会执行,创建handle用以后面恢复数据
    if (nullptr != refer_data) {
        refer_data->handle = std::coroutine_handle<promise_type>::from_promise(*this);
        ++refer_data->yield_times;
    }

    coro_data = refer_data.get();
    return std::suspend_always{};
}

// 下面的接入用侵入式的方式支持 co_await test_custom_coroutine , 实际上benchmark过程中并没有用到
// MSVC 目前支持使用非侵入式的方式实现，但是clang不支持
bool test_custom_coroutine::await_ready() noexcept {
    // 准备好地标志是协程handle执行完毕了
    return !data_ || !data_->handle || data_->handle.done();
}

void test_custom_coroutine::await_resume() {
    // do nothing when benchmark
}

void test_custom_coroutine::await_suspend(std::coroutine_handle<promise_type>) {
    // do nothing when benchmark
    // 被外部模块 co_await , 这里完整的协程任务链流程应该是要把coroutine_handle记录到test_custom_coroutine
    // 在return_void后需要对这些coroutine_handle做resume操作，但是这里为了减少benchmark的额外开销和保持干净
    // 所以留空
}

int test_custom_coroutine::resume() {
    if (!await_ready()) {
        data_->handle.resume();
        return 1;
    }
    return 0;
}

void test_custom_coroutine::set_sum_times(int times) {
    if (data_) {
        data_->sum_times = times;
    }
}

bool test_custom_coroutine::is_done() const {
    return !(data_ && data_->handle);
}

test_custom_coroutine_data* test_custom_coroutine::data() {
    return data_;
}

test_custom_coroutine::test_custom_coroutine(test_custom_coroutine_data* d) : data_(d) {}

// 异步协程函数
test_custom_coroutine coroutine_start_main(test_custom_coroutine_data*& coro_data) {
    // create done

    // begin to yield
    while (coro_data != nullptr && coro_data->yield_times < coro_data->sum_times) {
        co_yield coro_data;
    }

    // finish all yield
    co_return;
}

// 这里模拟生成数据
bool coroutine_resume(std::vector<test_custom_coroutine>& in, long long& real_switch_times) {
    bool ret = false;
    for (auto& co : in) {
        real_switch_times += co.resume();
        if (!co.is_done()) {
            ret = true;
        }
    }

    return ret;
}

int main(int argc, char* argv[]) {
#ifdef __cpp_coroutines
    std::cout << "__cpp_coroutines: " << __cpp_coroutines << std::endl;
#endif
    puts("###################### C++20 coroutine ###################");
    printf("########## Cmd:");
    for (int i = 0; i < argc; ++i) {
        printf(" %s", argv[i]);
    }
    puts("");

    int switch_count = 100;
    int max_coroutine_number = 100000; // 协程数量

    if (argc > 1) { // 创建的协程数量
        max_coroutine_number = atoi(argv[1]);
    }

    if (argc > 2) { // 切换的协程数量
        switch_count = atoi(argv[2]);
    }

    std::vector<test_custom_coroutine> co_arr;
    std::vector<test_custom_coroutine_data*> co_data_arr;

    co_arr.reserve(static_cast<size_t>(max_coroutine_number));
    co_data_arr.resize(static_cast<size_t>(max_coroutine_number), nullptr);

    time_t       begin_time = time(NULL);
    // CALC_CLOCK_T其实就是对于时间点的一个定义
    CALC_CLOCK_T begin_clock = CALC_CLOCK_NOW();

    // create coroutines
    // 相当于创建max_coroutine_number个协程，所有的协程会切换switch_count次
    for (int i = 0; i < max_coroutine_number; ++i) {
        co_arr.emplace_back(coroutine_start_main(co_data_arr[i]));
        co_arr.back().set_sum_times(switch_count); // 执行这里的时候还没有切换出去
        co_data_arr[i] = co_arr.back().data();
    }

    //sleep(5);
    time_t       end_time = time(NULL);
    CALC_CLOCK_T end_clock = CALC_CLOCK_NOW();
    // 创建max_coroutine_number个协程花费了多长时间，平均每一个协程创建花费的时间
    printf("create %d coroutine, cost time: %d s, clock time: %d ms, avg: %lld ns\n", max_coroutine_number,
        static_cast<int>(end_time - begin_time), CALC_MS_CLOCK(end_clock - begin_clock),
        CALC_NS_AVG_CLOCK(end_clock - begin_clock, max_coroutine_number));

    begin_time = end_time;
    begin_clock = end_clock;

    // yield & resume from runner
    long long real_switch_times = static_cast<long long>(0);

    bool is_continue = true;
    while (is_continue) {
        is_continue = coroutine_resume(co_arr, real_switch_times);
    }
    // sub create - resume
    real_switch_times -= max_coroutine_number;

    std::cout << "real_switch_times : " << real_switch_times << std::endl;

    end_time = time(NULL);
    end_clock = CALC_CLOCK_NOW();
    printf("switch %d coroutine contest %lld times, cost time: %d s, clock time: %d ms, avg: %lld ns\n", max_coroutine_number,
        real_switch_times, static_cast<int>(end_time - begin_time), CALC_MS_CLOCK(end_clock - begin_clock),
        CALC_NS_AVG_CLOCK(end_clock - begin_clock, real_switch_times));

    begin_time = end_time;
    begin_clock = end_clock;

    co_arr.clear();

    end_time = time(NULL);
    end_clock = CALC_CLOCK_NOW();
    printf("remove %d coroutine, cost time: %d s, clock time: %d ms, avg: %lld ns\n", max_coroutine_number,
        static_cast<int>(end_time - begin_time), CALC_MS_CLOCK(end_clock - begin_clock),
        CALC_NS_AVG_CLOCK(end_clock - begin_clock, max_coroutine_number));

    return 0;
}