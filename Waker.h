#pragma once
#include "declarations.h"
#include "Task.h"
#include <cassert>
#include <stdexcept>
class Waker
{
protected:
public:
    virtual bool has_waiters() = 0;
    virtual void add_waiter(SleepingTask &sleeping_task) = 0;
    virtual void wake_one(SingleThreadedExecutor &executor) = 0;
    virtual void wake_all(SingleThreadedExecutor &executor) = 0;
    virtual ~Waker() {};
};
class FifoWaker final : public Waker
{
    std::queue<std::reference_wrapper<SleepingTask>> wait_queue; // no nullptrs
public:
    FifoWaker() = default;
    bool has_waiters() override
    {
        return not wait_queue.empty();
    }
    void add_waiter(SleepingTask &sleeping_task) override
    {
        wait_queue.push(sleeping_task);
    }
    void wake_one(SingleThreadedExecutor &executor) override;
    void wake_all(SingleThreadedExecutor &executor) override;
};

class SingleTaskWaker final : public Waker
{
    SleepingTask *sleeping_task;
public:
    SingleTaskWaker() = default;
    bool has_waiters() override { return sleeping_task != nullptr; }
    void add_waiter(SleepingTask &sleeping_task) override 
    { 
        if (this->sleeping_task != nullptr)
            throw std::runtime_error("SingleTaskWaker should only have 1 waiter");
        this->sleeping_task = &sleeping_task; 
    };
    void wake_one(SingleThreadedExecutor &executor) override;
    void wake_all(SingleThreadedExecutor &executor) override
    {
        wake_one(executor);
    }
};
