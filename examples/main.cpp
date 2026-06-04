#include "../include/byte_weaver/EndianSafeValue.hpp"
#include <iostream>

using namespace byte_weaver;

int main() {
    // 使用无符号整型，0x80 完美容纳
    uint8_t buffer[] = {0x43, 0x5C, 0x80, 0x00};

    // 使用 C++17 的 std::begin 和 std::end 传入迭代器
    FloatStruct voltage(std::begin(buffer), std::end(buffer), ByteOrder::BigEndian);

    std::cout << "Voltage: " << voltage.get_value() << " V\n";
    return 0;
}
