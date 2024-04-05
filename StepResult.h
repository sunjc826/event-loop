#pragma once
#include "declarations.h"
#include <cstdint>

namespace step_result
{
    struct Done
    {
        std::unique_ptr<void, TypeErasedDeleter> return_value;
        // The child tasks, if non-empty, can be viewed as daemons.
        std::vector<std::unique_ptr<Task>> child_tasks;
        Done() = default;
        template <typename ReturnTypeT>
        Done(std::unique_ptr<ReturnTypeT> return_value, std::vector<std::unique_ptr<Task>> child_tasks = {})
            : return_value(make_type_erased(std::move(return_value))), child_tasks(std::move(child_tasks))
        {}

        Done(std::unique_ptr<void, TypeErasedDeleter> return_value, std::vector<std::unique_ptr<Task>> child_tasks = {})
            : return_value(std::move(return_value)), child_tasks(std::move(child_tasks))
        {}
        Done(std::vector<std::unique_ptr<Task>> child_tasks)
            : child_tasks(std::move(child_tasks))
        {}
    };
    struct Ready
    {
        // Go to front of executor rather than the back
        bool high_priority = false;
        // The child tasks, if non-empty, can be viewed as daemons.
        std::vector<std::unique_ptr<Task>> child_tasks;
        Ready() = default;
        Ready(bool high_priority)
            : high_priority(high_priority)
        {}
        Ready(bool high_priority, std::vector<std::unique_ptr<Task>> child_tasks)
            : high_priority(high_priority), child_tasks(std::move(child_tasks))
        {}
        Ready(std::vector<std::unique_ptr<Task>> child_tasks)
            : child_tasks(std::move(child_tasks))
        {}
    };
    struct WaitForWaker
    {
        Waker &waker;
        WaitForWaker(Waker &waker) 
            : waker(waker) 
        {}
    };
    struct WaitForChildTasks
    {
        std::vector<std::unique_ptr<Task>> tasks;
        WaitForChildTasks(std::vector<std::unique_ptr<Task>> tasks)
            : tasks(std::move(tasks))
        {}
    };
    struct Wait
    {
        enum OnWaitFinish : bool
        {
            task_automatically_done,
            task_not_done,
        };
        OnWaitFinish on_wait_finish;
        std::variant<WaitForWaker, WaitForChildTasks> wait_for;
        Wait(OnWaitFinish on_wait_finish, std::variant<WaitForWaker, WaitForChildTasks> wait_for)
            : on_wait_finish(on_wait_finish), wait_for(std::move(wait_for))
        {}

        Wait(OnWaitFinish on_wait_finish, std::vector<std::unique_ptr<Task>> child_tasks)
            : on_wait_finish(on_wait_finish), wait_for(WaitForChildTasks(std::move(child_tasks)))
        {}
        
        Wait(OnWaitFinish on_wait_finish, std::reference_wrapper<Waker> waker)
            : on_wait_finish(on_wait_finish), wait_for(WaitForWaker(waker))
        {}
    };

    // Used by composite tasks
    struct PartialWait
    {
        SubtaskStatus &status; 
        Wait wait;
        template <typename ...Args>
        PartialWait(SubtaskStatus &status, Args &&...args)
            : status(status), wait(std::forward<Args>(args)...)
        {}
    };
    // Used by composite tasks
    struct FullWait
    {
        SubtaskStatus &status; 
        Wait wait;
        template <typename ...Args>
        FullWait(SubtaskStatus &status, Args &&...args)
            : status(status), wait(std::forward<Args>(args)...)
        {}
    };
}

using StepResult = std::variant<
    step_result::Done, 
    step_result::Ready, 
    step_result::Wait, 
    step_result::PartialWait, 
    step_result::FullWait>;

