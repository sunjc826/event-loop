#pragma once
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <utility>
// reference counted non-thread-safe shared pointer
struct Deleter
{
    void *real_ptr;
    void (*deleter)(void *p);
    Deleter(void *real_ptr, void (*deleter)(void *p))
        : real_ptr(real_ptr), deleter(deleter)
    {
    }
    void operator()() { deleter(real_ptr); }
};
template <typename T>
class Rc
{
    template <typename>
    friend class Rc;
    T *ptr;
    unsigned *count;

    std::optional<Deleter> deleter;
    Rc(T *ptr, unsigned *count) : ptr(ptr), count(count)
    {
#ifndef NDEBUG
        std::cerr << "Rc(T *ptr, unsigned *count)\n";
#endif
    }

public:
    explicit Rc(T *ptr) : ptr(ptr), count{new unsigned(1)}
    {
#ifndef NDEBUG
        std::cerr << "Rc(T *ptr)\n";
#endif
    }

    template <typename... Args>
    static Rc<T> create(Args &&...args)
    {
#ifndef NDEBUG
        std::cerr << "Rc<T> create(Args &&...args)\n";
#endif
        // Incorrect for now because we always do 2 deletes in destructor
        // whereas there is only 1 allocation here.
        // void *p = malloc(offsetof(Rc, deleter));
        // T *ptr = (T *)p;
        // unsigned *count = (unsigned *)((char *)p + offsetof(Rc<T>, count));
        // std::construct_at(ptr, std::forward<Args>(args)...);
        // std::construct_at(count, 1);
        // return Rc<T>(ptr, count);

        return Rc(new T(std::forward<Args>(args)...));
    }

    Rc() = default;

    Rc(std::nullptr_t) : ptr(), count()
    {
#ifndef NDEBUG
        std::cerr << "Rc(std::nullptr_t = nullptr)\n";
#endif
    }

    Rc(Rc const &other)
        : ptr(other.ptr), count(other.count), deleter(other.deleter)
    {
#ifndef NDEBUG
        std::cerr << "Rc(Rc const &other)\n";
#endif
        if (count)
            ++*count;
    }

    template <typename U>
    Rc(Rc<U> const &other)
        : ptr(other.ptr), count(other.count), deleter(other.deleter)
    {
#ifndef NDEBUG
        std::cerr << "Rc(Rc<U> const &other)\n";
#endif
        if (count)
            ++*count;
    }

    Rc(Rc &&other) : ptr(other.ptr), count{other.count}, deleter(other.deleter)
    {
#ifndef NDEBUG
        std::cerr << "Rc(Rc<U> &&other)\n";
#endif
        other.ptr = nullptr;
        other.count = nullptr;
        other.deleter = std::nullopt;
    }

    template <typename U>
    Rc(Rc<U> &&other)
        : ptr(other.ptr), count{other.count}, deleter(other.deleter)
    {
#ifndef NDEBUG
        std::cerr << "Rc(Rc<U> &&other)\n";
#endif
        other.ptr = nullptr;
        other.count = nullptr;
        other.deleter = std::nullopt;
    }

    // aliasing constructor
    template <typename U>
    Rc(Rc<U> const &other, T *alias)
        : ptr(alias), count(other.count),
          deleter(Deleter(other.ptr,
                          [](void *p) { delete reinterpret_cast<U *>(p); }))
    {
#ifndef NDEBUG
        std::cerr << "Rc(Rc<U> const &other, T *alias)\n";
#endif
        // assume non null
        ++*count;
    }

    Rc<T> &operator=(Rc<T> const &other)
    {
#ifndef NDEBUG
        std::cerr << "Rc<T> &operator=(Rc<U> const &other)\n";
#endif
        if (&other == this)
            return *this;
        ptr = other.ptr;
        count = other.count;
        deleter = other.deleter;
        if (count)
            ++*count;
        return *this;
    }

    template <typename U>
    Rc<T> &operator=(Rc<U> const &other)
    {
#ifndef NDEBUG
        std::cerr << "Rc<T> &operator=(Rc<U> const &other)\n";
#endif
        if (&other == this)
            return *this;
        ptr = other.ptr;
        count = other.count;
        deleter = other.deleter;
        if (count)
            ++*count;
        return *this;
    }

    Rc<T> &operator=(Rc<T> &&other)
    {
#ifndef NDEBUG
        std::cerr << "Rc<T> &operator=(Rc<U> &&other)\n";
#endif
        if (&other == this)
            return *this;
        ptr = other.ptr;
        count = other.count;
        deleter = other.deleter;
        other.ptr = nullptr;
        other.count = nullptr;
        other.deleter = std::nullopt;
        return *this;
    }

    template <typename U>
    Rc<T> &operator=(Rc<U> &&other)
    {
#ifndef NDEBUG
        std::cerr << "Rc<T> &operator=(Rc<U> &&other)\n";
#endif
        if (&other == this)
            return *this;
        ptr = other.ptr;
        count = other.count;
        deleter = other.deleter;
        other.ptr = nullptr;
        other.count = nullptr;
        other.deleter = std::nullopt;
        return *this;
    }

    T &operator*() { return *ptr; }

    T *operator->() { return ptr; }

    T *get() { return ptr; }

    ~Rc()
    {
#ifndef NDEBUG
        std::cerr << "~Rc()\n";
#endif
        if (count != nullptr and --*count == 0)
        {
            if (deleter)
                (*deleter)();
            else
                delete ptr;
            delete count;
        }
    }
};