#pragma once
#include "Executor.h"
#include "StepResult.h"
#include "Task.h"
#include "declarations.h"
#include "utilities.h"
#include <coroutine>
#include <memory>
#include <stdexcept>
#include <string>

struct Void{};
struct ExecutorAwaiter{};
static constexpr ExecutorAwaiter executor_awaiter;
template <typename PromiseType, typename ChildT, typename CoroutineTaskT>
struct AbstractPromiseType
{
    Executor *most_recent_executor = nullptr;
    std::optional<StepResult> step_result;
    std::optional<std::vector<std::optional<std::unique_ptr<void, TypeErasedDeleter>>>> last_child_return_values;

    std::unique_ptr<CoroutineTaskT> get_return_object()
    {
        return std::make_unique<CoroutineTaskT>(
            static_cast<ChildT *>(this)->get_name(), 
            *static_cast<ChildT *>(this)
        );
    }

    std::suspend_always initial_suspend()
    {
        return {};
    }

    std::suspend_always yield_value(StepResult step_result)
    {
        this->step_result.emplace(std::move(step_result));
        return {};
    }
    
    auto await_transform(ExecutorAwaiter)
    {
        struct awaitable : std::suspend_never
        {
            PromiseType &promise;
            awaitable(PromiseType &promise)
                : promise(promise)
            {}
            Executor &await_resume()
            {
                return *promise.most_recent_executor;
            }
        };
        return awaitable(*static_cast<PromiseType *>(this));
    }

    template <typename TaskT>
    requires requires(TaskT)
    {
        std::derived_from<TaskT, Task>;
        typename TaskT::UnambiguousReturnType;
    }
    auto await_transform(std::unique_ptr<TaskT> task)
    {
        struct awaitable : std::suspend_always
        {
            PromiseType &promise;
            awaitable(PromiseType &promise) : promise(promise) {}
            std::unique_ptr<typename TaskT::UnambiguousReturnType> await_resume() const noexcept 
            {
                if constexpr (std::is_void_v<typename TaskT::UnambiguousReturnType>)
                    return;
                else
                {
                    std::optional<std::unique_ptr<void, TypeErasedDeleter>> ret_val = std::move((*promise.last_child_return_values)[0]);
                    void *raw_ptr = ret_val->release();
                    return std::unique_ptr<typename TaskT::UnambiguousReturnType>(reinterpret_cast<typename TaskT::UnambiguousReturnType *>(raw_ptr));
                }
            }
        };
        this->step_result.emplace(step_result::Wait(step_result::Wait::task_not_done, make_vector_unique<Task>(std::move(task))));
        return awaitable(*static_cast<PromiseType *>(this));
    }

    std::suspend_always final_suspend() noexcept
    {
        return {};
    }

    void unhandled_exception()
    {
    }
};

template <typename ChildT, typename CoroutineTaskT>
struct PromiseTypeWithReturnValue : public AbstractPromiseType<PromiseType<ChildT, CoroutineTaskT>, ChildT, CoroutineTaskT>
{
    void return_value(Void) {}

    template <typename ReturnTypeT>
    void return_value(std::unique_ptr<ReturnTypeT> ret_val)
    {
        this->step_result.emplace(step_result::Done(std::move(ret_val)));
    }

    template <typename ReturnTypeT>
    void return_value(ReturnTypeT ret_val)
    {
        this->step_result.emplace(step_result::Done(std::make_unique<ReturnTypeT>(std::move(ret_val))));
    }
};

template <typename ChildT, typename CoroutineTaskT>
struct PromiseType : public AbstractPromiseType<PromiseType<ChildT, CoroutineTaskT>, ChildT, CoroutineTaskT>
{
    void return_void() {}
};

template <typename PromiseTypeT>
requires requires(PromiseTypeT promise)
{
    { promise.most_recent_executor } -> DecaysTo<SingleThreadedExecutor *>;
    { promise.last_child_return_values } -> DecaysTo<std::optional<std::vector<std::optional<std::unique_ptr<void, TypeErasedDeleter>>>>>;
}
struct CoroutineTask : public Task
{
    using promise_type = PromiseTypeT;
    CoroutineTask(std::string name, promise_type &promise) 
        : 
        Task(std::move(name)), 
        handle(std::coroutine_handle<promise_type>::from_promise(promise))
    {}

    std::coroutine_handle<promise_type> handle;
    StepResult step(SingleThreadedExecutor &executor, std::vector<std::optional<std::unique_ptr<void, TypeErasedDeleter>>> child_return_values) override final
    {   
        handle.promise().most_recent_executor = &executor;
        handle.promise().last_child_return_values = std::move(child_return_values);
        if (not handle.done())
            handle.resume();
        if (not handle.promise().step_result)
            throw std::runtime_error("Should be present");
        return std::move(*handle.promise().step_result);
    }

    ~CoroutineTask()
    {
        handle.destroy();
    }
};

template <typename T, typename ...Args>
struct std::coroutine_traits<std::unique_ptr<T>, Args...>
{
    using promise_type = typename T::promise_type;
};

struct CoroMutexAcquireTask;
struct CoroMutexAcquireTaskPromise : PromiseType<CoroMutexAcquireTaskPromise, CoroMutexAcquireTask>
{
    static std::string get_name()
    {
        return "CoroMutexAcquireTaskPromise";
    }
};
struct CoroMutexAcquireTask final : CoroutineTask<CoroMutexAcquireTaskPromise>
{
    CoroMutexAcquireTask(std::string name, promise_type &promise) 
        : CoroutineTask(std::move(name), promise)
    {}
};

std::unique_ptr<CoroMutexAcquireTask> mutex_acquire_task(Rc<Mutex> mutex);

struct CoroMutexReleaseTask;
struct CoroMutexReleaseTaskPromise final : PromiseType<CoroMutexReleaseTaskPromise, CoroMutexReleaseTask>
{
    static std::string get_name()
    {
        return "CoroMutexReleaseTaskPromise"; 
    }
};
struct CoroMutexReleaseTask : public CoroutineTask<CoroMutexReleaseTaskPromise>
{
    CoroMutexReleaseTask(std::string name, promise_type &promise) 
        : CoroutineTask(std::move(name), promise)
    {}
};
std::unique_ptr<CoroMutexReleaseTask> mutex_release_task(Rc<Mutex> mutex);

struct CoroConditionVariableWaitTask;
struct CoroConditionVariableWaitTaskPromise final : PromiseType<CoroConditionVariableWaitTaskPromise, CoroConditionVariableWaitTask>
{
    static std::string get_name()
    {
        return "CoroConditionVariableWaitTaskPromise";
    }
};
struct CoroConditionVariableWaitTask : public CoroutineTask<CoroConditionVariableWaitTaskPromise>
{
    CoroConditionVariableWaitTask(std::string name, promise_type &promise)
        : CoroutineTask(std::move(name), promise)
    {}
};
std::unique_ptr<CoroConditionVariableWaitTask> condition_variable_wait_task(Rc<Mutex> mutex, Rc<ConditionVariable> cv);

struct CoroConditionVariableNotifyTask;
struct CoroConditionVariableNotifyTaskPromise final : PromiseType<CoroConditionVariableNotifyTaskPromise, CoroConditionVariableNotifyTask>
{
    static std::string get_name()
    {
        return "CoroConditionVariableNotifyTaskPromise";
    }
};
struct CoroConditionVariableNotifyTask : public CoroutineTask<CoroConditionVariableNotifyTaskPromise>
{
    CoroConditionVariableNotifyTask(std::string name, promise_type &promise)
        : CoroutineTask(std::move(name), promise)
    {}
};
std::unique_ptr<CoroConditionVariableNotifyTask> condition_variable_notify_task(bool notify_all, Rc<ConditionVariable> cv);
