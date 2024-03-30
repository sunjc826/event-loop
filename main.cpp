#include "CoroutineTask.h"
#include "Executor.h"
#include "Promise.h"
#include "StepResult.h"
#include "Task.h"
#include "declarations.h"
#include <algorithm>
#include <coroutine>
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
                    ConditionVariableNotifyTask(true, queue.cv)
                )
            );
        }
        default:
            throw std::runtime_error("Unreachable");
        }
    }
};
template <bool chain_mode>
class EnqueueTaskChain final : public Task
{
    template <bool> friend class EnqueueTaskChain;
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
                        ConditionVariableNotifyTask(true, queue.cv)
                    )
                );
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
            return step_result::Wait(
                step_result::Wait::task_automatically_done,
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
                make_vector_unique<Task>(RcMutexAcquireTask(Rc<Mutex>(queue, &queue->mutex))));
        }
        case 1:
        {
            if (not queue->object.empty())
            {
                int element = queue->object.front();
                queue->object.pop();
                std::cerr << "Pop: " << element << '\n';
            }
            return step_result::Done(
                make_vector_unique<Task>(RcMutexReleaseTask(Rc<Mutex>(queue, &queue->mutex))));
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
                make_vector_unique<Task>(RcMutexAcquireTask(Rc<Mutex>(queue, &queue->mutex))));
        }
        case State::dequeuing:
        {
            if (queue->object.empty())
                return step_result::Wait(
                    step_result::Wait::task_not_done,
                    make_vector_unique<Task>(
                        RcConditionVariableWaitTask(Rc<Mutex>(queue, &queue->mutex), Rc<ConditionVariable>(queue, &queue->cv))));
            int element = queue->object.front();
            queue->object.pop();
            std::cerr << "Pop: " << element << '\n';
            return step_result::Done(
                make_vector_unique<Task>(RcMutexReleaseTask(Rc<Mutex>(queue, &queue->mutex))));
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
                make_vector_unique<Task>(RcMutexAcquireTask(Rc<Mutex>(queue, &queue->mutex))));
        }
        case 1:
        {
            queue->object.push(element);
            return step_result::Done(
                make_vector_unique<Task>(
                    RcMutexReleaseTask(Rc<Mutex>(queue, &queue->mutex)),
                    RcConditionVariableNotifyTask(true, Rc<ConditionVariable>(queue, &queue->cv))
                )
            );
        }
        default:
            throw std::runtime_error("Unreachable");
        }
    }
};
template <bool chain_mode>
class EnqueueTaskChain final : public Task
{
    template <bool> friend class EnqueueTaskChain;

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
        : Task("EnqueueTaskChain"), queue(std::move(queue)), stack(std::move(stack)),
          stage(stage)
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
                make_vector_unique<Task>(RcMutexAcquireTask(
                    Rc<Mutex>(queue, &queue->mutex))));
        }
        case Stage::enqueue:
        {
            if (stack.empty())
                return step_result::Done(
                    make_vector_unique<Task>(
                        RcMutexReleaseTask(Rc<Mutex>(queue, &queue->mutex)),
                        RcConditionVariableNotifyTask(true, Rc<ConditionVariable>(queue, &queue->cv))
                    )
                );
            int element = stack.back();
            queue->object.push(element);
            stack.pop_back();
            if constexpr (chain_mode)
                return step_result::Done(
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
    Rc<MutexCvObject<std::queue<int>>> queue;

  public:
    MainTask() : Task("MainTask"), queue(Rc<MutexCvObject<std::queue<int>>>::create()) {}
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
            return step_result::Done(
                std::move(tasks));
        }
        default:
            throw std::runtime_error("Unreachable");
        }
    }
};
} // namespace rc_queue_test

namespace queue_coroutine_test
{
struct DequeueTaskPromiseType;
using DequeueTask = CoroutineTask<DequeueTaskPromiseType>;
struct DequeueTaskPromiseType final
    : PromiseType<DequeueTaskPromiseType, DequeueTask>
{
    static std::string get_name() { return "DequeueTaskPromiseType"; }
};
std::unique_ptr<DequeueTask> dequeue_task(Rc<MutexCvObject<std::queue<int>>> queue)
{
    co_yield step_result::Wait(
        step_result::Wait::task_not_done,
        make_vector_unique<Task>(RcMutexAcquireTask(Rc<Mutex>(queue, &queue->mutex))));
    
    if (queue->object.empty())
        co_return;
    int element = queue->object.front();
    queue->object.pop();
    std::cerr << "Pop: " << element << '\n';
    co_yield step_result::Wait(
            step_result::Wait::task_automatically_done,
            make_vector_unique<Task>(RcMutexReleaseTask(Rc<Mutex>(queue, &queue->mutex))));
}
struct EnqueueTaskPromiseType;
using EnqueueTask = CoroutineTask<EnqueueTaskPromiseType>;
struct EnqueueTaskPromiseType final
    : PromiseType<EnqueueTaskPromiseType, EnqueueTask>
{
    static std::string get_name() { return "EnqueueTaskPromiseType"; }
};
std::unique_ptr<EnqueueTask> enqueue_task(Rc<MutexCvObject<std::queue<int>>> queue,
                                          int element)
{
    co_yield step_result::Wait(
        step_result::Wait::task_not_done,
        make_vector_unique<Task>(RcMutexAcquireTask(Rc<Mutex>(queue, &queue->mutex))));
    
    queue->object.push(element);
    co_yield step_result::Wait(
            step_result::Wait::task_automatically_done,
            make_vector_unique<Task>(RcMutexReleaseTask(Rc<Mutex>(queue, &queue->mutex))));
}
struct EnqueueTaskChainPromiseType;
using EnqueueTaskChain = CoroutineTask<EnqueueTaskPromiseType>;
struct EnqueueTaskChainPromiseType final
    : PromiseType<EnqueueTaskChainPromiseType, EnqueueTaskChain>
{
    static std::string get_name() { return "EnqueueTaskChainPromiseType"; }
};

std::unique_ptr<EnqueueTaskChain>
enqueue_task_chain(Rc<MutexCvObject<std::queue<int>>> queue,
                   std::vector<int> elements_to_enqueue)
{
    co_yield step_result::Wait(
        step_result::Wait::task_not_done,
        make_vector_unique<Task>(RcMutexAcquireTask(Rc<Mutex>(queue, &queue->mutex))));
    
    for (int element : elements_to_enqueue)
    {
        queue->object.push(element);
        co_yield step_result::Ready();
    }

    co_yield step_result::Done(
                    make_vector_unique<Task>(RcMutexReleaseTask(
                        Rc<Mutex>(queue, &queue->mutex))));
            
}

struct MainTaskPromiseType;
using MainTask = CoroutineTask<MainTaskPromiseType>;
struct MainTaskPromiseType final : PromiseType<MainTaskPromiseType, MainTask>
{
    static std::string get_name() { return "MainTask"; }
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
        auto dequeue1 = dequeue_task(queue);
        auto dequeue2 = dequeue_task(queue);
        co_yield step_result::Wait(
            step_result::Wait::task_not_done,
            make_vector_unique<Task>(std::move(chain1), std::move(chain2),
                                     std::move(chain3), std::move(chain4),
                                     std::move(dequeue1), std::move(dequeue2)));
    }
    {
        std::vector<std::unique_ptr<Task>> tasks;
        for (int i = 0; i < 18; ++i)
            tasks.emplace_back(dequeue_task(queue));
        co_yield step_result::Wait(
            step_result::Wait::task_automatically_done,
            std::move(tasks));
    }
}
} // namespace queue_coroutine_test

void test1()
{
    using namespace queue_test;
    SingleThreadedExecutor executor;
    std::unique_ptr<MainTask> task = std::make_unique<MainTask>();
    executor.add_task(std::move(task));
    executor.run_until_completion();
}

void test2()
{
    using namespace rc_queue_test;
    SingleThreadedExecutor executor;
    std::unique_ptr<MainTask> task = std::make_unique<MainTask>();
    executor.add_task(std::move(task));
    executor.run_until_completion();
}

void test3()
{
    using namespace queue_coroutine_test;
    SingleThreadedExecutor executor;
    auto task = main_task();
    executor.add_task(std::move(task));
    executor.run_until_completion();
}

int main()
{
    test1();
    return EXIT_SUCCESS;
}
