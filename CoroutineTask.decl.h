#pragma once
#include "utilities.h"
#include "Executor.decl.h"
#include <optional>
template <typename ChildT, typename CoroutineTaskT>
struct PromiseType;
template <typename PromiseTypeT>
requires requires(PromiseTypeT promise)
{
    { promise.most_recent_executor } -> DecaysTo<SingleThreadedExecutor *>;
    { promise.last_child_return_values } -> DecaysTo<std::optional<std::vector<std::optional<std::unique_ptr<void, TypeErasedDeleter>>>>>;
}
struct CoroutineTask;