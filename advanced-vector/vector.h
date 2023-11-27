#pragma once
#include <cassert>
#include <cstdlib>
#include <new>
#include <utility>
#include <memory>
#include <algorithm>

template <typename T>
class RawMemory
{
public:
    RawMemory() = default;

    explicit RawMemory(size_t capacity)
        : buffer_(Allocate(capacity)), capacity_(capacity)
    {
    }

    RawMemory(const RawMemory &other) = delete;

    RawMemory(RawMemory &&other) noexcept
    {
        buffer_ = other.buffer_;
        capacity_ = other.capacity_;
        other.capacity_ = 0;
        other.buffer_ = nullptr;
    }

    RawMemory &operator=(const RawMemory &rhs) = delete;

    RawMemory &operator=(RawMemory &&rhs) noexcept
    {
        RawMemory tmp(std::move(rhs));
        this->Swap(tmp);
        return *this;
    }

    ~RawMemory()
    {
        Deallocate(buffer_);
    }

    T *operator+(size_t offset) noexcept
    {
        assert(offset <= capacity_);
        return buffer_ + offset;
    }

    const T *operator+(size_t offset) const noexcept
    {
        return const_cast<RawMemory &>(*this) + offset;
    }

    const T &operator[](size_t index) const noexcept
    {
        return const_cast<RawMemory &>(*this)[index];
    }

    T &operator[](size_t index) noexcept
    {
        assert(index < capacity_);
        return buffer_[index];
    }

    void Swap(RawMemory &other) noexcept
    {
        std::swap(buffer_, other.buffer_);
        std::swap(capacity_, other.capacity_);
    }

    const T *GetAddress() const noexcept
    {
        return buffer_;
    }

    T *GetAddress() noexcept
    {
        return buffer_;
    }

    size_t Capacity() const
    {
        return capacity_;
    }

private:
    static T *Allocate(size_t n)
    {
        return n != 0 ? static_cast<T *>(operator new(n * sizeof(T))) : nullptr;
    }

    static void Deallocate(T *buf) noexcept
    {
        operator delete(buf);
    }

    T *buffer_ = nullptr;
    size_t capacity_ = 0;
};

template <typename T>
class Vector
{
public:
    Vector() = default;

    explicit Vector(size_t size)
        : data_(RawMemory<T>(size)), size_(size)
    {
        std::uninitialized_value_construct_n(data_.GetAddress(), size);
    }

    Vector(const Vector &other)
        : data_(other.size_), size_(other.size_)
    {
        size_t i = 0;
        try
        {
            for (; i != other.size_; ++i)
            {

                CopyConstruct(data_ + i, other.data_[i]);
            }
        }
        catch (...)
        {
            std::destroy_n(data_.GetAddress(), i);
            throw;
        }
    }

    Vector(Vector &&other)
    {
        this->Swap(other);
    }

    ~Vector()
    {
        std::destroy_n(data_.GetAddress(), size_);
    }

    using iterator = T *;
    using const_iterator = const T *;

    iterator begin() noexcept
    {
        return data_.GetAddress();
    }
    iterator end() noexcept
    {
        return begin() + size_;
    }
    const_iterator begin() const noexcept
    {
        return cbegin();
    }
    const_iterator end() const noexcept
    {
        return cend();
    }
    const_iterator cbegin() const noexcept
    {
        return data_.GetAddress();
    }
    const_iterator cend() const noexcept
    {
        return begin() + size_;
    }

    Vector &operator=(const Vector &rhs)
    {
        if (this != &rhs)
        {
            if (rhs.size_ > data_.Capacity())
            {
                Vector tmp(rhs);
                this->Swap(tmp);
            }
            else
            {
                if (size_ > rhs.size_)
                {
                    size_t i = 0;
                    for (; i < rhs.size_; i++)
                    {
                        data_[i] = rhs[i];
                    }
                    if (size_ > rhs.size_)
                    {
                        std::destroy_n(data_.GetAddress() + rhs.size_, size_ - rhs.size_);
                    }
                }
                else
                {
                    size_t i = 0;
                    for (; i < size_; i++)
                    {
                        data_[i] = rhs[i];
                    }
                    std::uninitialized_copy_n(rhs.data_.GetAddress() + i, rhs.size_ - size_, data_.GetAddress() + i);
                }
                size_ = rhs.size_;
            }
        }
        return *this;
    }

    Vector &operator=(Vector &&rhs)
    {
        data_ = std::move(rhs.data_);
        size_ = rhs.size_;
        rhs.size_ = 0;
        return *this;
    }

    void Swap(Vector &other) noexcept
    {
        std::swap(data_, other.data_);
        std::swap(size_, other.size_);
    }

    size_t Size() const noexcept
    {
        return size_;
    }

    size_t Capacity() const noexcept
    {
        return data_.Capacity();
    }

    const T &operator[](size_t index) const noexcept
    {
        return data_[index];
    }

    T &operator[](size_t index) noexcept
    {
        assert(index < size_);
        return data_[index];
    }

    void Reserve(size_t new_capacity)
    {
        if (new_capacity <= data_.Capacity())
        {
            return;
        }
        RawMemory<T> new_data(new_capacity);
        ElementsMigration(data_, new_data, size_);
    }

    void Resize(size_t new_size)
    {
        if (new_size > size_)
        {
            if (new_size > data_.Capacity())
            {
                Reserve(new_size);
            }
            std::uninitialized_value_construct_n(data_.GetAddress() + size_, new_size - size_);
            size_ = new_size;
        }
        else if (new_size < size_)
        {
            std::destroy_n(data_.GetAddress() + new_size, size_ - new_size);
            size_ = new_size;
        }
    }

    void PushBack(T &value)
    {
        EmplaceBack(value);
    }

    void PushBack(T &&value)
    {
        EmplaceBack(std::move(value));
    }

    template <typename... Args>
    T &EmplaceBack(Args &&...args)
    {
        if (size_ == Capacity())
        {
            RawMemory<T> new_data (size_ == 0 ? 1 : size_ * 2);
            new (new_data + size_) T(std::forward<Args>(args)...);
            ElementsMigration(data_, new_data, size_);
            ++size_;
        }
        else
        {
            new (end()) T(std::forward<Args>(args)...);
            ++size_;
        }
        return data_[size_ - 1];
    }

    iterator Insert(const_iterator pos, const T &value)
    {
        return Emplace(pos, value);
    }

    iterator Insert(const_iterator pos, T &&value)
    {
        return Emplace(pos, std::move(value));
    }

    template <typename... Args>
    iterator Emplace(const_iterator pos, Args &&...args)
    {
        assert(begin()<=pos && pos<=end());
        if (pos == end())
        {
            EmplaceBack(std::forward<Args>(args)...);
            return begin() + (size_ - 1);
        }
        size_t position = std::distance(cbegin(), pos);
        if (size_ == Capacity())
        {
            RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);
            new (new_data.GetAddress() + position) T(std::forward<Args>(args)...);
            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>)
            {
                std::uninitialized_move_n(data_.GetAddress(), position, new_data.GetAddress());
                std::uninitialized_move_n(data_.GetAddress() + position, size_ - position, new_data.GetAddress() + position + 1);
            }
            else
            {
                    std::uninitialized_copy_n(data_.GetAddress(), position, new_data.GetAddress());
                    std::uninitialized_copy_n(data_.GetAddress() + position, size_ - position, new_data.GetAddress() + position + 1);
            }
            std::destroy_n(data_.GetAddress(), size_);
            data_.Swap(new_data);
            ++size_;
        }
        else
        {
            T temporary(std::forward<Args>(args)...);
            new (end()) T(std::move(data_[size_-1]));
            std::move_backward(begin() + position, end()-1, end());
            data_[position] = std::move(temporary);
            ++size_;
        }
        return data_.GetAddress() + position;
    }

    iterator Erase(const_iterator pos) noexcept
    {
        assert(begin() <= pos && pos < end());
        size_t position = std::distance(cbegin(), pos);
        std::move(begin() + position + 1, end(), begin() + position);
        std::destroy_n(end() - 1, 1);
        --size_;
        return begin() + position;
    }
    void PopBack() noexcept
    {
        assert(size_>0);
        std::destroy_n(end() - 1, 1);
        --size_;
    }

private:
    // Создаёт копию объекта elem в сырой памяти по адресу buf
    static void CopyConstruct(T *buf, const T &elem)
    {
        new (buf) T(elem);
    }

    // Перемещает элемены в новую область памяти
    void ElementsMigration(RawMemory<T> &from, RawMemory<T> &to, size_t number_of_elements)
    {
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>)
        {
            std::uninitialized_move_n(from.GetAddress(), number_of_elements, to.GetAddress());
        }
        else
        {
            std::uninitialized_copy_n(from.GetAddress(), number_of_elements, to.GetAddress());
        }
        std::destroy_n(from.GetAddress(), number_of_elements);
        from.Swap(to);
    }

    RawMemory<T> data_;
    size_t size_ = 0;
};