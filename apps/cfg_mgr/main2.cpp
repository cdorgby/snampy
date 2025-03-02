
#include <common/log.hpp>
#include <io/io.hpp>

using namespace io;

io_func<int> int_test(io::io_loop &io)
{
    LOG(info) << "Starting test";
    co_return 1;
}

io_func<std::string &> ref_test(io::io_loop &io)
{
    static std::string s = "Hello";
    co_return s;
}

io_func<std::string &> ref_test2(io::io_loop &io)
{
    static std::string s = "World";
    co_return s;
}

io_task void_test(io::io_loop &loop)
{
    LOG(info) << "Starting test";
    auto ret = co_await ref_test(loop);
    LOG(info) << "Returned: " << ret;
    auto ret2 = co_await ref_test2(loop);
    LOG(info) << "Returned 2: " << ret2;

    auto now = time_now();
    auto r  = io::detail::io_promise{loop, now + std::chrono::seconds(2)};
    auto r2 = io::detail::io_promise{loop, now + std::chrono::seconds(4)};

    auto p = io::detail::io_wait_for_any_promise{
        loop,
        time_now() + std::chrono::seconds(6),
        {&r, &r2}
    };

    auto ready = co_await p;
    LOG(info) << "Returned: " << ready.size();
    LOG(info) << "r: " << &r;
    LOG(info) << "r2: " << &r2;

    for (auto *waiter : ready)
    {
        auto ret = co_await *waiter;

        LOG(info) << "Waiter: " << waiter;
        LOG(info) << "Returned: " << to_int(ret);
    }
    {
        auto ret = co_await r2;
        LOG(info) << "Returned: " << to_int(ret);
    }

    co_return;
}

int main(int argc, char *argv[])
{

    io_loop io;

    auto a = void_test(io);

    io.schedule(std::move(void_test(io)), "test1");
    io.run();

    return 0;
}