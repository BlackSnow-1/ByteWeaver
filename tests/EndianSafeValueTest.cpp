//
// Created by Administrator on 2026/6/6.
//
#include <gtest/gtest.h>

#include <vector>
#include <string_view>
#include <cmath>

#include "byte_weaver/EndianSafeValue.hpp"

using namespace byte_weaver;

// 获取当前平台的原生字节序，以便在测试中进行动态断言
constexpr ByteOrder SystemNativeOrder = ByteOrder::Native;
constexpr ByteOrder NonNativeOrder = (SystemNativeOrder == ByteOrder::LittleEndian)
                                     ? ByteOrder::BigEndian
                                     : ByteOrder::LittleEndian;

class EndianSafeValueTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// =========================================================================
// 1. 基本构造与 Native 字节序测试
// =========================================================================
TEST_F(EndianSafeValueTest, BasicNativeOrder) {
    OneByteStruct v1(0x12, SystemNativeOrder);
    EXPECT_EQ(v1.get_value(), 0x12);

    TwoByteStruct v2(0x1234, SystemNativeOrder);
    EXPECT_EQ(v2.get_value(), 0x1234);

    FourByteStruct v4(0x12345678, SystemNativeOrder);
    EXPECT_EQ(v4.get_value(), 0x12345678);

    EightByteStruct v8(0x1122334455667788ULL, SystemNativeOrder);
    EXPECT_EQ(v8.get_value(), 0x1122334455667788ULL);
}

// =========================================================================
// 2. 字节序翻转逻辑测试 (整数)
// =========================================================================
TEST_F(EndianSafeValueTest, IntegerByteSwapping) {
    // 1 字节：不应发生翻转
    OneByteStruct v1(0x12, NonNativeOrder);
    EXPECT_EQ(v1.get_value(), 0x12);

    // 2 字节翻转
    TwoByteStruct v2(0x1234, NonNativeOrder);
    EXPECT_EQ(v2.get_value(), 0x3412);

    // 4 字节翻转
    FourByteStruct v4(0x12345678, NonNativeOrder);
    EXPECT_EQ(v4.get_value(), 0x78563412);

    // 8 字节翻转
    EightByteStruct v8(0x1122334455667788ULL, NonNativeOrder);
    EXPECT_EQ(v8.get_value(), 0x8877665544332211ULL);
}

// =========================================================================
// 3. 浮点数翻转测试
// =========================================================================
TEST_F(EndianSafeValueTest, FloatingPointSwapping) {
    // Float 测试
    float orig_float = 3.14159f;
    FloatStruct vf(orig_float, NonNativeOrder);

    // 手动翻转并比较，确保安全内存转换符合预期
    uint32_t float_as_int;
    std::memcpy(&float_as_int, &orig_float, 4);
    uint32_t swapped_int = detail::swap_bytes(float_as_int);
    float swapped_float;
    std::memcpy(&swapped_float, &swapped_int, 4);

    EXPECT_FLOAT_EQ(vf.get_value(), swapped_float);

    // 再次翻转应变回原值
    vf.set_value(vf.get_value(), NonNativeOrder);
    EXPECT_FLOAT_EQ(vf.get_value(), orig_float);
}

// =========================================================================
// 4. std::string_view 与 内存解析测试
// =========================================================================
TEST_F(EndianSafeValueTest, StringViewConstructor) {
    // 构造一个包含 0x12, 0x34 的字符串 (内存顺序)
    char data[] = {0x12, 0x34, 0x56, 0x78};
    std::string_view sv(data, sizeof(data));

    // 使用系统原生顺序解析前两个字节
    TwoByteStruct v2(sv, SystemNativeOrder);
    uint16_t expected_v2;
    std::memcpy(&expected_v2, data, 2);
    EXPECT_EQ(v2.get_value(), expected_v2);

    // 异常测试：数据长度不足
    std::string_view short_sv(data, 1);
    EXPECT_THROW({
        TwoByteStruct v_short(short_sv, SystemNativeOrder);
    }, std::out_of_range);
}

// =========================================================================
// 5. 迭代器范围测试
// =========================================================================
TEST_F(EndianSafeValueTest, IteratorConstructor) {
    std::vector<uint8_t> buffer = {0xAA, 0xBB, 0xCC, 0xDD};

    // 取前两个字节
    TwoByteStruct v2(buffer.begin(), buffer.begin() + 2, SystemNativeOrder);
    uint16_t expected_v2;
    std::memcpy(&expected_v2, buffer.data(), 2);
    EXPECT_EQ(v2.get_value(), expected_v2);

    // 异常测试：迭代器范围不足
    EXPECT_THROW({
        FourByteStruct v4(buffer.begin(), buffer.begin() + 2, SystemNativeOrder);
    }, std::out_of_range);
}

// =========================================================================
// 6. get_bit 位操作测试
// =========================================================================
TEST_F(EndianSafeValueTest, GetBitMethod) {
    // 测试数据: 0b 0000 0000 0000 0101 (即 5)
    TwoByteStruct v(5, SystemNativeOrder);

    EXPECT_TRUE(v.get_bit(0));  // 第 0 位是 1
    EXPECT_FALSE(v.get_bit(1)); // 第 1 位是 0
    EXPECT_TRUE(v.get_bit(2));  // 第 2 位是 1
    EXPECT_FALSE(v.get_bit(3)); // 第 3 位是 0

    // 异常测试：越界
    EXPECT_THROW(v.get_bit(16), std::out_of_range);

    // 测试浮点数符号位 (IEEE 754 标准，单精度浮点数的符号位是第31位)
    FloatStruct v_pos(1.0f, SystemNativeOrder);
    EXPECT_FALSE(v_pos.get_bit(31)); // 正数，符号位为0

    FloatStruct v_neg(-1.0f, SystemNativeOrder);
    EXPECT_TRUE(v_neg.get_bit(31));  // 负数，符号位为1
}

// =========================================================================
// 7. 操作符重载测试 (bool, ==, !=)
// =========================================================================
TEST_F(EndianSafeValueTest, Operators) {
    FourByteStruct v_zero(0, SystemNativeOrder);
    FourByteStruct v_nonzero(42, SystemNativeOrder);

    // operator bool
    EXPECT_FALSE(static_cast<bool>(v_zero));
    EXPECT_TRUE(static_cast<bool>(v_nonzero));

    // Float operator bool
    FloatStruct f_zero(0.0f, SystemNativeOrder);
    FloatStruct f_nonzero(0.001f, SystemNativeOrder);
    EXPECT_FALSE(static_cast<bool>(f_zero));
    EXPECT_TRUE(static_cast<bool>(f_nonzero));

    // operator== 和 operator!=
    FourByteStruct v_same(42, SystemNativeOrder);
    EXPECT_TRUE(v_nonzero == v_same);
    EXPECT_FALSE(v_nonzero != v_same);
    EXPECT_TRUE(v_zero != v_nonzero);
}