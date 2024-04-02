#include "StepResult.h"
#include "Task.h"

template <typename TaskT, typename... OtherTaskT>
struct ConcatTask final : public Task
{
    std::optional<TaskT> task;
    ConcatTask<OtherTaskT...> other_tasks;
    ConcatTask(TaskT &&task, OtherTaskT &&...other_tasks)
        : task(std::move(task)), other_tasks(ConcatTask<OtherTaskT...>(std::move(other_tasks)...))
    {}
    StepResult step(SingleThreadedExecutor &executor, std::vector<std::optional<std::unique_ptr<void, TypeErasedDeleter>>> child_return_values) override
    {
        if (task)
        {
            auto result = task->step(executor, std::move(child_return_values));
            if (step_result::Done *done = std::get_if<step_result::Done>(&result))
            {
                task = std::nullopt;
                return step_result::Ready(done->child_tasks);
            }
            else
            if (step_result::Ready *ready = std::get_if<step_result::Ready>(&result))
            {
                return std::move(result);
            }
            else
            if (step_result::Wait *wait = std::get_if<step_result::Wait>(&result))
            {
                switch (wait->on_wait_finish)
                {
                case step_result::Wait::task_automatically_done:
                    task = std::nullopt;
                    break;
                case step_result::Wait::task_not_done:
                    break;
                }
                wait->on_wait_finish = step_result::Wait::task_not_done;
                return std::move(result);
            }
        }
        else
        {
            return other_tasks.step(executor, std::move(child_return_values));
        }
    }
};

template <typename TaskT>
struct ConcatTask<TaskT> final : public Task
{
    TaskT task;
    ConcatTask(TaskT &&task)
        : task(std::move(task))
    {}
    StepResult step(SingleThreadedExecutor &executor, std::vector<std::optional<std::unique_ptr<void, TypeErasedDeleter>>> child_return_values) override
    {
        return task.step(executor, std::move(child_return_values));
    }
};

template <typename TaskT, typename... OtherTasks>
struct IndependentTasks final : public Task
{
    std::optional<TaskT> task;
    IndependentTasks<OtherTasks...> other_tasks;
    IndependentTasks(TaskT &&task, OtherTasks &&...other_tasks)
        : task(std::move(task)), other_tasks(IndependentTasks<OtherTasks...>(std::move(other_tasks)...))
    {}

    StepResult step(SingleThreadedExecutor &executor, std::vector<std::optional<std::unique_ptr<void, TypeErasedDeleter>>> child_return_values) override
    {
        if (task)
        {
            auto result = task->step(executor, std::move(child_return_values));
            if (step_result::Done *done = std::get_if<step_result::Done>(&result))
            {
                task = std::nullopt;
                return step_result::Ready(done->child_tasks);
            }
            else
            if (step_result::Ready *ready = std::get_if<step_result::Ready>(&result))
            {
                return std::move(result);
            }
            else
            if (step_result::Wait *wait = std::get_if<step_result::Wait>(&result))
            {
                // TODO: Use Task::woken_task_id
                switch (wait->on_wait_finish)
                {
                case step_result::Wait::task_automatically_done:
                    task = std::nullopt;
                    break;
                case step_result::Wait::task_not_done:
                    break;
                }
                return step_result::Wait(step_result::Wait::task_not_done, std::move(wait->wait_for));
            }
        }
        else
        {
            return other_tasks.step(executor, std::move(child_return_values));
        }
    }
};

template <typename TaskT>
struct IndependentTasks<TaskT> final : public Task
{
    TaskT task;
    IndependentTasks(Task &&task)
        : task(std::move(task))
    {}

    StepResult step(SingleThreadedExecutor &executor, std::vector<std::optional<std::unique_ptr<void, TypeErasedDeleter>>> child_return_values) override
    {
        return task.step(executor, std::move(child_return_values));
    }
};
