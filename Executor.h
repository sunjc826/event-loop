#pragma once
#include "Task.h"
#include "Waker.h"
#include <deque>
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
    Counter(SingleThreadedExecutor &executor, std::unique_ptr<Waker> waker,
            size_t count);
    Waker &get_waker();
    void operator()();
};

class SingleThreadedExecutor
{
    std::unique_ptr<SleepingTask> sleeping_task_list;
    std::deque<std::unique_ptr<Task>> tasks;
    bool is_sleeping_task_list_empty();
    size_t number_of_sleeping_tasks();
    void handle_wait(std::unique_ptr<Task>, step_result::Wait &);
    void add_sleeping_task(std::unique_ptr<Task> task, Waker &waker,
                           bool destroy_on_wake);

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

// We may want to implement abstract class Executor in future
using Executor = SingleThreadedExecutor;