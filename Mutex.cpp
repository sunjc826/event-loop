#include "Mutex.h"

Mutex::Mutex() : waker(std::make_unique<FifoWaker>()) {}

StepResult MutexAcquireTask::step(SingleThreadedExecutor &executor
                                  [[maybe_unused]])
{
    if (!mutex.is_acquired)
    {
        mutex.is_acquired = true;
        return step_result::Done();
    }
    else
        return step_result::Wait(step_result::Wait::task_not_done,
                                 step_result::WaitForWaker(*mutex.waker.get()));
}

StepResult RcMutexAcquireTask::step(SingleThreadedExecutor &executor
                                    [[maybe_unused]])
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

std::unique_ptr<CoroMutexAcquireTask> mutex_acquire_task(Rc<Mutex> mutex)
{
    if (!mutex->is_acquired)
    {
        mutex->is_acquired = true;
        co_yield step_result::Done();
    }
    else
        co_yield step_result::Wait(
            step_result::Wait::task_automatically_done,
            step_result::WaitForWaker(*mutex->waker.get()));
}

std::unique_ptr<CoroMutexReleaseTask> mutex_release_task(Rc<Mutex> mutex)
{
    mutex->is_acquired = false;
    if (mutex->waker->has_waiters())
        mutex->waker->wake_one(co_await executor_awaiter);
    co_yield step_result::Done();
}