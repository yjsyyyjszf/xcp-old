#if !defined(_XCP_INFRA_SIGHANDLE_H_INCLUDED_)
#define _XCP_INFRA_SIGHANDLE_H_INCLUDED_

#if !defined(_XCP_INFRA_INFRA_H_INCLUDED_)
#error "Please don't directly #include this file. Instead, #include infra.h"
#endif  // !defined(_XCP_INFRA_INFRA_H_INCLUDED_)


namespace infra
{
    class sighandle
    {
    private:
        static inline std::atomic_bool __exit_required { false };
        static inline semaphore __sem { 0 };

#if PLATFORM_LINUX || PLATFORM_CYGWIN
        static void stop_handler(
            const int sig,
            [[maybe_unused]] siginfo_t* const info,
            [[maybe_unused]] void* const ucontext)
        {
            LOG_INFO("Caught signal: {}", sig);
            require_exit();
        }
#elif PLATFORM_WINDOWS
        static void stop_handler(const int sig)
        {
            LOG_INFO("Caught signal: {}", sig);
            require_exit();
        }
#else
#   error "Unknown platform"
#endif

    public:
        static void wait_for_exit_required()
        {
            try {
                __sem.wait();
            }
            catch (const std::system_error& ex) {
                if (ex.code() != std::error_code(EINTR, std::system_category())) {
                    throw;
                }

                if (!__exit_required) {
                    throw;
                }
                // __sem.wait() is interrupted
            }

            __sem.post();
        }

        static void require_exit()
        {
            bool expected = false;
            if (__exit_required.compare_exchange_strong(expected, true)) {
                LOG_INFO("Exit required...");
                __exit_required = true;

                __sem.post();
            }
        }

        static bool is_exit_required() noexcept
        {
            return __exit_required;
        }

        static void setup_signal_handler()
        {
#if PLATFORM_LINUX || PLATFORM_CYGWIN
            #define _SETUP_FOR_SIGNAL(_Signal_) \
                do { \
                    struct sigaction act { }; \
                    act.sa_flags = SA_RESTART | SA_SIGINFO; \
                    act.sa_sigaction = &stop_handler; \
                    if (sigaction((_Signal_), &act, nullptr) != 0) { \
                        LOG_ERROR("sigaction() for {} failed: errno = {} ({})", #_Signal_, errno, strerror(errno)); \
                        THROW_SYSTEM_ERROR(errno, sigaction); \
                    } \
                    LOG_TRACE("Setup signal handler for {}", #_Signal_); \
                } while(false)

            _SETUP_FOR_SIGNAL(SIGINT);
            _SETUP_FOR_SIGNAL(SIGABRT);
            _SETUP_FOR_SIGNAL(SIGTERM);
            _SETUP_FOR_SIGNAL(SIGQUIT);
            _SETUP_FOR_SIGNAL(SIGTSTP);

            #undef _SETUP_FOR_SIGNAL

#elif PLATFORM_WINDOWS

            // Signal on Windows
            //  See https://docs.microsoft.com/zh-cn/cpp/c-runtime-library/reference/signal?view=vs-2019

            #define _SETUP_FOR_SIGNAL(_Signal_) \
                do { \
                    if (signal((_Signal_), stop_handler) == SIG_ERR) { \
                        LOG_ERROR("signal() for {} failed: errno = {} ({})", #_Signal_, errno, strerror(errno)); \
                        THROW_SYSTEM_ERROR(errno, signal); \
                    } \
                    LOG_TRACE("Setup signal handler for {}", #_Signal_); \
                } while(false)

            _SETUP_FOR_SIGNAL(SIGINT);
            _SETUP_FOR_SIGNAL(SIGABRT);
            _SETUP_FOR_SIGNAL(SIGTERM);

            #undef _SETUP_FOR_SIGNAL
#else
#   error "Unknown platform"
#endif
        }
    };

}  // namespace infra


#endif  // !defined(_XCP_INFRA_SIGHANDLE_H_INCLUDED_)
