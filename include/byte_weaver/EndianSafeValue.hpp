//
// Created by Administrator on 2026/6/4.
//

#ifndef ENDIAN_SAFE_VALUE_HPP
#define ENDIAN_SAFE_VALUE_HPP

#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <iterator>

namespace byte_weaver {

/* 字节序定义 */
enum class ByteOrder {
    LittleEndian,
    BigEndian,
    // C++20 有 std::endian，C++17 我们可以通过宏或常见架构推断 Native 字节序
#if defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__)
    Native = (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__) ? LittleEndian : BigEndian
#elif defined(_WIN32) || defined(__x86_64__) || defined(__i386__)
    Native = LittleEndian
#else
    Native = LittleEndian // 默认回退方案
#endif
};

/* 编译期字节翻转工具 (C++17) */
namespace detail {
    template <typename T>
    constexpr T swap_bytes(T value) noexcept {
        static_assert(std::is_integral_v<T>, "Only integral types can be byte-swapped");
        if constexpr (sizeof(T) == 1) {
            return value;
        } else if constexpr (sizeof(T) == 2) {
            return static_cast<T>((value << 8) | (value >> 8));
        } else if constexpr (sizeof(T) == 4) {
            auto v = static_cast<uint32_t>(value);
            v = ((v << 8) & 0xFF00FF00) | ((v >> 8) & 0x00FF00FF);
            return static_cast<T>((v << 16) | (v >> 16));
        } else if constexpr (sizeof(T) == 8) {
            auto v = static_cast<uint64_t>(value);
            v = ((v << 8) & 0xFF00FF00FF00FF00ULL) | ((v >> 8) & 0x00FF00FF00FF00FFULL);
            v = ((v << 16) & 0xFFFF0000FFFF0000ULL) | ((v >> 16) & 0x0000FFFF0000FFFFULL);
            return static_cast<T>((v << 32) | (v >> 32));
        }
        return value;
    }
}

/* 核心泛型类 */
template <typename T>
class EndianSafeValue {
    static_assert(std::is_arithmetic_v<T>, "EndianSafeValue only supports arithmetic types (integers/floats).");

public:
    static constexpr std::size_t byte_length = sizeof(T);

    EndianSafeValue() = default;
    ~EndianSafeValue() = default;

    // 默认拷贝与移动
    EndianSafeValue(const EndianSafeValue&) = default;
    EndianSafeValue& operator=(const EndianSafeValue&) = default;
    EndianSafeValue(EndianSafeValue&&) noexcept = default;
    EndianSafeValue& operator=(EndianSafeValue&&) noexcept = default;

    /**
     * @brief 从标量值直接构造
     */
    explicit EndianSafeValue(T input, ByteOrder inputOrder = ByteOrder::LittleEndian) {
        set_value(input, inputOrder);
    }

    /**
     * @brief 零拷贝解析 C-String / std::string / 内存块 (C++17 std::string_view)
     */
    explicit EndianSafeValue(std::string_view data, const ByteOrder inputOrder = ByteOrder::LittleEndian) {
        if (data.size() < byte_length) {
            throw std::out_of_range("Insufficient data in string_view.");
        }
        load_from_memory(data.data(), inputOrder);
    }

    /**
     * @brief 从迭代器范围构造
     */
    template <typename InputIt>
    explicit EndianSafeValue(InputIt first, InputIt last, const ByteOrder inputOrder = ByteOrder::LittleEndian) {
        const auto available = std::distance(first, last);
        if (available < static_cast<std::ptrdiff_t>(byte_length)) {
            throw std::out_of_range("Insufficient data in iterator range.");
        }

        uint8_t buffer[byte_length];
        for (std::size_t i = 0; i < byte_length; ++i) {
            buffer[i] = static_cast<uint8_t>(*first++);
        }
        load_from_memory(buffer, inputOrder);
    }

    /**
     * @brief 写入原始值
     */
    void set_value(T input, ByteOrder inputOrder = ByteOrder::LittleEndian) {
        m_value = input;
        if (inputOrder != ByteOrder::Native) {
            swap_internal();
        }
    }

    /**
     * @brief 获取本机的逻辑值
     */
    [[nodiscard]] T get_value() const noexcept {
        return m_value;
    }

    /**
     * @brief 获取某一位的状态 (基于逻辑值，而非内存分布)
     */
    [[nodiscard]] bool get_bit(uint8_t bit_index) const {
        if (bit_index >= byte_length * 8) {
            throw std::out_of_range("Bit index out of range.");
        }

        // 浮点数需要先安全转换为同大小的无符号整数才能进行位运算
        if constexpr (std::is_floating_point_v<T>) {
            using UintType = std::conditional_t<sizeof(T) == 4, uint32_t, uint64_t>;
            UintType int_rep = 0;
            std::memcpy(&int_rep, &m_value, byte_length);
            return (int_rep >> bit_index) & 1;
        } else {
            return (static_cast<uintmax_t>(m_value) >> bit_index) & 1;
        }
    }

    explicit operator bool() const noexcept {
        if constexpr (std::is_floating_point_v<T>) {
            return m_value != static_cast<T>(0.0);
        } else {
            return m_value != 0;
        }
    }

    bool operator==(const EndianSafeValue& other) const noexcept {
        return m_value == other.m_value;
    }

    bool operator!=(const EndianSafeValue& other) const noexcept {
        return m_value != other.m_value;
    }

private:
    T m_value{0};

    // 核心：使用 std::memcpy 替代 Union 消除 UB (严格别名规则)
    void load_from_memory(const void* src, ByteOrder inputOrder) {
        std::memcpy(&m_value, src, byte_length);
        if (inputOrder != ByteOrder::Native) {
            swap_internal();
        }
    }

    void swap_internal() {
        if constexpr (byte_length > 1) {
            if constexpr (std::is_floating_point_v<T>) {
                // 浮点数翻转：先 memcpy 到整型，翻转整型，再 memcpy 回来
                using UintType = std::conditional_t<sizeof(T) == 4, uint32_t, uint64_t>;
                UintType temp = 0;
                std::memcpy(&temp, &m_value, byte_length);
                temp = detail::swap_bytes(temp);
                std::memcpy(&m_value, &temp, byte_length);
            } else {
                m_value = detail::swap_bytes(m_value);
            }
        }
    }
};

/* 提供符合你习惯的类型别名 */
using OneByteStruct  = EndianSafeValue<uint8_t>;
using TwoByteStruct  = EndianSafeValue<uint16_t>;
using FourByteStruct = EndianSafeValue<uint32_t>;
using FloatStruct    = EndianSafeValue<float>;

using DoubleStruct   = EndianSafeValue<double>;
using EightByteStruct = EndianSafeValue<uint64_t>;

} // namespace byte_weaver

#endif // ENDIAN_SAFE_VALUE_HPP