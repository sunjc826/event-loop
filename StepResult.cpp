#include "StepResult.h"

namespace step_result
{
Done::Done(std::unique_ptr<void, TypeErasedDeleter> return_value,
           std::vector<std::unique_ptr<Task>> child_tasks)
    : return_value(std::move(return_value)), child_tasks(std::move(child_tasks))
{
}
Done::Done(std::vector<std::unique_ptr<Task>> child_tasks)
    : child_tasks(std::move(child_tasks))
{
}
Ready::Ready(bool high_priority) : high_priority(high_priority) {}
Ready::Ready(bool high_priority, std::vector<std::unique_ptr<Task>> child_tasks)
    : high_priority(high_priority), child_tasks(std::move(child_tasks))
{
}
Ready::Ready(std::vector<std::unique_ptr<Task>> child_tasks)
    : child_tasks(std::move(child_tasks))
{
}
WaitForWaker::WaitForWaker(Waker &waker) : waker(waker) {}
WaitForChildTasks::WaitForChildTasks(std::vector<std::unique_ptr<Task>> tasks)
    : tasks(std::move(tasks))
{
}

Wait::Wait(OnWaitFinish on_wait_finish,
           std::variant<WaitForWaker, WaitForChildTasks> wait_for)
    : on_wait_finish(on_wait_finish), wait_for(std::move(wait_for))
{
}
Wait::Wait(OnWaitFinish on_wait_finish,
           std::vector<std::unique_ptr<Task>> child_tasks)
    : on_wait_finish(on_wait_finish),
      wait_for(WaitForChildTasks(std::move(child_tasks)))
{
}

Wait::Wait(OnWaitFinish on_wait_finish, std::reference_wrapper<Waker> waker)
    : on_wait_finish(on_wait_finish), wait_for(WaitForWaker(waker))
{
}
}; // namespace step_result