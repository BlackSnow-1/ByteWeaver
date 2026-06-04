# ByteWeaver 🧶

**ByteWeaver** 是一个专为工控通信、网络协议报文及底层硬件交互设计的现代 C++17 基础库。它彻底摒弃了传统 C 语言中使用 `union` 解析字节流所带来的未定义行为（UB），通过 `std::memcpy` 与泛型模板，在保证极致性能与零拷贝（Zero-copy）的前提下，提供了一套优雅、跨平台且端序安全的数据打包与提取方案。

## ✨ 核心特性

* **开箱即用 (Header-only)：** 纯头文件设计，只需 `#include` 即可无缝接入项目，无需复杂的编译与静态库链接配置。
* **端序安全 (Endian-safe)：** 内置编译期大小端推断与高性能位运算翻转逻辑，跨平台、跨架构数据交换毫无压力。
* **告别 UB (UB-free)：** 严格遵循现代 C++ 规范，使用定长内存拷贝替代 `union` 别名转换（Type Punning），安全且高效。
* **零拷贝解析 (Zero-copy)：** 深度结合 C++17 `std::string_view` 与迭代器，绝不多余分配一次内存，专为高频采样与底层报文打造。
* **极简泛型 (Generic & Extensible)：** 一个核心模板类 `EndianSafeValue<T>` 统管所有算术类型（8/16/32/64 位整数及浮点数），并提供友好的类型别名。

---

## 🚀 快速上手 (Quick Start)

### 场景演示：解析底层采样值与保护逻辑标志位

在处理诸如**变压器差动保护**或**二次谐波越限**的高频采样报文时，报文通常以大端序（Big-Endian）通过网络传输。借助 ByteWeaver，你可以极速且安全地剥离出数据与逻辑标志。

```cpp
#include <iostream>
#include <vector>
#include "byte_weaver/EndianSafeValue.hpp"

using namespace byte_weaver;

int main() {
    // 模拟一段来自合并单元或底层采样板卡的大端报文数据 (16进制)
    // 假设前4字节为二次谐波电流采样值 (Float), 紧跟1字节为越限标志位
    uint8_t packet[] = {
        0x41, 0x1A, 0x66, 0x66, // 浮点数 9.65 (大端序)
        0x81                    // 标志位 1000 0001 (位7和位0为1)
    };

    // 1. 零拷贝提取浮点数值（自动处理大端到本机端序的转换）
    FloatStruct harmonic_current(std::begin(packet), std::end(packet) - 1, ByteOrder::BigEndian);
    
    // 2. 提取单字节保护逻辑状态
    OneByteStruct protection_flags(packet[4]);

    std::cout << "--- 采样分析 ---" << std::endl;
    std::cout << "二次谐波电流值: " << harmonic_current.get_value() << " A" << std::endl;
    
    // 假设第 7 位(最高位)为“电流越限”启动元件标志
    bool is_over_limit = protection_flags.get_bit(7);
    // 假设第 0 位(最低位)为“通道有效”标志
    bool is_channel_valid = protection_flags.get_bit(0);

    std::cout << "通道是否有效: " << (is_channel_valid ? "是" : "否") << std::endl;
    
    if (is_channel_valid && is_over_limit) {
        std::cout << "[告警] 检测到二次谐波电流越限！" << std::endl;
    }

    return 0;
}

```

---

## 🛠️ 集成指南 (Integration)

由于 ByteWeaver 是一个纯头文件库，你可以通过以下两种方式轻松集成：

### 方法一：直接拷贝（推荐用于小型脚本/项目）

直接将 `include/byte_weaver` 文件夹复制到你的项目源码树中，并在代码中包含：

```cpp
#include "byte_weaver/EndianSafeValue.hpp"
```

### 方法二：CMake 优雅集成 (现代 C++ 最佳实践)

如果你使用 CMake 管理工程，推荐将 ByteWeaver 作为 `INTERFACE` 库引入，以保持目录整洁并支持依赖传递：

在你的顶级 `CMakeLists.txt` 中添加：

```cmake
# 声明 ByteWeaver 为接口库
add_library(ByteWeaver INTERFACE)
target_include_directories(ByteWeaver INTERFACE 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/path/to/ByteWeaver/include>
)

# 在你的业务目标中链接它
add_executable(ProtectionLogicApp main.cpp)
target_link_libraries(ProtectionLogicApp PRIVATE ByteWeaver)

```

---

## 🧩 支持的类型别名 (Type Aliases)

为了符合常见的工程直觉，库内部提供了以下开箱即用的类型别名，你也可以根据需要随时通过 `EndianSafeValue<T>` 拓展：

| 别名名称 | 内部映射 | 字节长度 | 适用场景 |
| --- | --- | --- | --- |
| `OneByteStruct` | `EndianSafeValue<uint8_t>` | 1 Byte | 状态机、控制字、逻辑标志位 |
| `TwoByteStruct` | `EndianSafeValue<uint16_t>` | 2 Bytes | 短整型采样、寻址偏移 |
| `FourByteStruct` | `EndianSafeValue<uint32_t>` | 4 Bytes | 标准整型数据、时间戳 |
| `EightByteStruct` | `EndianSafeValue<uint64_t>` | 8 Bytes | 高精度时间戳、MAC地址合并 |
| `FloatStruct` | `EndianSafeValue<float>` | 4 Bytes | IEEE 754 单精度浮点采样值 |
| `DoubleStruct` | `EndianSafeValue<double>` | 8 Bytes | IEEE 754 双精度运算结果 |

---

## 📄 许可证 (License)

本项目采用 **MIT License** 开源。完全允许商业使用、修改及分发。详见 [LICENSE](https://www.google.com/search?q=LICENSE) 文件。