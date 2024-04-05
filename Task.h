#pragma once
#include "Waker.h"
#include "declarations.h"
#include "StepResult.h"
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <stdexcept>
#include <sys/epoll.h>
#include <unistd.h>
struct Task
{
    friend class SingleThreadedExecutor;
private:
    std::string name;
    std::vector<std::function<void()>> on_done_callbacks;
    std::optional<std::unique_ptr<void, TypeErasedDeleter>> *parent_return_value_location = nullptr;
    std::vector<std::optional<std::unique_ptr<void, TypeErasedDeleter>>> last_child_return_values;
public:
    Task(std::string name) : name(std::move(name)) {}
    Task(Task const &) = delete;
    Task(Task &&) = default;
    virtual StepResult step(SingleThreadedExecutor &executor) { return step_result::Done(); }
    virtual StepResult step(SingleThreadedExecutor &executor, std::vector<std::optional<std::unique_ptr<void, TypeErasedDeleter>>> child_return_values)
    {
        return step(executor);
    }
    void done()
    {
        for (auto &callback : on_done_callbacks)
            callback();
    }
    virtual ~Task()
    {
    }
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
    SleepingTask(std::unique_ptr<SleepingTask> next, std::unique_ptr<Task> task, bool destroy_on_wake);
};

struct Mutex
{
    bool is_acquired = false;
    std::unique_ptr<Waker> waker; // mutex queue
    Mutex();
};

template <typename T>
struct MutexGuardedObject
{
    Mutex mutex;
    T object;
    MutexGuardedObject()
        : object()
    {
    }
    MutexGuardedObject(T object)
        : object(std::move(object))
    {
    }
};

struct ConditionVariable
{
    std::unique_ptr<Waker> waker; // cv queue
    ConditionVariable();
};

class MutexAcquireTask final : public Task
{
    Mutex &mutex;
public:
    MutexAcquireTask(Mutex &mutex) : Task("MutexAcquireTask"), mutex(mutex) {}
    StepResult step(SingleThreadedExecutor &) override;
};

class RcMutexAcquireTask final : public Task
{
    Rc<Mutex> mutex;
public:
    RcMutexAcquireTask(Rc<Mutex> mutex) : Task("MutexAcquireTask"), mutex(std::move(mutex)) {}
    StepResult step(SingleThreadedExecutor &) override;
};

class MutexReleaseTask final : public Task
{
    Mutex &mutex;
public:
    MutexReleaseTask(Mutex &mutex) : Task("MutexReleaseTask"), mutex(mutex) {}
    StepResult step(SingleThreadedExecutor &) override;
};

class RcMutexReleaseTask final : public Task
{
    Rc<Mutex> mutex;
public:
    RcMutexReleaseTask(Rc<Mutex> mutex) : Task("MutexReleaseTask"), mutex(std::move(mutex)) {}
    StepResult step(SingleThreadedExecutor &) override; 
};

class ConditionVariableWaitTask final : public Task
{
    Mutex &mutex;
    ConditionVariable &cv;
    unsigned stage = 0;
public:
    ConditionVariableWaitTask(Mutex &mutex, ConditionVariable &cv) : Task("ConditionVariableWaitTask"), mutex(mutex), cv(cv) {}
    StepResult step(SingleThreadedExecutor &) override;
};

class RcConditionVariableWaitTask final : public Task
{
    Rc<Mutex> mutex;
    Rc<ConditionVariable> cv;
    unsigned stage = 0;
public:
    RcConditionVariableWaitTask(Rc<Mutex> mutex, Rc<ConditionVariable> cv) : Task("ConditionVariableWaitTask"), mutex(std::move(mutex)), cv(std::move(cv)) {}
    StepResult step(SingleThreadedExecutor &) override;
};

class ConditionVariableNotifyTask final : public Task
{
    bool notify_all;
    ConditionVariable &cv;
public:
    ConditionVariableNotifyTask(bool notify_all, ConditionVariable &cv) 
        : Task("ConditionVariableNotifyTask"), notify_all(notify_all), cv(cv) {}
    StepResult step(SingleThreadedExecutor &) override;
};

class RcConditionVariableNotifyTask final : public Task
{
    bool notify_all;
    Rc<ConditionVariable> cv;
public:
    RcConditionVariableNotifyTask(bool notify_all, Rc<ConditionVariable> cv) 
        : Task("ConditionVariableNotifyTask"), notify_all(notify_all), cv(std::move(cv)) {}
    StepResult step(SingleThreadedExecutor &) override;
};

class EpollTask : public Task
{
    int epoll_fd;
protected:
    void execute(SingleThreadedExecutor &);
public:
    EpollTask(int epoll_fd) : Task("EpollTask"), epoll_fd(epoll_fd) {}
    StepResult step(SingleThreadedExecutor &) override;
    virtual ~EpollTask()
    {
        if (close(epoll_fd) == -1)
            perror("close epoll_fd: ");
    }
};

class Handler 
{
    int fd;
public:
    // nullptr means no Task needed to be spawned
    virtual std::unique_ptr<Task> handle(uint32_t active_events) = 0;   
};
