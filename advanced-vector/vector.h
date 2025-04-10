#pragma once
#include <cassert>
#include <cstdlib>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>
#include <algorithm> 

template <typename T>
class RawMemory {
public:
    RawMemory() = default;

    explicit RawMemory(size_t capacity)
        : buffer_(Allocate(capacity)), capacity_(capacity) {
    }

    RawMemory(RawMemory&& other) noexcept {
        Swap(other);
    }

    RawMemory& operator=(RawMemory&& rhs) noexcept {
        Swap(rhs);
        return *this;
    }

    RawMemory(const RawMemory&) = delete;
    RawMemory& operator=(const RawMemory&) = delete;

    ~RawMemory() {
        Deallocate(buffer_);
    }

    T* operator+(size_t offset) noexcept {
        assert(offset <= capacity_);
        return buffer_ + offset;
    }

    const T* operator+(size_t offset) const noexcept {
        return const_cast<RawMemory&>(*this) + offset;
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<RawMemory&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < capacity_);
        return buffer_[index];
    }

    void Swap(RawMemory& other) noexcept {
        std::swap(buffer_, other.buffer_);
        std::swap(capacity_, other.capacity_);
    }

    const T* GetAddress() const noexcept {
        return buffer_;
    }

    T* GetAddress() noexcept {
        return buffer_;
    }

    size_t Capacity() const {
        return capacity_;
    }

private:
    static T* Allocate(size_t n) {
        return n ? static_cast<T*>(operator new(n * sizeof(T))) : nullptr;
    }

    static void Deallocate(T* buf) noexcept {
        operator delete(buf);
    }

    T* buffer_ = nullptr;
    size_t capacity_ = 0;
};

template <typename T>
class Vector { 
public: 
    Vector() noexcept = default; 

    explicit Vector(size_t size) 
        : data_(size), size_(size) { 
        std::uninitialized_value_construct_n(data_.GetAddress(), size_); 
    } 

    Vector(const Vector& other) 
        : data_(other.size_), size_(other.size_) { 
        std::uninitialized_copy_n(other.data_.GetAddress(), size_, data_.GetAddress()); 
    } 

    Vector(Vector&& other) noexcept 
        : data_(std::move(other.data_)), size_(std::exchange(other.size_, 0)) { 
    } 

    Vector& operator=(const Vector& rhs) { 
        if (this != &rhs) { 
            if (rhs.size_ > data_.Capacity()) { 
                Vector tmp(rhs); 
                Swap(tmp); 
            } else { 

                // Вместо цикла использован существующий алгоритм std::copy_n
                
                std::copy_n(rhs.data_.GetAddress(), std::min(size_, rhs.size_), data_.GetAddress()); 
                if (size_ < rhs.size_) { 
                    std::uninitialized_copy_n(rhs.data_ + size_, rhs.size_ - size_, data_ + size_); 
                } else if (size_ > rhs.size_) { 
                    std::destroy_n(data_ + rhs.size_, size_ - rhs.size_); 
                } 
                size_ = rhs.size_; 
            } 
        } 
        return *this; 
    } 

    Vector& operator=(Vector&& rhs) noexcept { 
        Swap(rhs); 
        return *this; 
    } 

    ~Vector() { 
        std::destroy_n(data_.GetAddress(), size_); 
    } 

    void Reserve(size_t new_capacity) { 
        if (new_capacity <= data_.Capacity()) return; 

        RawMemory<T> new_data(new_capacity); 
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) { 
            std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress()); 
        } else { 
            std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress()); 
        } 
        std::destroy_n(data_.GetAddress(), size_); 
        data_.Swap(new_data); 
    } 

    void Swap(Vector& other) noexcept { 
        data_.Swap(other.data_); 
        std::swap(size_, other.size_); 
    } 

    size_t Size() const noexcept { 
        return size_; 
    } 

    size_t Capacity() const noexcept { 
        return data_.Capacity(); 
    } 

    const T& operator[](size_t index) const noexcept { 
        return const_cast<Vector&>(*this)[index]; 
    } 

    T& operator[](size_t index) noexcept { 
        assert(index < size_); 
        return data_[index]; 
    } 

    void Resize(size_t new_size) { 
        if (new_size < size_) { 
            std::destroy_n(data_.GetAddress() + new_size, size_ - new_size); 
        } else { 
            if (new_size > data_.Capacity()) { 
                Reserve(new_size); 
            } 
            std::uninitialized_value_construct_n(data_.GetAddress() + size_, new_size - size_); 
        } 
        size_ = new_size; 
    } 

    void PushBack(const T& value) { 
        EmplaceBack(value); 
    } 

    void PushBack(T&& value) { 
        EmplaceBack(std::move(value)); 
    } 

    void PopBack() noexcept { 
        assert(size_ > 0); 
        std::destroy_at(data_.GetAddress() + --size_); 
    } 

    // Несмотря на то, что EmplaceBack — частный случай Emplace, вызов Emplace(end(), ...)
    // приводит к лишнему перемещению аргумента (через std::forward),
    // что нарушает требования тестов к количеству операций перемещения/копирования.
    // Поэтому EmplaceBack реализован отдельно для точного соответствия тестам.
    
    template <typename... Args>
    T& EmplaceBack(Args&&... args) {
        if (size_ == Capacity()) {
            size_t new_capacity = size_ == 0 ? 1 : size_ * 2;
            RawMemory<T> new_data(new_capacity);

            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
            } else {
                std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
            }

            T* place = new_data.GetAddress() + size_;
            try {
                std::construct_at(place, std::forward<Args>(args)...);
            } catch (...) {
                std::destroy_n(new_data.GetAddress(), size_);
                throw;
            }

            std::destroy_n(data_.GetAddress(), size_);
            data_.Swap(new_data);
            ++size_;
            return *place;
        } else {
            T* place = data_.GetAddress() + size_;
            std::construct_at(place, std::forward<Args>(args)...);
            ++size_;
            return *place;
        }
    }


    using iterator = T*; 
    using const_iterator = const T*; 

    iterator begin() noexcept { 
        return data_.GetAddress(); 
    } 

    iterator end() noexcept { 
        return data_.GetAddress() + size_; 
    } 

    const_iterator begin() const noexcept { 
        return data_.GetAddress(); 
    } 

    const_iterator end() const noexcept { 
        return data_.GetAddress() + size_; 
    } 

    const_iterator cbegin() const noexcept { 
        return begin(); 
    } 

    const_iterator cend() const noexcept { 
        return end(); 
    } 

    template <typename... Args> 
    iterator Emplace(const_iterator pos, Args&&... args) { 
        size_t index = pos - begin(); 
        return size_ == Capacity() 
            ? EmplaceWithRealloc(index, std::forward<Args>(args)...) 
            : EmplaceWithoutRealloc(index, std::forward<Args>(args)...); 
    } 

    iterator Insert(const_iterator pos, const T& value) { 
        return Emplace(pos, value); 
    } 

    iterator Insert(const_iterator pos, T&& value) { 
        return Emplace(pos, std::move(value)); 
    } 

    iterator Erase(const_iterator pos) noexcept(std::is_nothrow_move_assignable_v<T>) { 
        assert(pos >= begin() && pos < end()); 
        T* mutable_pos = data_.GetAddress() + (pos - begin()); 
        std::move(mutable_pos + 1, data_.GetAddress() + size_, mutable_pos); 
        std::destroy_at(data_.GetAddress() + --size_); 
        return mutable_pos; 
    } 

private: 

    // Добавлены 2 вспомогательные функции
    // вставки с релокацией памяти и без

    template <typename... Args> 
    iterator EmplaceWithRealloc(size_t index, Args&&... args) { 
        size_t new_capacity = size_ == 0 ? 1 : size_ * 2; 
        RawMemory<T> new_data(new_capacity); 
        T* new_pos = new_data.GetAddress() + index; 

        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) { 
            std::uninitialized_move_n(data_.GetAddress(), index, new_data.GetAddress()); 
            std::construct_at(new_pos, std::forward<Args>(args)...); 
            std::uninitialized_move_n(data_.GetAddress() + index, size_ - index, new_pos + 1); 
        } else { 
            std::uninitialized_copy_n(data_.GetAddress(), index, new_data.GetAddress()); 
            std::construct_at(new_pos, std::forward<Args>(args)...); 
            std::uninitialized_copy_n(data_.GetAddress() + index, size_ - index, new_pos + 1); 
        } 

        std::destroy_n(data_.GetAddress(), size_); 
        data_.Swap(new_data); 
        ++size_; 
        return new_pos; 
    } 

    template <typename... Args> 
    iterator EmplaceWithoutRealloc(size_t index, Args&&... args) { 
        T temp(std::forward<Args>(args)...); 
        T* pos_ptr = data_.GetAddress() + index; 

        if (index < size_) { 
            std::construct_at(data_.GetAddress() + size_, std::move(data_[size_ - 1])); 
            for (size_t i = size_ - 1; i > index; --i) { 
                data_[i] = std::move(data_[i - 1]); 
            } 
            data_[index] = std::move(temp); 
        } else { 
            std::construct_at(pos_ptr, std::move(temp)); 
        } 

        ++size_; 
        return pos_ptr; 
    } 

    RawMemory<T> data_; 
    size_t size_ = 0; 
};

