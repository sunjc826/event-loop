#include "ConditionVariable.h"
ConditionVariable::ConditionVariable() : waker(std::make_unique<FifoWaker>()) {}

StepResult ConditionVariableWaitTask::step(SingleThreadedExecutor &executor)
{
    switch (stage++)
    {
    case 0:
        mutex.is_acquired = false;
        if (mutex.waker->has_waiters())
            mutex.waker->wake_one(executor);
        return step_result::Wait(step_result::Wait::task_not_done,
                                 step_result::WaitForWaker(*cv.waker.get()));
    case 1:
        return step_result::Wait(
            step_result::Wait::task_automatically_done,
            step_result::WaitForChildTasks(
                make_vector_unique<Task>(MutexAcquireTask(mutex))));
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
        return step_result::Wait(step_result::Wait::task_not_done,
                                 step_result::WaitForWaker(*cv->waker.get()));
    case 1:
        return step_result::Wait(
            step_result::Wait::task_automatically_done,
            step_result::WaitForChildTasks(
                make_vector_unique<Task>(RcMutexAcquireTask(mutex))));
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

std::unique_ptr<CoroConditionVariableWaitTask>
condition_variable_wait_task(Rc<Mutex> mutex, Rc<ConditionVariable> cv)
{

    mutex->is_acquired = false;
    if (mutex->waker->has_waiters())
        mutex->waker->wake_one(co_await executor_awaiter);
    co_yield step_result::Wait(step_result::Wait::task_not_done,
                               step_result::WaitForWaker(*cv->waker.get()));

    co_yield step_result::Wait(
        step_result::Wait::task_automatically_done,
        step_result::WaitForChildTasks(
            make_vector_unique<Task>(RcMutexAcquireTask(mutex))));
}

std::unique_ptr<CoroConditionVariableNotifyTask>
condition_variable_notify_task(bool notify_all, Rc<ConditionVariable> cv)
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
