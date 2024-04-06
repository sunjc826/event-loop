#include "CompositeTask.h"
#include "ConditionVariable.h"
#include "CoroutineTask.h"
#include "Executor.h"
#include "Mutex.h"
#include "Rc.h"
#include "StepResult.h"
#include "Task.h"
#include "utilities.h"

#include <algorithm>
#include <cstdlib>
#include <memory>
#include <stdexcept>

template <typename T>
struct MutexCvObject
{
    Mutex mutex;
    ConditionVariable cv;
    T object;
    MutexCvObject() : object() {}
    MutexCvObject(T object) : object(std::move(object)) {}
};

namespace queue_test
{
class DequeueTask final : public Task
{
    MutexCvObject<std::queue<int>> &queue;
    unsigned state = 0;

public:
    DequeueTask(MutexCvObject<std::queue<int>> &queue)
        : Task("DequeueTask"), queue(queue)
    {
    }
    StepResult step(SingleThreadedExecutor &executor) override
    {
        switch (state++)
        {
        case 0:
        {
            return step_result::Wait(
                step_result::Wait::task_not_done,
                make_vector_unique<Task>(MutexAcquireTask(queue.mutex)));
        }
        case 1:
        {
            if (not queue.object.empty())
            {
                int element = queue.object.front();
                queue.object.pop();
                std::cerr << "Pop: " << element << '\n';
            }
            return step_result::Wait(
                step_result::Wait::task_automatically_done,
                make_vector_unique<Task>(MutexReleaseTask(queue.mutex)));
        }
        default:
            throw std::runtime_error("Unreachable");
        }
    }
};
class GuaranteedDequeueTask final : public Task
{
    MutexCvObject<std::queue<int>> &queue;
    enum class State
    {
        acquiring,
        dequeuing,
    };
    State state = State::acquiring;

public:
    GuaranteedDequeueTask(MutexCvObject<std::queue<int>> &queue)
        : Task("GuaranteedDequeueTask"), queue(queue)
    {
    }
    StepResult step(SingleThreadedExecutor &executor) override
    {
        switch (state)
        {
        case State::acquiring:
        {
            state = State::dequeuing;
            return step_result::Wait(
                step_result::Wait::task_not_done,
                make_vector_unique<Task>(MutexAcquireTask(queue.mutex)));
        }
        case State::dequeuing:
        {
            if (queue.object.empty())
                return step_result::Wait(
                    step_result::Wait::task_not_done,
                    make_vector_unique<Task>(
                        ConditionVariableWaitTask(queue.mutex, queue.cv)));
            int element = queue.object.front();
            queue.object.pop();
            std::cerr << "Pop: " << element << '\n';
            return step_result::Wait(
                step_result::Wait::task_automatically_done,
                make_vector_unique<Task>(MutexReleaseTask(queue.mutex)));
        }
        default:
            throw std::runtime_error("Unreachable");
        }
    }
};
class EnqueueTask final : public Task
{
    MutexCvObject<std::queue<int>> &queue;
    int element;
    unsigned state = 0;

public:
    EnqueueTask(MutexCvObject<std::queue<int>> &queue, int element)
        : Task("EnqueueTask"), queue(queue), element(element)
    {
    }
    StepResult step(SingleThreadedExecutor &executor) override
    {
        switch (state++)
        {
        case 0:
        {
            return step_result::Wait(
                step_result::Wait::task_not_done,
                make_vector_unique<Task>(MutexAcquireTask(queue.mutex)));
        }
        case 1:
        {
            queue.object.push(element);
            return step_result::Wait(
                step_result::Wait::task_automatically_done,
                make_vector_unique<Task>(
                    MutexReleaseTask(queue.mutex),
                    ConditionVariableNotifyTask(true, queue.cv)));
        }
        default:
            throw std::runtime_error("Unreachable");
        }
    }
};
template <bool chain_mode>
class EnqueueTaskChain final : public Task
{
    template <bool>
    friend class EnqueueTaskChain;
    MutexCvObject<std::queue<int>> &queue;
    std::vector<int> stack;
    enum Stage
    {
        acquire,
        enqueue,
    };
    Stage stage;

public:
    EnqueueTaskChain(MutexCvObject<std::queue<int>> &queue,
                     std::vector<int> stack, Stage stage)
        : Task("EnqueueTaskChain"), queue(queue), stack(std::move(stack)),
          stage(stage)
    {
    }
    static std::unique_ptr<EnqueueTaskChain>
    create(MutexCvObject<std::queue<int>> &queue,
           std::vector<int> elements_to_enqueue)
    {
        std::ranges::reverse(elements_to_enqueue);
        return std::make_unique<EnqueueTaskChain>(
            queue, std::move(elements_to_enqueue), Stage::acquire);
    }

    StepResult step(SingleThreadedExecutor &) override
    {
        switch (stage)
        {
        case Stage::acquire:
        {
            if (stack.empty())
                return step_result::Done();
            stage = Stage::enqueue;
            return step_result::Wait(
                step_result::Wait::task_not_done,
                make_vector_unique<Task>(MutexAcquireTask(queue.mutex)));
        }
        case Stage::enqueue:
        {
            if (stack.empty())
                return step_result::Wait(
                    step_result::Wait::task_automatically_done,
                    make_vector_unique<Task>(
                        MutexReleaseTask(queue.mutex),
                        ConditionVariableNotifyTask(true, queue.cv)));
            int element = stack.back();
            queue.object.push(element);
            stack.pop_back();
            if constexpr (chain_mode)
                return step_result::Wait(
                    step_result::Wait::task_automatically_done,
                    make_vector_unique<Task>(EnqueueTaskChain(
                        queue, std::move(stack), Stage::enqueue)));
            else
                return step_result::Ready();
        }
        default:
            throw std::runtime_error("Should not be reached");
        }
    }
};
class MainTask final : public Task
{
    unsigned state = 0;
    MutexCvObject<std::queue<int>> queue;

public:
    MainTask() : Task("MainTask") {}
    StepResult step(SingleThreadedExecutor &executor) override
    {
        switch (state++)
        {
        case 0:
        {
            // spawn some tasks
            auto chain1 = EnqueueTaskChain<true>::create(queue, {1, 2, 3, 4});
            auto chain2 =
                EnqueueTaskChain<false>::create(queue, {1, 2, 3, 4, 5});
            auto chain3 = EnqueueTaskChain<true>::create(
                queue, {1, 2, 3, 4, 5, 6, 7, 8, 9, 10});
            auto chain4 = std::make_unique<EnqueueTask>(queue, 100);
            auto dequeue1 = std::make_unique<GuaranteedDequeueTask>(queue);
            auto dequeue2 = std::make_unique<GuaranteedDequeueTask>(queue);
            return step_result::Wait(
                step_result::Wait::task_not_done,
                make_vector_unique<Task>(std::move(chain1), std::move(chain2),
                                         std::move(chain3), std::move(chain4),
                                         std::move(dequeue1),
                                         std::move(dequeue2)));
        }
        case 1:
        {
            std::vector<std::unique_ptr<Task>> tasks;
            for (int i = 0; i < 18; ++i)
                tasks.push_back(std::make_unique<GuaranteedDequeueTask>(queue));
            return step_result::Wait(step_result::Wait::task_automatically_done,
                                     std::move(tasks));
        }
        default:
            throw std::runtime_error("Unreachable");
        }
    }
};
} // namespace queue_test

namespace rc_queue_test
{
class DequeueTask final : public Task
{
    Rc<MutexCvObject<std::queue<int>>> queue;
    unsigned state = 0;

public:
    DequeueTask(Rc<MutexCvObject<std::queue<int>>> queue)
        : Task("DequeueTask"), queue(std::move(queue))
    {
    }
    StepResult step(SingleThreadedExecutor &executor) override
    {
        switch (state++)
        {
        case 0:
        {
            return step_result::Wait(
                step_result::Wait::task_not_done,
                make_vector_unique<Task>(
                    RcMutexAcquireTask(Rc<Mutex>(queue, &queue->mutex))));
        }
        case 1:
        {
            if (not queue->object.empty())
            {
                int element = queue->object.front();
                queue->object.pop();
                std::cerr << "Pop: " << element << '\n';
            }
            return step_result::Done(make_vector_unique<Task>(
                RcMutexReleaseTask(Rc<Mutex>(queue, &queue->mutex))));
        }
        default:
            throw std::runtime_error("Unreachable");
        }
    }
};
class GuaranteedDequeueTask final : public Task
{
    Rc<MutexCvObject<std::queue<int>>> queue;
    enum class State
    {
        acquiring,
        dequeuing,
    };
    State state = State::acquiring;

public:
    GuaranteedDequeueTask(Rc<MutexCvObject<std::queue<int>>> queue)
        : Task("GuaranteedDequeueTask"), queue(std::move(queue))
    {
    }
    StepResult step(SingleThreadedExecutor &executor) override
    {
        switch (state)
        {
        case State::acquiring:
        {
            state = State::dequeuing;
            return step_result::Wait(
                step_result::Wait::task_not_done,
                make_vector_unique<Task>(
                    RcMutexAcquireTask(Rc<Mutex>(queue, &queue->mutex))));
        }
        case State::dequeuing:
        {
            if (queue->object.empty())
                return step_result::Wait(
                    step_result::Wait::task_not_done,
                    make_vector_unique<Task>(RcConditionVariableWaitTask(
                        Rc<Mutex>(queue, &queue->mutex),
                        Rc<ConditionVariable>(queue, &queue->cv))));
            int element = queue->object.front();
            queue->object.pop();
            std::cerr << "Pop: " << element << '\n';
            return step_result::Done(make_vector_unique<Task>(
                RcMutexReleaseTask(Rc<Mutex>(queue, &queue->mutex))));
        }
        default:
            throw std::runtime_error("Unreachable");
        }
    }
};
class EnqueueTask final : public Task
{
    Rc<MutexCvObject<std::queue<int>>> queue;
    int element;
    unsigned state = 0;

public:
    EnqueueTask(Rc<MutexCvObject<std::queue<int>>> queue, int element)
        : Task("EnqueueTask"), queue(std::move(queue)), element(element)
    {
    }
    StepResult step(SingleThreadedExecutor &executor) override
    {
        switch (state++)
        {
        case 0:
        {
            return step_result::Wait(
                step_result::Wait::task_not_done,
                make_vector_unique<Task>(
                    RcMutexAcquireTask(Rc<Mutex>(queue, &queue->mutex))));
        }
        case 1:
        {
            queue->object.push(element);
            return step_result::Done(make_vector_unique<Task>(
                RcMutexReleaseTask(Rc<Mutex>(queue, &queue->mutex)),
                RcConditionVariableNotifyTask(
                    true, Rc<ConditionVariable>(queue, &queue->cv))));
        }
        default:
            throw std::runtime_error("Unreachable");
        }
    }
};
template <bool chain_mode>
class EnqueueTaskChain final : public Task
{
    template <bool>
    friend class EnqueueTaskChain;

    Rc<MutexCvObject<std::queue<int>>> queue;
    std::vector<int> stack;
    enum Stage
    {
        acquire,
        enqueue,
    };
    Stage stage;

public:
    EnqueueTaskChain(Rc<MutexCvObject<std::queue<int>>> queue,
                     std::vector<int> stack, Stage stage)
        : Task("EnqueueTaskChain"), queue(std::move(queue)),
          stack(std::move(stack)), stage(stage)
    {
    }
    static std::unique_ptr<EnqueueTaskChain>
    create(Rc<MutexCvObject<std::queue<int>>> queue,
           std::vector<int> elements_to_enqueue)
    {
        std::ranges::reverse(elements_to_enqueue);
        return std::make_unique<EnqueueTaskChain>(
            std::move(queue), std::move(elements_to_enqueue), Stage::acquire);
    }

    static EnqueueTaskChain<chain_mode>
    create_non_ptr(Rc<MutexCvObject<std::queue<int>>> queue,
                   std::vector<int> elements_to_enqueue)
    {
        std::ranges::reverse(elements_to_enqueue);
        return EnqueueTaskChain(std::move(queue),
                                std::move(elements_to_enqueue),
                                EnqueueTaskChain<chain_mode>::Stage::acquire);
    }

    StepResult step(SingleThreadedExecutor &) override
    {
        switch (stage)
        {
        case Stage::acquire:
        {
            if (stack.empty())
                return step_result::Done();
            stage = Stage::enqueue;
            return step_result::Wait(
                step_result::Wait::task_not_done,
                make_vector_unique<Task>(
                    RcMutexAcquireTask(Rc<Mutex>(queue, &queue->mutex))));
        }
        case Stage::enqueue:
        {
            if (stack.empty())
                return step_result::Done(make_vector_unique<Task>(
                    RcMutexReleaseTask(Rc<Mutex>(queue, &queue->mutex)),
                    RcConditionVariableNotifyTask(
                        true, Rc<ConditionVariable>(queue, &queue->cv))));
            int element = stack.back();
            queue->object.push(element);
            stack.pop_back();
            if constexpr (chain_mode)
                return step_result::Done(make_vector_unique<Task>(
                    EnqueueTaskChain(queue, std::move(stack), Stage::enqueue)));
            else
                return step_result::Ready();
        }
        default:
            throw std::runtime_error("Should not be reached");
        }
    }
};
class MainTask final : public Task
{
    unsigned state = 0;
    Rc<MutexCvObject<std::queue<int>>> queue;

public:
    MainTask()
        : Task("MainTask"), queue(Rc<MutexCvObject<std::queue<int>>>::create())
    {
    }
    StepResult step(SingleThreadedExecutor &executor) override
    {
        switch (state++)
        {
        case 0:
        {
            // spawn some tasks
            auto chain1 = EnqueueTaskChain<true>::create(queue, {1, 2, 3, 4});
            auto chain2 =
                EnqueueTaskChain<false>::create(queue, {1, 2, 3, 4, 5});
            auto chain3 = EnqueueTaskChain<true>::create(
                queue, {1, 2, 3, 4, 5, 6, 7, 8, 9, 10});
            auto chain4 = std::make_unique<EnqueueTask>(queue, 100);
            auto dequeue1 = std::make_unique<GuaranteedDequeueTask>(queue);
            auto dequeue2 = std::make_unique<GuaranteedDequeueTask>(queue);
            return step_result::Wait(
                step_result::Wait::task_not_done,
                make_vector_unique<Task>(std::move(chain1), std::move(chain2),
                                         std::move(chain3), std::move(chain4),
                                         std::move(dequeue1),
                                         std::move(dequeue2)));
        }
        case 1:
        {
            std::vector<std::unique_ptr<Task>> tasks;
            for (int i = 0; i < 18; ++i)
                tasks.push_back(std::make_unique<GuaranteedDequeueTask>(queue));
            /* Notice that because we use Rc, this is safe */
            return step_result::Done(std::move(tasks));
        }
        default:
            throw std::runtime_error("Unreachable");
        }
    }
};
} // namespace rc_queue_test

namespace queue_coroutine_test
{
struct DequeueTask;
struct DequeueTaskPromiseType final
    : PromiseType<DequeueTaskPromiseType, DequeueTask>
{
    static std::string get_name() { return "DequeueTaskPromiseType"; }
};
struct DequeueTask final : public CoroutineTask<DequeueTaskPromiseType>
{
    DequeueTask(std::string name, promise_type &promise)
        : CoroutineTask(std::move(name), promise)
    {
    }
};

std::unique_ptr<DequeueTask>
dequeue_task(Rc<MutexCvObject<std::queue<int>>> queue)
{
    co_yield step_result::Wait(step_result::Wait::task_not_done,
                               make_vector_unique<Task>(mutex_acquire_task(
                                   Rc<Mutex>(queue, &queue->mutex))));

    if (queue->object.empty())
    {
        co_yield step_result::Done(make_vector_unique<Task>(
            mutex_release_task(Rc<Mutex>(queue, &queue->mutex))));
        co_return;
    }
    int element = queue->object.front();
    queue->object.pop();
    std::cerr << "Pop: " << element << '\n';
    co_yield step_result::Wait(step_result::Wait::task_automatically_done,
                               make_vector_unique<Task>(mutex_release_task(
                                   Rc<Mutex>(queue, &queue->mutex))));
}

struct GuaranteedDequeueTask;
struct GuaranteedDequeueTaskPromiseType final
    : PromiseType<GuaranteedDequeueTaskPromiseType, GuaranteedDequeueTask>
{
    static std::string get_name() { return "GuaranteedDequeueTaskPromiseType"; }
};
struct GuaranteedDequeueTask : CoroutineTask<GuaranteedDequeueTaskPromiseType>
{
    GuaranteedDequeueTask(std::string name, promise_type &promise)
        : CoroutineTask(std::move(name), promise)
    {
    }
};
std::unique_ptr<GuaranteedDequeueTask>
guaranteed_dequeue_task(Rc<MutexCvObject<std::queue<int>>> queue)
{
    co_yield step_result::Wait(step_result::Wait::task_not_done,
                               make_vector_unique<Task>(mutex_acquire_task(
                                   Rc<Mutex>(queue, &queue->mutex))));

    while (queue->object.empty())
        co_yield step_result::Wait(
            step_result::Wait::task_not_done,
            make_vector_unique<Task>(condition_variable_wait_task(
                Rc<Mutex>(queue, &queue->mutex),
                Rc<ConditionVariable>(queue, &queue->cv))));
    int element = queue->object.front();
    queue->object.pop();
    std::cerr << "Pop: " << element << '\n';
    co_yield step_result::Done(make_vector_unique<Task>(
        mutex_release_task(Rc<Mutex>(queue, &queue->mutex))));
}

struct EnqueueTask;
struct EnqueueTaskPromiseType final
    : PromiseType<EnqueueTaskPromiseType, EnqueueTask>
{
    static std::string get_name() { return "EnqueueTaskPromiseType"; }
};
struct EnqueueTask final : public CoroutineTask<EnqueueTaskPromiseType>
{
    EnqueueTask(std::string name, promise_type &promise)
        : CoroutineTask(std::move(name), promise)
    {
    }
};

std::unique_ptr<EnqueueTask>
enqueue_task(Rc<MutexCvObject<std::queue<int>>> queue, int element)
{
    co_yield step_result::Wait(step_result::Wait::task_not_done,
                               make_vector_unique<Task>(mutex_acquire_task(
                                   Rc<Mutex>(queue, &queue->mutex))));

    queue->object.push(element);
    co_yield step_result::Wait(
        step_result::Wait::task_automatically_done,
        make_vector_unique<Task>(
            mutex_release_task(Rc<Mutex>(queue, &queue->mutex)),
            condition_variable_notify_task(
                true, Rc<ConditionVariable>(queue, &queue->cv))));
}
struct EnqueueTaskChain;
struct EnqueueTaskChainPromiseType final
    : PromiseType<EnqueueTaskChainPromiseType, EnqueueTaskChain>
{
    static std::string get_name() { return "EnqueueTaskChainPromiseType"; }
};
struct EnqueueTaskChain final
    : public CoroutineTask<EnqueueTaskChainPromiseType>
{
    EnqueueTaskChain(std::string name, promise_type &promise)
        : CoroutineTask(std::move(name), promise)
    {
    }
};

std::unique_ptr<EnqueueTaskChain>
enqueue_task_chain(Rc<MutexCvObject<std::queue<int>>> queue,
                   std::vector<int> elements_to_enqueue)
{
    co_yield step_result::Wait(step_result::Wait::task_not_done,
                               make_vector_unique<Task>(RcMutexAcquireTask(
                                   Rc<Mutex>(queue, &queue->mutex))));

    for (int element : elements_to_enqueue)
    {
        queue->object.push(element);
        co_yield step_result::Ready();
    }

    co_yield step_result::Done(make_vector_unique<Task>(
        mutex_release_task(Rc<Mutex>(queue, &queue->mutex)),
        condition_variable_notify_task(
            true, Rc<ConditionVariable>(queue, &queue->cv))));
}

struct MainTask;
struct MainTaskPromiseType final : PromiseType<MainTaskPromiseType, MainTask>
{
    static std::string get_name() { return "MainTask"; }
};
struct MainTask final : public CoroutineTask<MainTaskPromiseType>
{
    MainTask(std::string name, promise_type &promise)
        : CoroutineTask(std::move(name), promise)
    {
    }
};

std::unique_ptr<MainTask> main_task()
{
    auto queue = Rc<MutexCvObject<std::queue<int>>>::create();
    {
        auto chain1 = enqueue_task_chain(queue, {1, 2, 3, 4});
        auto chain2 = enqueue_task_chain(queue, {1, 2, 3, 4, 5});
        auto chain3 =
            enqueue_task_chain(queue, {1, 2, 3, 4, 5, 6, 7, 8, 9, 10});
        auto chain4 = enqueue_task(queue, 100);
        auto dequeue1 = guaranteed_dequeue_task(queue);
        auto dequeue2 = guaranteed_dequeue_task(queue);
        co_yield step_result::Wait(
            step_result::Wait::task_not_done,
            make_vector_unique<Task>(std::move(chain1), std::move(chain2),
                                     std::move(chain3), std::move(chain4),
                                     std::move(dequeue1), std::move(dequeue2)));
    }
    {
        std::vector<std::unique_ptr<Task>> tasks;
        for (int i = 0; i < 18; ++i)
            tasks.emplace_back(guaranteed_dequeue_task(queue));
        co_yield step_result::Wait(step_result::Wait::task_automatically_done,
                                   std::move(tasks));
    }
}
} // namespace queue_coroutine_test

namespace return_type_test
{
template <typename ReturnTypeT>
class ReturnTask final : public Task
{
    ReturnTypeT return_value;

public:
    using UnambiguousReturnType = ReturnTypeT;
    ReturnTask(ReturnTypeT return_value = {})
        : Task("ReturnTask"), return_value(std::move(return_value))
    {
    }

    StepResult step(SingleThreadedExecutor &executor) override
    {
        return step_result::Done(
            std::make_unique<ReturnTypeT>(std::move(return_value)));
    }
};

template <typename ReturnTypeT>
struct CoroReturnTask;
template <typename ReturnTypeT>
struct CoroReturnTaskPromiseType final
    : public PromiseTypeWithReturnValue<CoroReturnTaskPromiseType<ReturnTypeT>,
                                        CoroReturnTask<ReturnTypeT>>
{
    static std::string get_name() { return "CoroReturnTaskPromiseType"; }
};

template <typename ReturnTypeT>
struct CoroReturnTask final
    : public CoroutineTask<CoroReturnTaskPromiseType<ReturnTypeT>>
{
    using promise_type = CoroReturnTaskPromiseType<ReturnTypeT>;
    using UnambiguousReturnType = ReturnTypeT;
    CoroReturnTask(std::string name, promise_type &promise)
        : CoroutineTask<CoroReturnTaskPromiseType<ReturnTypeT>>(std::move(name),
                                                                promise)
    {
    }
};

template <typename ReturnTypeT>
std::unique_ptr<CoroReturnTask<ReturnTypeT>>
return_task(ReturnTypeT return_value)
{
    co_return std::make_unique<ReturnTypeT>(std::move(return_value));
}

struct MainTask;
struct MainTaskPromiseType final : PromiseType<MainTaskPromiseType, MainTask>
{
    static std::string get_name() { return "MainTask"; }
};
struct MainTask final : public CoroutineTask<MainTaskPromiseType>
{
    MainTask(std::string name, promise_type &promise)
        : CoroutineTask(std::move(name), promise)
    {
    }
};

std::unique_ptr<MainTask> main_task()
{
    {
        std::unique_ptr<std::string> ret_val =
            co_await std::make_unique<ReturnTask<std::string>>("Hello");
        std::cout << *ret_val << '\n';
    }
    {
        std::unique_ptr<std::string> ret_val =
            co_await return_task<std::string>("world");
        std::cout << *ret_val << '\n';
    }
}
} // namespace return_type_test

namespace composite_task_test
{
using rc_queue_test::EnqueueTask;
using rc_queue_test::EnqueueTaskChain;
using rc_queue_test::GuaranteedDequeueTask;

struct MainTask final : public Task
{
    unsigned state = 0;
    Rc<MutexCvObject<std::queue<int>>> queue;
    MainTask()
        : Task("MainTask"), queue(Rc<MutexCvObject<std::queue<int>>>::create())
    {
    }
    StepResult step(SingleThreadedExecutor &executor) override
    {
        switch (state++)
        {
        case 0:
        {
            auto tasks = make_independent_tasks(
                EnqueueTaskChain<true>::create_non_ptr(queue, {1, 2, 3, 4}),
                EnqueueTaskChain<false>::create_non_ptr(queue, {1, 2, 3, 4, 5}),
                EnqueueTaskChain<true>::create_non_ptr(
                    queue, {1, 2, 3, 4, 5, 6, 7, 8, 9, 10}),
                EnqueueTask(queue, 100), GuaranteedDequeueTask(queue),
                GuaranteedDequeueTask(queue));

            return step_result::Wait(
                step_result::Wait::task_not_done,
                make_vector_unique<Task>(std::move(tasks)));
        }
        case 1:
        {
            std::vector<std::unique_ptr<Task>> tasks;
            for (int i = 0; i < 18; ++i)
                tasks.push_back(std::make_unique<GuaranteedDequeueTask>(queue));
            return step_result::Wait(step_result::Wait::task_automatically_done,
                                     std::move(tasks));
        }
        default:
            throw std::runtime_error("Unreachable");
        }
    }
};
} // namespace composite_task_test

void test0()
{
    using namespace queue_test;
    SingleThreadedExecutor executor;
    std::unique_ptr<MainTask> task = std::make_unique<MainTask>();
    executor.add_task(std::move(task));
    executor.run_until_completion();
}

void test1()
{
    using namespace rc_queue_test;
    SingleThreadedExecutor executor;
    std::unique_ptr<MainTask> task = std::make_unique<MainTask>();
    executor.add_task(std::move(task));
    executor.run_until_completion();
}

void test2()
{
    using namespace queue_coroutine_test;
    SingleThreadedExecutor executor;
    auto task = main_task();
    executor.add_task(std::move(task));
    executor.run_until_completion();
}

void test3()
{
    using namespace return_type_test;
    SingleThreadedExecutor executor;
    executor.add_task(main_task());
    executor.run_until_completion();
}

void test4()
{
    using namespace composite_task_test;
    SingleThreadedExecutor executor;
    executor.add_task(std::make_unique<MainTask>());
    executor.run_until_completion();
}

int main(int argc, char const **argv)
{
    std::array tests{test0, test1, test2, test3, test4};
    if (argc < 2)
    {
        fprintf(stderr, "Usage: %s <test_number>\n", program_invocation_name);
        exit(EXIT_FAILURE);
    }

    size_t test_num = std::strtoul(argv[1], nullptr, 10);
    if (test_num >= tests.size())
        fputs("Test number too big\n", stderr);

    tests[test_num]();

    return EXIT_SUCCESS;
}
