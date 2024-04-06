#pragma once
#include "CoroutineTask.h"
#include "Mutex.h"
#include "Rc.h"
#include "Task.h"
#include "Waker.h"

struct ConditionVariable
{
    std::unique_ptr<Waker> waker; // cv queue
    ConditionVariable();
};

class ConditionVariableWaitTask final : public Task
{
    Mutex &mutex;
    ConditionVariable &cv;
    unsigned stage = 0;

public:
    ConditionVariableWaitTask(Mutex &mutex, ConditionVariable &cv)
        : Task("ConditionVariableWaitTask"), mutex(mutex), cv(cv)
    {
    }
    StepResult step(SingleThreadedExecutor &) override;
};

class RcConditionVariableWaitTask final : public Task
{
    Rc<Mutex> mutex;
    Rc<ConditionVariable> cv;
    unsigned stage = 0;

public:
    RcConditionVariableWaitTask(Rc<Mutex> mutex, Rc<ConditionVariable> cv)
        : Task("ConditionVariableWaitTask"), mutex(std::move(mutex)),
          cv(std::move(cv))
    {
    }
    StepResult step(SingleThreadedExecutor &) override;
};

class ConditionVariableNotifyTask final : public Task
{
    bool notify_all;
    ConditionVariable &cv;

public:
    ConditionVariableNotifyTask(bool notify_all, ConditionVariable &cv)
        : Task("ConditionVariableNotifyTask"), notify_all(notify_all), cv(cv)
    {
    }
    StepResult step(SingleThreadedExecutor &) override;
};

class RcConditionVariableNotifyTask final : public Task
{
    bool notify_all;
    Rc<ConditionVariable> cv;

public:
    RcConditionVariableNotifyTask(bool notify_all, Rc<ConditionVariable> cv)
        : Task("ConditionVariableNotifyTask"), notify_all(notify_all),
          cv(std::move(cv))
    {
    }
    StepResult step(SingleThreadedExecutor &) override;
};

struct CoroConditionVariableWaitTask;
struct CoroConditionVariableWaitTaskPromise final
    : PromiseType<CoroConditionVariableWaitTaskPromise,
                  CoroConditionVariableWaitTask>
{
    static std::string get_name()
    {
        return "CoroConditionVariableWaitTaskPromise";
    }
};
struct CoroConditionVariableWaitTask
    : public CoroutineTask<CoroConditionVariableWaitTaskPromise>
{
    CoroConditionVariableWaitTask(std::string name, promise_type &promise)
        : CoroutineTask(std::move(name), promise)
    {
    }
};
std::unique_ptr<CoroConditionVariableWaitTask>
condition_variable_wait_task(Rc<Mutex> mutex, Rc<ConditionVariable> cv);

struct CoroConditionVariableNotifyTask;
struct CoroConditionVariableNotifyTaskPromise final
    : PromiseType<CoroConditionVariableNotifyTaskPromise,
                  CoroConditionVariableNotifyTask>
{
    static std::string get_name()
    {
        return "CoroConditionVariableNotifyTaskPromise";
    }
};
struct CoroConditionVariableNotifyTask
    : public CoroutineTask<CoroConditionVariableNotifyTaskPromise>
{
    CoroConditionVariableNotifyTask(std::string name, promise_type &promise)
        : CoroutineTask(std::move(name), promise)
    {
    }
};
std::unique_ptr<CoroConditionVariableNotifyTask>
condition_variable_notify_task(bool notify_all, Rc<ConditionVariable> cv);
