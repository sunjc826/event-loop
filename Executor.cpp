#include "Executor.h"
#include "StepResult.h"
#include "Task.h"
#include "Waker.h"
#include "declarations.h"
#include <memory>
#include <stdexcept>
#include <variant>

bool SingleThreadedExecutor::is_sleeping_task_list_empty()
{
    return sleeping_task_list->next == nullptr;
}

size_t SingleThreadedExecutor::number_of_sleeping_tasks()
{
    size_t count{};
    for (SleepingTask const *node = sleeping_task_list->next.get(); node != nullptr; node = node->next.get())
        count++;
    return count;
}

void SingleThreadedExecutor::print_tasks()
{
    std::cerr << "++++++++++++++\n";
    std::cerr << "++Sleeping tasks\n";
    for (SleepingTask const *node = sleeping_task_list.get(); node->next != nullptr; node = node->next.get())
        std::cerr << '\t' << node->task->name << '\n';
    std::cerr << "--\n";
    std::cerr << "++Awake tasks\n";
    for (std::unique_ptr<Task> const &task : tasks)
        std::cerr << '\t' << task->name << '\n';
    std::cerr << "--\n";
    std::cerr << "--------------\n";
}

void SingleThreadedExecutor::add_task(std::unique_ptr<Task> task)
{
    tasks.emplace_back(std::move(task));
}

void SingleThreadedExecutor::add_sleeping_task(std::unique_ptr<Task> task, Waker &waker, bool destroy_on_wake)
{
    sleeping_task_list = std::make_unique<SleepingTask>(std::move(sleeping_task_list), std::move(task), destroy_on_wake);
    waker.add_waiter(*sleeping_task_list);
}

void SingleThreadedExecutor::wake_sleeping_task(SleepingTask &sleeping_task)
{
    std::unique_ptr<SleepingTask> owned_sleeping_task;
    if (sleeping_task.prev)
    {
        if (sleeping_task.prev->next.get() != &sleeping_task)
            throw std::runtime_error("Unexpected");
        owned_sleeping_task = std::move(sleeping_task.prev->next);
        owned_sleeping_task->next->prev = owned_sleeping_task->prev;
        owned_sleeping_task->prev->next = std::move(owned_sleeping_task->next);
    }
    else
    {
        if (sleeping_task_list.get() != &sleeping_task)
            throw std::runtime_error("Unexpected");
        owned_sleeping_task = std::move(sleeping_task_list);
        sleeping_task_list = std::move(owned_sleeping_task->next);
        sleeping_task_list->prev = nullptr;
    }

    if (owned_sleeping_task->destroy_on_wake)
        owned_sleeping_task->task->done();
    else
        add_task(std::move(owned_sleeping_task->task));
}

ExecutorStepResult SingleThreadedExecutor::step()
{
    if (tasks.empty())
    {
        if (is_sleeping_task_list_empty())
            return ExecutorStepResult::done;
        else
            return ExecutorStepResult::done_with_tasks_sleeping;
    }
    std::unique_ptr<Task> task = std::move(tasks.front());
    tasks.pop_front();
    StepResult result = task->step(*this, std::move(task->last_child_return_values));
    std::optional<std::unique_ptr<void, TypeErasedDeleter>> return_value;
    if (auto *done = std::get_if<step_result::Done>(&result))
    {
        return_value.emplace(std::move(done->return_value));
        for (auto &child_task : done->child_tasks)
            tasks.emplace_back(std::move(child_task));
    }
    else if (auto *ready = std::get_if<step_result::Ready>(&result))
    {
        tasks.push_back(std::move(task));
        for (auto &child_task : ready->child_tasks)
            tasks.emplace_back(std::move(child_task));
    }
    else if (auto *wait = std::get_if<step_result::Wait>(&result))
    {
        bool const destroy_on_wake = wait->on_wait_finish == step_result::Wait::task_automatically_done;
        if (auto *wait_for_waker = std::get_if<step_result::WaitForWaker>(&wait->wait_for))
            add_sleeping_task(std::move(task), wait_for_waker->waker, destroy_on_wake);
        else if (auto *wait_for_child_tasks = std::get_if<step_result::WaitForChildTasks>(&wait->wait_for))
        {
            if (wait_for_child_tasks->tasks.empty())
                goto end;

            Counter counter(*this, std::make_unique<SingleTaskWaker>(), wait_for_child_tasks->tasks.size());
            auto &waker = counter.get_waker();
            task->last_child_return_values.clear();
            task->last_child_return_values.resize(wait_for_child_tasks->tasks.size());
            for (size_t i = wait_for_child_tasks->tasks.size(); i --> 0;)
            {
                std::unique_ptr<Task> &child_task = wait_for_child_tasks->tasks[i];
                child_task->parent_return_value_location = &task->last_child_return_values[i];
                if (i == 0)
                    child_task->on_done_callbacks.push_back(std::move(counter));
                else
                    child_task->on_done_callbacks.push_back(counter);
                tasks.emplace_back(std::move(child_task));
            }
            add_sleeping_task(std::move(task), waker, destroy_on_wake);
        }
    }
end:    
    if (task)
    {
        if (task->parent_return_value_location != nullptr and return_value.has_value())
            task->parent_return_value_location->emplace(*std::move(return_value));
        task->done();
    }
    return ExecutorStepResult::more_to_go;
}

void SingleThreadedExecutor::run_until_completion()
{
    ExecutorStepResult result;
    while ((result = step()) == ExecutorStepResult::more_to_go)
#       ifdef NDEBUG
        ;
#       else
        print_tasks();
#       endif
    if (result == ExecutorStepResult::done_with_tasks_sleeping)
    {
        std::cerr << "Warning: " << number_of_sleeping_tasks() << " tasks sleeping\n";
        print_tasks();
    }
}


