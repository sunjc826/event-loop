#pragma once
#include "Executor.decl.h"
#include "StepResult.decl.h"
#include "Waker.decl.h"
#include "utilities.h"
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <sys/epoll.h>
#include <unistd.h>
struct Task
{
    friend class SingleThreadedExecutor;

private:
    std::string name;
    std::vector<std::function<void()>> on_done_callbacks;
    std::optional<std::unique_ptr<void, TypeErasedDeleter>>
        *parent_return_value_location = nullptr;
    std::vector<std::optional<std::unique_ptr<void, TypeErasedDeleter>>>
        last_child_return_values;

public:
    Task(std::string name) : name(std::move(name)) {}
    Task(Task const &) = delete;
    Task(Task &&) noexcept = default;
    virtual StepResult step(SingleThreadedExecutor &executor);
    virtual StepResult step_with_result(
        SingleThreadedExecutor &executor,
        std::vector<std::optional<std::unique_ptr<void, TypeErasedDeleter>>>
            child_return_values);
    void done()
    {
        for (auto &callback : on_done_callbacks)
            callback();
    }
    virtual ~Task() {}
};

class SleepingTask
{
    friend class SingleThreadedExecutor;
    SleepingTask *prev = nullptr;
    std::unique_ptr<SleepingTask> next;

public:
    std::unique_ptr<Task> task;
    bool destroy_on_wake = false;
    SleepingTask() : prev() {}
    SleepingTask(std::unique_ptr<SleepingTask> next, std::unique_ptr<Task> task,
                 bool destroy_on_wake);
};

class EpollTask : public Task
{
    int epoll_fd;

protected:
    void execute(SingleThreadedExecutor &);

public:
    EpollTask(int epoll_fd) : Task("EpollTask"), epoll_fd(epoll_fd) {}
    StepResult step(SingleThreadedExecutor &) override;
    ~EpollTask()
    {
        if (close(epoll_fd) == -1)
            perror("close epoll_fd: ");
    }
};

class Handler
{
protected:
    int fd;

public:
    // nullptr means no Task needed to be spawned
    virtual std::unique_ptr<Task> handle(uint32_t active_events) = 0;
};
