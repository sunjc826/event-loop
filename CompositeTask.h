#include "StepResult.h"
#include "Task.h"

template <typename TaskT, typename... OtherTaskT>
struct ConcatTask final : public Task
{
    std::optional<TaskT> task;
    ConcatTask<OtherTaskT...> other_tasks;
    ConcatTask(TaskT &&task, OtherTaskT &&...other_tasks)
        : task(std::move(task)),
          other_tasks(ConcatTask<OtherTaskT...>(std::move(other_tasks)...))
    {
    }
    StepResult step_with_result(
        SingleThreadedExecutor &executor,
        std::vector<std::optional<std::unique_ptr<void, TypeErasedDeleter>>>
            child_return_values) override
    {
        if (task)
        {
            StepResult result = task->step_with_result(
                executor, std::move(child_return_values));
            if (step_result::Done *done =
                    std::get_if<step_result::Done>(&result))
            {
                task = std::nullopt;
                return step_result::Ready(done->child_tasks);
            }
            else if (step_result::Ready *ready =
                         std::get_if<step_result::Ready>(&result))
            {
                // implicitly movable entity
                return result;
            }
            else if (step_result::Wait *wait =
                         std::get_if<step_result::Wait>(&result))
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
                return result;
            }
        }
        else
        {
            return other_tasks.step_with_result(executor,
                                                std::move(child_return_values));
        }
    }
};

template <typename TaskT>
struct ConcatTask<TaskT> final : public Task
{
    TaskT task;
    ConcatTask(TaskT &&task) : task(std::move(task)) {}
    StepResult step_with_result(
        SingleThreadedExecutor &executor,
        std::vector<std::optional<std::unique_ptr<void, TypeErasedDeleter>>>
            child_return_values) override
    {
        return task.step_with_result(executor, std::move(child_return_values));
    }
};

enum class SubtaskStatus
{
    ready,
    waiting,
    done,
};
template <typename TaskT>
struct TaskWithStatus
{
    TaskT task;
    SubtaskStatus status = SubtaskStatus::ready;
    TaskWithStatus(TaskT task) : task(std::move(task)) {}
};

template <typename TaskT, typename... OtherTasks>
struct IndependentTasks final : public Task
{
    std::optional<TaskWithStatus<TaskT>> task_with_status;
    IndependentTasks<OtherTasks...> other_tasks;
    IndependentTasks(TaskT task, OtherTasks... other_tasks)
        : Task("IndependentTasks"),
          task_with_status(TaskWithStatus<TaskT>(std::move(task))),
          other_tasks(
              IndependentTasks<OtherTasks...>(std::move(other_tasks)...))
    {
    }

    StepResult step_with_result(
        SingleThreadedExecutor &executor,
        std::vector<std::optional<std::unique_ptr<void, TypeErasedDeleter>>>
            child_return_values) override
    {

        if (task_with_status)
        {
            switch (task_with_status->status)
            {
            case SubtaskStatus::ready:
            {
                TaskT &task = task_with_status->task;
                StepResult result = task.step_with_result(
                    executor, std::move(child_return_values));
                if (step_result::Done *done =
                        std::get_if<step_result::Done>(&result))
                {
                    task_with_status = std::nullopt;
                    return step_result::Ready(std::move(done->child_tasks));
                }
                else if (step_result::Ready *ready =
                             std::get_if<step_result::Ready>(&result))
                {
                    // implicitly movable entity
                    return result;
                }
                else if (step_result::Wait *wait =
                             std::get_if<step_result::Wait>(&result))
                {
                    task_with_status->status = SubtaskStatus::waiting;
                    return step_result::PartialWait(task_with_status->status,
                                                    std::move(*wait));
                }
                else if (step_result::PartialWait *partial_wait =
                             std::get_if<step_result::PartialWait>(&result))
                {
                    return result;
                }

                break;
            }
            case SubtaskStatus::waiting:
            {
                break;
            }
            case SubtaskStatus::done:
            {
                task_with_status = std::nullopt;
                break;
            }
            }
        }
        bool is_task_waiting = task_with_status.has_value();
        StepResult result = other_tasks.step_with_result(
            executor, std::move(child_return_values));
        if (step_result::Done *done = std::get_if<step_result::Done>(&result))
        {
            if (is_task_waiting)
            {
                // TODO
                return step_result::Ready(std::move(done->child_tasks));
            }
            else
            {
                return result;
            }
        }
        else if (step_result::Ready *ready =
                     std::get_if<step_result::Ready>(&result))
        {
            return result;
        }
        else if (step_result::Wait *wait =
                     std::get_if<step_result::Wait>(&result))
        {
            return result;
        }
        else if (step_result::PartialWait *partial_wait =
                     std::get_if<step_result::PartialWait>(&result))
        {
            return result;
        }
    }
};

template <typename TaskT>
struct IndependentTasks<TaskT> final : public Task
{
    std::optional<TaskWithStatus<TaskT>> task_with_status;
    IndependentTasks(TaskT task)
        : Task("IndependentTasks"),
          task_with_status(TaskWithStatus<TaskT>(std::move(task)))
    {
    }

    StepResult step_with_result(
        SingleThreadedExecutor &executor,
        std::vector<std::optional<std::unique_ptr<void, TypeErasedDeleter>>>
            child_return_values) override
    {
        if (task_with_status)
        {
            TaskT &task = task_with_status->task;
            switch (task_with_status->status)
            {
            case SubtaskStatus::ready:
            {
                break;
            }
            case SubtaskStatus::waiting:
            {
                break;
            }
            case SubtaskStatus::done:
            {
                break;
            }
            }
            return task.step_with_result(executor,
                                         std::move(child_return_values));
        }
    }
};

template <typename TaskT, typename... OtherTasks>
std::unique_ptr<IndependentTasks<TaskT, OtherTasks...>>
make_independent_tasks(TaskT &&task, OtherTasks &&...other_tasks)
{
    return std::make_unique<IndependentTasks<TaskT, OtherTasks...>>(
        std::forward<TaskT>(task), std::forward<OtherTasks>(other_tasks)...);
}
