#pragma once
#include "Executor.decl.h"
#include "Task.decl.h"
#include <queue>
#include <stdexcept>

class Waker
{
protected:
public:
    virtual bool has_waiters() = 0;
    virtual void add_waiter(SleepingTask &sleeping_task) = 0;
    virtual void wake_one(SingleThreadedExecutor &executor) = 0;
    virtual void wake_all(SingleThreadedExecutor &executor) = 0;
    virtual ~Waker(){};
};
class FifoWaker final : public Waker
{
    std::queue<std::reference_wrapper<SleepingTask>> wait_queue; // no nullptrs
public:
    FifoWaker() = default;
    bool has_waiters() override;
    void add_waiter(SleepingTask &sleeping_task) override;
    void wake_one(SingleThreadedExecutor &executor) override;
    void wake_all(SingleThreadedExecutor &executor) override;
};

class SingleTaskWaker : public Waker
{
protected:
    SleepingTask *sleeping_task;

public:
    SingleTaskWaker() = default;
    bool has_waiters() override { return sleeping_task != nullptr; }
    void add_waiter(SleepingTask &sleeping_task) override;
    void wake_one(SingleThreadedExecutor &executor) override;
    void wake_all(SingleThreadedExecutor &executor) override;
};

class ReusableSingleTaskWaker final : public SingleTaskWaker
{
public:
    ReusableSingleTaskWaker() = default;
    void wake_one(SingleThreadedExecutor &executor) override;
};
