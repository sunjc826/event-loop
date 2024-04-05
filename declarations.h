#pragma once
#include <cassert>
#include <variant>
#include <deque>
#include <queue>
#include <vector>
#include <memory>
#include <functional>
#include <iostream>
#include <exception>
#include <cstring>
#include <coroutine>
#include <type_traits>
#include <utility>
#include <optional>
#include <concepts>
#include "utilities.h"
struct SleepingTask;
struct Task;
struct SingleThreadedExecutor;
enum class SubtaskStatus;
struct Waker;
template <typename ChildT, typename CoroutineTaskT>
struct PromiseType;
template <typename PromiseTypeT>
requires requires(PromiseTypeT promise)
{
    { promise.most_recent_executor } -> DecaysTo<SingleThreadedExecutor *>;
    { promise.last_child_return_values } -> DecaysTo<std::optional<std::vector<std::optional<std::unique_ptr<void, TypeErasedDeleter>>>>>;
}
struct CoroutineTask;
#include "Rc.h"
