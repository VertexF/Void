#ifndef FOUNDATION_THREADING_THREAD_HDR
#define FOUNDATION_THREADING_THREAD_HDR

#include <thread>
#include <utility>

namespace Engine::Threading {

using ThreadId = std::thread::id;

class Thread {
public:
    Thread() = default;
    ~Thread()
    {
        if (IsJoinable()) {
            Join();
        }
    }

    Thread(const Thread&) = delete;
    Thread& operator=(const Thread&) = delete;
    Thread(Thread&&) noexcept = default;
    Thread& operator=(Thread&&) noexcept = default;

    template<typename Fn>
    void Start(Fn&& fn)
    {
        if (IsJoinable()) {
            Join();
        }
        m_thread = std::thread(std::forward<Fn>(fn));
    }

    [[nodiscard]] bool IsJoinable() const noexcept
    {
        return m_thread.joinable();
    }

    void Join()
    {
        if (m_thread.joinable()) {
            m_thread.join();
        }
    }

    void Detach()
    {
        if (m_thread.joinable()) {
            m_thread.detach();
        }
    }

    [[nodiscard]] ThreadId GetId() const noexcept
    {
        return m_thread.get_id();
    }

    static void Yield() noexcept
    {
        std::this_thread::yield();
    }

private:
    std::thread m_thread;
};

inline void YieldCpu() noexcept
{
    std::this_thread::yield();
}

} // namespace Engine::Threading

#endif // FOUNDATION_THREADING_THREAD_HDR
