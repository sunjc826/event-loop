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
struct SleepingTask;
struct Task;
struct SingleThreadedExecutor;
struct Waker;
template <typename ChildT, typename CoroutineTaskT>
struct PromiseType;
template <typename PromiseTypeT>
struct CoroutineTask;
#include <memory>
#include <vector>
#include <type_traits>
#include <utility>
#include "Rc.h"
#include "utilities.h"
// https://stackoverflow.com/questions/9618268/initializing-container-of-unique-ptrs-from-initializer-list-fails-with-gcc-4-7/9618553#9618553
template <class T> 
auto move_to_unique(T &&t) {
    return std::make_unique<std::decay_t<T>>(std::forward<T>(t));
}
template <class T>
auto move_to_unique(std::unique_ptr<T> &&p) {
    return std::move(p);
}
template <class V, class ... Args> 
auto make_vector_unique(Args &&... args) {
    std::vector<std::unique_ptr<V>> rv;
    (rv.push_back(move_to_unique(std::forward<Args>(args))), ...);
    return rv;
}