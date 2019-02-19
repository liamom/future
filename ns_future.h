#include <gsl/gsl>

#include <functional>
#include <mutex>
#include <condition_variable>
#include <future>

#include <thread>

namespace ns {

using executor_t = void (*)(std::function<void()>);
auto default_executor_impl = [](std::function<void()> f) {
    std::thread(std::move(f)).detach();
};

extern executor_t g_default_executor;

template<class TYPE>
class state : public std::enable_shared_from_this<state<TYPE>> {
    union {
        TYPE value_;
        std::exception_ptr exception_;
    };

    enum {
        NONE,
        HAS_VALUE,
        HAS_EXCEPTION,
    } status_{NONE};

    mutable std::mutex mutex_;
    mutable std::condition_variable cv_;
    std::function<void(std::shared_ptr<state>)> continuation_;


public:
    void reset() {
        switch (status_) {
            case NONE:
                break;
            case HAS_VALUE:
                value_.TYPE::~TYPE();
                break;
            case HAS_EXCEPTION:
                exception_.std::exception_ptr::~exception_ptr();
                break;
        }

        status_ = NONE;
    }

    // for make_ready_future
    template<class ...ARGS>
    state(ARGS &&...args) {
        status_ = HAS_VALUE;
        new(&value_) TYPE(std::forward<ARGS>(args)...);
    }

    state() {}

    ~state() {
        reset();
    }

    template<class ...ARGS>
    void set_value(ARGS &&...args) {
        if (status_ != NONE)
            throw std::future_error{std::future_errc::promise_already_satisfied};

        {
            std::lock_guard<std::mutex> lock(mutex_);
            status_ = HAS_VALUE;
            new(&value_) TYPE(std::forward<ARGS>(args)...);
        }
        cv_.notify_one();

        if (continuation_) {
            continuation_(this->shared_from_this());
        }
    }

    void set_exception(std::exception_ptr p) {
        if (status_ != NONE)
            throw std::future_error{std::future_errc::promise_already_satisfied};

        {
            std::lock_guard<std::mutex> lock(mutex_);
            status_ = HAS_EXCEPTION;
            new(&exception_) std::exception_ptr(p);
        }
        cv_.notify_one();

        if (continuation_)
            continuation_(this->shared_from_this());
    }

    TYPE get() {
        std::unique_lock<decltype(mutex_)> lock{mutex_};

        cv_.wait(lock, [this]() {
            return this->status_ != NONE;
        });

        if (status_ == HAS_EXCEPTION)
            std::rethrow_exception(exception_);

        return std::move(value_);
    }

    std::future_status wait() const {
        std::unique_lock<decltype(mutex_)> lock{mutex_};

        cv_.wait(lock, [this]() {
            return this->status_ != NONE;
        });

        return std::future_status::ready;
    }

    template<class Rep, class Period>
    std::future_status wait_for(const std::chrono::duration<Rep, Period> &rel_time) const {
        std::unique_lock<decltype(mutex_)> lock{mutex_};

        bool time_out = cv_.wait_for(lock, rel_time, [this]() {
            return this->status_ != NONE;
        });

        if (time_out)
            return std::future_status::timeout;

        return std::future_status::ready;
    }

    template<class Clock, class Duration>
    std::future_status wait_until(const std::chrono::time_point<Clock, Duration> &abs_time) const {
        std::unique_lock<decltype(mutex_)> lock{mutex_};

        bool time_out = cv_.wait_until(lock, abs_time, [this]() {
            return this->status_ != NONE;
        });

        if (time_out)
            return std::future_status::timeout;

        return std::future_status::ready;
    }

    void then(std::function<void(std::shared_ptr<state>)> &&func) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (status_ == NONE) {
                continuation_ = std::move(func);
                return;
            }
        }

        // don't need to lock cause the value is already set and it will through an exception if you try to set it again
        //
        func(this->shared_from_this());
    }
};

template<class TYPE>
class future;

template<class TYPE, class ...ARGS>
future<TYPE> create_ready_future(ARGS &&... args);

template<class future_t>
class future {
public:
    using type = future_t;

private:
    template<class ...ARGS>
    friend future<future_t> create_ready_future(ARGS &&... args);

    std::shared_ptr<state<future_t>> state_;

public:

    template<class ...ARGS>
    future(ARGS &&... args): state_(std::make_shared<future_t>(std::forward<ARGS>(args)...)) {}

    future() {}

    future(const future &) = delete;

    future &operator=(const future &) = default;

    future(future &&p) : state_(std::move(p.state_)) {
        p.state_ = nullptr;
    }

    future &operator=(future &&p) {
        this->state_ = std::move(p.state_);
        p.state_ = nullptr;
    }

    future(std::shared_ptr<state<future_t>> state) : state_(std::move(state)) {}

    future_t get() {
        if (!state_)
            throw std::future_error{std::future_errc::no_state};

        auto rm_state = gsl::finally([this]() {
            this->state_->reset();
            this->state_ = nullptr;
        });

        return state_->get();
    }

    std::future_status wait() {
        if (!state_)
            throw std::future_error{std::future_errc::no_state};

        return state_->wait();
    }

    template<class Rep, class Period>
    std::future_status wait_for(const std::chrono::duration<Rep, Period> &rel_time) const {
        if (!state_)
            throw std::future_error{std::future_errc::no_state};

        return state_->wait_for(rel_time);
    }

    template<class Clock, class Duration>
    std::future_status wait_until(const std::chrono::time_point<Clock, Duration> &abs_time) const {
        if (!state_)
            throw std::future_error{std::future_errc::no_state};

        return state_->wait_until(abs_time);
    }

    bool valid() {
        return state_ != nullptr;
    }

    template <class callback_t, class return_t = typename std::result_of<callback_t(future_t)>::type>
    auto then(callback_t cb_function) -> return_t;

};

template<class TYPE, class ...ARGS>
future<TYPE> create_ready_future(ARGS &&... args) {
    return {std::forward<ARGS>(args)...};
}


template<class TYPE>
class promise {
    std::shared_ptr<state<TYPE>> state_;
    bool has_future = false;

public:
    promise() : state_(std::make_shared<state<TYPE>>()) {}

    promise(const promise &) = delete;

    promise &operator=(const promise &) = delete;

    promise(promise &&p) : state_(std::move(p.state_)) {}

    promise &operator=(promise &&p) noexcept {
        this->state_ = std::move(p.state_);
    }

    future<TYPE> get_future() {
        if(has_future)
            throw std::future_error{std::future_errc::future_already_retrieved};

        has_future = true;

        return {state_};
    }

    template<class ...ARGS>
    void set_value(ARGS &&...args) {
        if (!state_)
            throw std::future_error{std::future_errc::no_state};

        state_->set_value(std::forward<ARGS>(args)...);
    }

    void set_exception(std::exception_ptr p) {
        if (!state_)
            throw std::future_error{std::future_errc::no_state};

        state_->set_exception(std::move(p));
    }

};

template<class future_t>
template<class callback_t, class return_t>
auto future<future_t>::then(callback_t cb_function) -> return_t {
    static_assert(std::is_same<return_t, void>::value, "is null");

    if (!state_)
        throw std::future_error{std::future_errc::no_state};

    state_->then([cb_function = std::move(cb_function)](std::shared_ptr<state<future_t>> state) mutable {
        g_default_executor([cb_function = std::move(cb_function), state = std::move(state)]() mutable {
            cb_function(future<future_t> {state});
        });
    });
}

}