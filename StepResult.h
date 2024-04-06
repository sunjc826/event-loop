#pragma once
#include "CompositeTask.decl.h"
#include "Task.h"
#include "utilities.h"
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
    Done(std::unique_ptr<ReturnTypeT> return_value,
         std::vector<std::unique_ptr<Task>> child_tasks = {})
        : return_value(make_type_erased(std::move(return_value))),
          child_tasks(std::move(child_tasks))
    {
    }
    Done(std::unique_ptr<void, TypeErasedDeleter> return_value,
         std::vector<std::unique_ptr<Task>> child_tasks = {});
    Done(std::vector<std::unique_ptr<Task>> child_tasks);
};
struct Ready
{
    // Go to front of executor rather than the back
    bool high_priority = false;
    // The child tasks, if non-empty, can be viewed as daemons.
    std::vector<std::unique_ptr<Task>> child_tasks;
    Ready() = default;
    Ready(bool high_priority);
    Ready(bool high_priority, std::vector<std::unique_ptr<Task>> child_tasks);
    Ready(std::vector<std::unique_ptr<Task>> child_tasks);
};
struct WaitForWaker
{
    Waker &waker;
    WaitForWaker(Waker &waker);
};
struct WaitForChildTasks
{
    std::vector<std::unique_ptr<Task>> tasks;
    WaitForChildTasks(std::vector<std::unique_ptr<Task>> tasks);
};
using WaitFor = std::variant<WaitForWaker, WaitForChildTasks>;
struct Wait
{
    enum OnWaitFinish : bool
    {
        task_automatically_done,
        task_not_done,
    };
    OnWaitFinish on_wait_finish;
    WaitFor wait_for;
    Wait(OnWaitFinish on_wait_finish, WaitFor wait_for);
    Wait(OnWaitFinish on_wait_finish,
         std::vector<std::unique_ptr<Task>> child_tasks);
    Wait(OnWaitFinish on_wait_finish, Waker &waker);
};

// Used by composite tasks
struct CompositeWait
{
    bool all_subtasks_sleeping;
    Waker &root_waker;
    SubtaskStatus &leaf_status;
    std::vector<std::reference_wrapper<SubtaskStatus>> statuses;
    Wait wait;
    CompositeWait(bool all_subtasks_sleeping, Waker &root_waker,
                  SubtaskStatus &status, Wait wait)
        : all_subtasks_sleeping(all_subtasks_sleeping), root_waker(root_waker),
          leaf_status(status), wait(std::move(wait))
    {
    }

    CompositeWait(bool all_subtasks_sleeping, Waker &root_waker,
                  SubtaskStatus &status, CompositeWait composite_wait)
        : all_subtasks_sleeping(all_subtasks_sleeping), root_waker(root_waker),
          leaf_status(composite_wait.leaf_status),
          statuses(
              [&status, &composite_wait]
              {
                  composite_wait.statuses.push_back(std::ref(status));
                  return std::move(composite_wait.statuses);
              }()),
          wait(std::move(composite_wait.wait))
    {
    }

    CompositeWait(Waker &root_waker, CompositeWait composite_wait)
        : all_subtasks_sleeping(composite_wait.all_subtasks_sleeping),
        root_waker(root_waker),
        leaf_status(composite_wait.leaf_status),
        statuses(std::move(composite_wait.statuses)),
        wait(std::move(composite_wait.wait))
    {}
};
} // namespace step_result

using StepResult = std::variant<step_result::Done, step_result::Ready,
                                step_result::Wait, step_result::CompositeWait>;