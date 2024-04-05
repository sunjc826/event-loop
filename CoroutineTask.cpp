#include "CoroutineTask.h"
#include "Task.h"
#include "declarations.h"

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

std::unique_ptr<CoroConditionVariableWaitTask> condition_variable_wait_task(Rc<Mutex> mutex, Rc<ConditionVariable> cv)
{
    
    mutex->is_acquired = false;
    if (mutex->waker->has_waiters())
        mutex->waker->wake_one(co_await executor_awaiter);
    co_yield step_result::Wait(
        step_result::Wait::task_not_done, 
        step_result::WaitForWaker(*cv->waker.get()));

    co_yield step_result::Wait(
        step_result::Wait::task_automatically_done, 
        step_result::WaitForChildTasks(make_vector_unique<Task>(RcMutexAcquireTask(mutex))));
}

std::unique_ptr<CoroConditionVariableNotifyTask> condition_variable_notify_task(bool notify_all, Rc<ConditionVariable> cv)
{
    if (cv->waker->has_waiters())
    {
        if (notify_all)
            cv->waker->wake_all(co_await executor_awaiter);
        else
            cv->waker->wake_one(co_await executor_awaiter);
    }
    co_yield step_result::Done();
}