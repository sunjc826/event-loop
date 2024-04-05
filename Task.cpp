#include "Task.h"
#include "Executor.h"
#include "StepResult.h"
#include "declarations.h"
#include <memory>
#include <stdexcept>
#include <sys/epoll.h>
#include <span>
Mutex::Mutex() : waker(std::make_unique<FifoWaker>()) {}
ConditionVariable::ConditionVariable() : waker(std::make_unique<FifoWaker>()) {}
StepResult MutexAcquireTask::step(SingleThreadedExecutor &executor [[maybe_unused]]) 
{
    if (!mutex.is_acquired)
    {
        mutex.is_acquired = true;
        return step_result::Done();
    }
    else
        return step_result::Wait(
            step_result::Wait::task_not_done, 
            step_result::WaitForWaker(*mutex.waker.get()));
}

StepResult RcMutexAcquireTask::step(SingleThreadedExecutor &executor [[maybe_unused]]) 
{
    if (!mutex->is_acquired)
    {
        mutex->is_acquired = true;
        return step_result::Done();
    }
    else
        return step_result::Wait(
            step_result::Wait::task_not_done,
            step_result::WaitForWaker(*mutex->waker.get()));
}

StepResult MutexReleaseTask::step(SingleThreadedExecutor &executor)
{
    mutex.is_acquired = false;
    if (mutex.waker->has_waiters())
        mutex.waker->wake_one(executor);
    return step_result::Done();
}

StepResult RcMutexReleaseTask::step(SingleThreadedExecutor &executor)
{
    mutex->is_acquired = false;
    if (mutex->waker->has_waiters())
        mutex->waker->wake_one(executor);
    return step_result::Done();
}

StepResult ConditionVariableWaitTask::step(SingleThreadedExecutor &executor)
{
    switch (stage++)
    {
    case 0:
        mutex.is_acquired = false;
        if (mutex.waker->has_waiters())
            mutex.waker->wake_one(executor);
        return step_result::Wait(step_result::Wait::task_not_done, step_result::WaitForWaker(*cv.waker.get()));
    case 1:
        return step_result::Wait(step_result::Wait::task_automatically_done, step_result::WaitForChildTasks(make_vector_unique<Task>(MutexAcquireTask(mutex))));
    default:
        throw std::runtime_error("Unreachable");
    }
}

StepResult RcConditionVariableWaitTask::step(SingleThreadedExecutor &executor)
{
    switch (stage++)
    {
    case 0:
        mutex->is_acquired = false;
        if (mutex->waker->has_waiters())
            mutex->waker->wake_one(executor);
        return step_result::Wait(
            step_result::Wait::task_not_done, 
            step_result::WaitForWaker(*cv->waker.get()));
    case 1:
        return step_result::Wait(
            step_result::Wait::task_automatically_done, 
            step_result::WaitForChildTasks(make_vector_unique<Task>(RcMutexAcquireTask(mutex))));
    default:
        throw std::runtime_error("Unreachable");
    }
}

StepResult ConditionVariableNotifyTask::step(SingleThreadedExecutor &executor)
{
    if (cv.waker->has_waiters())
    {
        if (notify_all)
            cv.waker->wake_all(executor);
        else
            cv.waker->wake_one(executor);
    }
    return step_result::Done();
}

StepResult RcConditionVariableNotifyTask::step(SingleThreadedExecutor &executor)
{
    if (cv->waker->has_waiters())
    {
        if (notify_all)
            cv->waker->wake_all(executor);
        else
            cv->waker->wake_one(executor);
    }
    return step_result::Done();
}

void EpollTask::execute(SingleThreadedExecutor &executor)
{
    std::array<epoll_event, 5> events;
    int num_events;
    if ((num_events = epoll_wait(epoll_fd, events.data(), static_cast<int>(events.size()), 0)) == -1)
    {
        std::array<char, 1024> buf;
        std::snprintf(buf.data(), buf.size(), "%s", strerror(errno));
        throw std::runtime_error(buf.data());
    }
    std::span<epoll_event> const returned_events(events.begin(), events.begin() + num_events);
    for (epoll_event const &event : returned_events)
    {
        Handler *handler = reinterpret_cast<Handler *>(event.data.ptr);
        std::unique_ptr<Task> task = handler->handle(event.events);
        if (task != nullptr)
            executor.add_task(std::move(task));
    }
}

StepResult EpollTask::step(SingleThreadedExecutor &executor)
{
    execute(executor);
    return step_result::Ready();
}

SleepingTask::SleepingTask(std::unique_ptr<SleepingTask> next, std::unique_ptr<Task> task, bool destroy_on_wake)
    : prev(), next(std::move(next)), task(std::move(task)), destroy_on_wake(destroy_on_wake)
{
    this->next->prev = this;
}
