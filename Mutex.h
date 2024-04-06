#pragma once
#include "CoroutineTask.h"
#include "Rc.h"
#include "Task.h"
#include "Waker.h"

struct Mutex
{
    bool is_acquired = false;
    std::unique_ptr<Waker> waker; // mutex queue
    Mutex();
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
    RcMutexAcquireTask(Rc<Mutex> mutex)
        : Task("MutexAcquireTask"), mutex(std::move(mutex))
    {
    }
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
    RcMutexReleaseTask(Rc<Mutex> mutex)
        : Task("MutexReleaseTask"), mutex(std::move(mutex))
    {
    }
    StepResult step(SingleThreadedExecutor &) override;
};

struct CoroMutexAcquireTask;
struct CoroMutexAcquireTaskPromise
    : PromiseType<CoroMutexAcquireTaskPromise, CoroMutexAcquireTask>
{
    static std::string get_name() { return "CoroMutexAcquireTaskPromise"; }
};
struct CoroMutexAcquireTask final : CoroutineTask<CoroMutexAcquireTaskPromise>
{
    CoroMutexAcquireTask(std::string name, promise_type &promise)
        : CoroutineTask(std::move(name), promise)
    {
    }
};

std::unique_ptr<CoroMutexAcquireTask> mutex_acquire_task(Rc<Mutex> mutex);

struct CoroMutexReleaseTask;
struct CoroMutexReleaseTaskPromise final
    : PromiseType<CoroMutexReleaseTaskPromise, CoroMutexReleaseTask>
{
    static std::string get_name() { return "CoroMutexReleaseTaskPromise"; }
};
struct CoroMutexReleaseTask : public CoroutineTask<CoroMutexReleaseTaskPromise>
{
    CoroMutexReleaseTask(std::string name, promise_type &promise)
        : CoroutineTask(std::move(name), promise)
    {
    }
};
std::unique_ptr<CoroMutexReleaseTask> mutex_release_task(Rc<Mutex> mutex);
