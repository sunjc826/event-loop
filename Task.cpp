#include "Task.h"
#include "Executor.h"
#include "StepResult.h"
#include <memory>
#include <stdexcept>
#include <sys/epoll.h>
#include <span>
#include <cstring>

StepResult Task::step(SingleThreadedExecutor &executor) 
{ 
    return step_result::Done(); 
}

StepResult Task::step_with_result(SingleThreadedExecutor &executor, std::vector<std::optional<std::unique_ptr<void, TypeErasedDeleter>>> child_return_values)
{
    return step(executor);
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
