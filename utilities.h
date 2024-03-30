#include <memory>

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
