#pragma once
#include "declarations.h"
#include "Task.h"
#include "StepResult.h"
#include <memory>

enum class ExecutorStepResult
{
    done,
    more_to_go,
    done_with_tasks_sleeping,
};

class Counter
{
    SingleThreadedExecutor &executor;
    struct Owned
    {
        std::unique_ptr<Waker> waker;
        size_t count;
    };
    std::shared_ptr<Owned> shared;
public:
    Counter(SingleThreadedExecutor &executor, std::unique_ptr<Waker> waker, size_t count)
        : executor(executor), shared(std::make_shared<Owned>(std::move(waker), count))
    {
    }
    Waker &get_waker()
    {
        return *shared->waker;
    }
    void
    operator()()
    {
        if (--shared->count == 0)
            shared->waker->wake_one(executor);
    }
};

class SingleThreadedExecutor
{
    std::unique_ptr<SleepingTask> sleeping_task_list;
    std::deque<std::unique_ptr<Task>> tasks;
    bool is_sleeping_task_list_empty();
    size_t number_of_sleeping_tasks();
    
    void add_sleeping_task(std::unique_ptr<Task> task, Waker &waker, bool destroy_on_wake);
public:
    SingleThreadedExecutor()
        : sleeping_task_list(std::make_unique<SleepingTask>()) // head sentinel
    {
    }
    void print_tasks();
    void add_task(std::unique_ptr<Task>);
    void wake_sleeping_task(SleepingTask &sleeping_task);
    ExecutorStepResult step();
    void run_until_completion();
};

// TODO
class SingleThreadedCoroutineExecutor
{

};
