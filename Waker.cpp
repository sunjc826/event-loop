#include "Waker.h"
#include "Executor.h"
void FifoWaker::wake_one(SingleThreadedExecutor &executor)
{
    if (wait_queue.empty()) return;
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

void SingleTaskWaker::wake_one(SingleThreadedExecutor &executor) 
{
    if (sleeping_task == nullptr)
        return;
    
    executor.wake_sleeping_task(*sleeping_task);
}
