#pragma once
#include "StepResult.h"
#include "Task.h"
#include "Promise.h"
#include <coroutine>
#include <memory>
#include <stdexcept>
#include <string>

template <typename PromiseTypeT>
struct CoroutineTask : public Task
{
    using promise_type = PromiseTypeT;
    CoroutineTask(std::string name, promise_type &promise) 
        : 
        Task(std::move(name)), 
        handle(std::coroutine_handle<promise_type>::from_promise(promise))
    {}

    std::coroutine_handle<promise_type> handle;
    StepResult step(SingleThreadedExecutor &executor) override final
    {
        if (not handle.done())
            handle.resume();
        if (not handle.promise().step_result)
            throw std::runtime_error("Should be present");
        return std::move(*handle.promise().step_result);
    }
};

template <typename T, typename ...Args>
struct std::coroutine_traits<std::unique_ptr<T>, Args...>
{
    using promise_type = typename T::promise_type;
};