## Zenoh 多语言入门示例（C++ & Python）

本示例展示如何使用 **zenoh** 协议在 **C++** 与 **Python** 之间进行最基础的发布/订阅通信。

- **统一 key**: `demo/zenoh/getting-started`
- 你可以自由组合：
  - C++ 发布 -> Python 订阅
  - Python 发布 -> C++ 订阅
  - 同语言之间互通

### 1. 运行前准备

- 已安装并可运行的 **zenoh 路由器**（推荐）：
  - 可参考官方文档安装 `zenohd`，示例启动命令：
    - `zenohd`
- **C++ 环境**：
  - CMake（>= 3.x）
  - 已安装 `zenoh-cpp` 及其依赖（见下方「安装 zenoh 与 C++ 库」）
- **Python 环境**：
  - Python 3.8+
  - `pip` 可用

#### 安装 zenoh 与 C++ 库（macOS / Homebrew）

使用官方 [Eclipse Zenoh Homebrew tap](https://github.com/eclipse-zenoh/homebrew-zenoh) 安装，无需从源码编译。

1. 添加 tap 并安装 **zenoh 路由器**（可选，用于多机或路由模式）：

   ```bash
   brew tap eclipse-zenoh/homebrew-zenoh
   brew install zenoh
   ```

   启动路由器：`zenohd`

2. 安装 **C++ 编译依赖**（编译本示例所必需）：
   - `zenoh-cpp` 依赖 C 后端，需同时安装 **libzenohc**（C 库）和 **libzenohcpp**（C++ 封装）：

   ```bash
   brew tap eclipse-zenoh/homebrew-zenoh
   brew install libzenohc libzenohcpp
   ```

   安装后 CMake 会从 Homebrew 前缀自动找到库，无需额外设置 `CMAKE_PREFIX_PATH`。

### 2. Python 示例

1. 安装依赖：

   ```bash
   cd python
   pip install -r requirements.txt
   ```

2. 运行订阅者（监听 C++ 或 Python 发布的消息）：

   ```bash
   python zenoh_py_sub.py
   ```

3. 另开终端运行发布者：

   ```bash
   python zenoh_py_pub.py
   ```

### 3. C++ 示例

1. 在根目录编译 C++ 示例：

   ```bash
   mkdir -p build
   cd build
   cmake ../cpp
   cmake --build .
   ```

   构建成功后将生成两个可执行文件（名称可能依赖你的工具链设置，示例假定为）：

   - `zenoh_cpp_pub`
   - `zenoh_cpp_sub`

2. 运行订阅者：

   ```bash
   ./zenoh_cpp_sub
   ```

3. 另开终端运行发布者：

   ```bash
   ./zenoh_cpp_pub
   ```

### 4. 跨语言互通示例

确保 zenoh 路由器（或 peer）已在运行，然后尝试以下组合（任选其一或全部）：

- **C++ 发布，Python 订阅**
  - 终端 1：在 `build` 目录运行 `./zenoh_cpp_pub`
  - 终端 2：在 `python` 目录运行 `python zenoh_py_sub.py`
- **Python 发布，C++ 订阅**
  - 终端 1：在 `python` 目录运行 `python zenoh_py_pub.py`
  - 终端 2：在 `build` 目录运行 `./zenoh_cpp_sub`

所有示例默认使用相同 key：`demo/zenoh/getting-started`，因此能够互相接收消息。

### 5. 常见问题简要提示

- 如 C++ 构建阶段找不到 `zenoh-cpp`：
  - 请确认已按照上方「安装 zenoh 与 C++ 库」安装；非 macOS 可参考官方文档并配置 CMake 查找路径（如 `CMAKE_PREFIX_PATH`）。
- 如报错 **Failed to detect zenoh-cpp backend, you need to have either zenoh-c or zenoh-pico installed**：
  - 说明只安装了 C++ 封装，未安装 C 后端。macOS 上请执行：`brew install libzenohc`（与 `libzenohcpp` 一起使用）。
- 如收不到消息：
  - 确认路由器或 peer 已运行。
  - 确认发布者和订阅者使用的是同一 key（本示例默认 `demo/zenoh/getting-started`）。

