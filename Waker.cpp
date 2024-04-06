#include "Waker.h"
#include "Executor.h"
bool FifoWaker::has_waiters() { return not wait_queue.empty(); }

void FifoWaker::add_waiter(SleepingTask &sleeping_task)
{
    wait_queue.push(sleeping_task);
}

void FifoWaker::wake_one(SingleThreadedExecutor &executor)
{
    if (wait_queue.empty())
        return;
    executor.wake_sleeping_task(wait_queue.front());
    wait_queue.pop();
}

void FifoWaker::wake_all(SingleThreadedExecutor &executor)
{
    while (not wait_queue.empty())
    {
        executor.wake_sleeping_task(wait_queue.front());
        wait_queue.pop();
    }
}

void SingleTaskWaker::add_waiter(SleepingTask &sleeping_task)
{
    if (this->sleeping_task != nullptr)
        throw std::runtime_error("SingleTaskWaker should only have 1 waiter");
    this->sleeping_task = &sleeping_task;
}

void SingleTaskWaker::wake_one(SingleThreadedExecutor &executor)
{
    if (sleeping_task == nullptr)
        return;

    executor.wake_sleeping_task(*sleeping_task);
}

void SingleTaskWaker::wake_all(SingleThreadedExecutor &executor)
{
    wake_one(executor);
}