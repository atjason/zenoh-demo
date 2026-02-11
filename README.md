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

#### 安装 zenoh 路由器与 C/C++ 库

> 说明：**apt 包名**和 **CMake `find_package()` 包名**不是一回事。
>
> - Ubuntu/Debian 上你需要安装的开发包叫 `libzenohcpp-dev`，但本项目 CMake 仍然是 `find_package(zenohcxx)`（这是 Zenoh C++ API 的 CMake 包名）。

##### macOS / Homebrew

使用官方 [Eclipse Zenoh Homebrew tap](https://github.com/eclipse-zenoh/homebrew-zenoh) 安装。

1. 添加 tap 并安装 **zenoh 路由器**（可选，用于多机或路由模式）：

   ```bash
   brew tap eclipse-zenoh/homebrew-zenoh
   brew install zenoh
   ```

   启动路由器：`zenohd`，`zenohd -l tcp/0.0.0.0:7447`。

2. 安装 **C++ 编译依赖**（编译本示例所必需）：
   - `zenoh-cpp` 依赖 C 后端，需同时安装 **libzenohc**（C 库）和 **libzenohcpp**（C++ 封装）：

   ```bash
   brew tap eclipse-zenoh/homebrew-zenoh
   brew install libzenohc libzenohcpp
   ```

   安装后 CMake 会从 Homebrew 前缀自动找到库，无需额外设置 `CMAKE_PREFIX_PATH`。

##### Ubuntu 22.04 / Debian（x86_64 等）

1. 添加 Eclipse Zenoh apt 源并安装路由器（`zenohd`）：

   ```bash
   # 添加公钥到 keyring
   curl -L https://download.eclipse.org/zenoh/debian-repo/zenoh-public-key \
     | sudo gpg --dearmor --yes --output /etc/apt/keyrings/zenoh-public-key.gpg

   # 添加源并更新
   echo "deb [signed-by=/etc/apt/keyrings/zenoh-public-key.gpg] https://download.eclipse.org/zenoh/debian-repo/ /" \
     | sudo tee -a /etc/apt/sources.list > /dev/null
   sudo apt update

   # 安装路由器（也可以安装 meta 包 `zenoh`，里面会拉取 `zenohd`）
   sudo apt install zenohd
   ```

   启动路由器：`zenohd`

2. 安装 **Zenoh C / C++ 开发库**（提供 CMake 的 `zenohc`、`zenohcxx` 包）：

   ```bash
   sudo apt install libzenohc-dev libzenohcpp-dev
   ```

3. 安装 C++ 编译工具链（如还没装）：

   ```bash
   sudo apt install build-essential cmake pkg-config
   ```

> 若你不是安装到系统默认前缀（例如自行 `cmake --install` 到 `~/.local`），请在配置时指定：
>
> - `cmake -DCMAKE_PREFIX_PATH="$HOME/.local" ../cpp`

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

- 如 C++ 构建阶段找不到 `zenohc` / `zenohcxx`（CMake `find_package` 失败）：
  - Ubuntu/Debian：确认已安装 `libzenohc-dev libzenohcpp-dev`
  - 若你把库安装在非系统前缀：配置时加 `-DCMAKE_PREFIX_PATH=/path/to/prefix`
- 如报错 **Failed to detect zenoh-cpp backend, you need to have either zenoh-c or zenoh-pico installed**：
  - 说明只安装了 C++ 封装，未安装 C 后端：
    - macOS：`brew install libzenohc libzenohcpp`
    - Ubuntu/Debian：`sudo apt install libzenohc-dev libzenohcpp-dev`
- 如收不到消息：
  - 确认路由器或 peer 已运行。
  - 确认发布者和订阅者使用的是同一 key（本示例默认 `demo/zenoh/getting-started`）。

