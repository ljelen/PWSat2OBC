#ifndef SRC_BASE_UTILS_H
#define SRC_BASE_UTILS_H

#pragma once

#include <stdbool.h>
#include <cstdint>
#include <type_traits>
#include <utility>

/**
 * @brief Converts bool value to 1 or 0
 * @param[in] value Value to convert
 * @return 1 for true, 0 for false
 */
static inline int ToInt(bool value)
{
    return value ? 1 : 0;
}

/**
 * @brief Just marker indicating that value is in bytes
 * @param[in] value Value
 * @return The same value
 */
constexpr std::size_t operator"" _Bytes(unsigned long long int value)
{
    return value;
}

/**
 * @brief Converts value in kilobytes to bytes
 * @param[in] value Values in kilobytes
 * @return Value in bytes
 */
constexpr std::size_t operator"" _KB(unsigned long long int value)
{
    return value * 1024_Bytes;
}

/**
 * @brief Converts value in megabytes to bytes
 * @param[in] value Values in megabytes
 * @return Value in bytes
 */
constexpr std::size_t operator"" _MB(unsigned long long int value)
{
    return value * 1024_KB;
}

/**
 * @brief Inheriting from this class, will make derived class unconstructible
 */
struct PureStatic
{
    PureStatic() = delete;
};

/**
 * @brief Class that can logically contain either a value of type T or no value.
 */
template <class T> class Option
{
  public:
    /**
     * @brief Factory method that constructs empty Option instance.
     * @return Empty Option instance.
     */
    static Option<T> None()
    {
        return Option<T>(false, T());
    }

    /**
     * @brief Factory method that constructs Option instance that holds given value.
     * @param[in] value Value to hold in Option instance.
     * @return Option instance that holds a value.
     */
    static Option<T> Some(T&& value)
    {
        return Option<T>(true, std::move(value));
    }

    /**
     * @brief Factory method that constructs Option instance that holds given value.
     * @param[in] value Value to hold in Option instance.
     * @return Option instance that holds a value.
     */
    static Option<T> Some(T& value)
    {
        return Option<T>(true, value);
    }

    /**
      * @brief A flag indicating if this Option instance holds a value.
      */
    const bool HasValue;

    /**
      * @brief Value held by Option instance if HasValue is true.
      */
    const T Value;

  private:
    Option(bool hasValue, T&& value) : HasValue(hasValue), Value(std::move(value))
    {
    }

    Option(bool hasValue, T& value) : HasValue(hasValue), Value(value)
    {
    }
};

/**
 * @brief Factory method that constructs empty Option instance.
 * @return Empty Option instance.
 */
template <typename T> static inline Option<T> None()
{
    return Option<T>::None();
}

/**
 * @brief Factory method that constructs Option instance that holds given value.
 * @param[in] value Value to hold in Option instance.
 * @return Option instance that holds a value.
 */
template <typename T> static inline Option<std::remove_reference_t<T>> Some(T&& value)
{
    using U = std::remove_reference_t<T>;

    return Option<U>::Some(std::forward<U>(value));
}

/**
 * @brief Private-inherit this class to prevent copy-operations
 */
struct NotCopyable
{
    NotCopyable() = default;
    NotCopyable(const NotCopyable& arg) = delete;
    NotCopyable& operator=(const NotCopyable& arg) = delete;
};

/**
 * @brief Private-inherit this class to prevent move-operations
 */
struct NotMoveable
{
    NotMoveable() = default;
    NotMoveable(NotMoveable&& arg) = delete;
    NotMoveable& operator=(NotMoveable&& arg) = delete;
};

#endif
