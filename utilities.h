#pragma once
#include <memory>
#include <vector>
#include <utility>
template <typename U, typename T>
concept DecaysTo = std::same_as<std::decay_t<U>, T>;

struct TypeErasedDeleter
{
    void (*const deleter)(void *);
    TypeErasedDeleter()
        : deleter()
    {}

    TypeErasedDeleter(void (* deleter)(void *))
        : deleter(deleter)
    {}

    template <typename T>
    static TypeErasedDeleter create()
    {
        return TypeErasedDeleter([](void *p) { delete reinterpret_cast<T *>(p); });
    }

    void
    operator()(void *p) const
    {
        if (deleter)
            deleter(p);
    }
};

template <typename T>
std::unique_ptr<void, TypeErasedDeleter> make_type_erased(std::unique_ptr<T> ptr)
{
    std::unique_ptr<void, TypeErasedDeleter> type_erased_ptr(ptr.get(), TypeErasedDeleter::create<T>());
    ptr.release();
    return type_erased_ptr;
}

template <typename T, typename ...Args>
std::unique_ptr<void, TypeErasedDeleter> make_type_erased(Args &&...args)
{
    return make_type_erased<T>(std::make_unique<T>(std::forward<Args>(args)...));    
}

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