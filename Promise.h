#pragma once
#include "CoroutineTask.h"
#include "StepResult.h"
#include <memory>
#include <coroutine>
#include <optional>
struct Void{};

template <typename ChildT, typename CoroutineTaskT>
struct PromiseType
{
    std::optional<StepResult> step_result;

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

    // std::suspend_always yield_value(Void)
    // {
    //     return {};
    // }

    std::suspend_always yield_value(StepResult step_result)
    {
        this->step_result.emplace(std::move(step_result));
        return {};
    }

    void return_void()
    {
    }

    std::suspend_always final_suspend() noexcept
    {
        return {};
    }

    void unhandled_exception()
    {
    }
};

