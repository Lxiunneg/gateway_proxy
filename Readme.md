# Gateway Proxy

一个基于 C++20 开发的网关代理程序，具备端口扫描、服务发现和请求转发功能，支持通过配置文件灵活定制代理行为。

## 项目功能

1. **端口扫描**：定期扫描指定区间的端口，监测活跃服务
2. **服务发现**：提供 API 接口返回当前可用服务列表
3. **请求转发**：将外部请求转发到指定的内部服务端口
4. **日志记录**：支持控制台和文件双输出的日志系统
5. **配置管理**：通过 JSON 配置文件灵活设置代理参数

## 技术栈

- **开发语言**：C++20
- **构建工具**：CMake 3.10+
- **依赖库**：
  - httplib（网络通信）
  - Jsoncpp（JSON 处理）
  - Boost.PFR（结构体序列化）
- **平台支持**：Windows（依赖 Winsock 网络 API）

## 项目结构

```
gateway_proxy/
├── main.cpp               # 程序入口，包含代理核心逻辑
├── CMakeLists.txt         # 主构建配置
├── script/
│   └── generate_config.py # 配置文件生成脚本
├── config/
│   └── config.json        # 配置文件（自动生成）
├── module/
│   ├── my_json/           # JSON 序列化/反序列化模块
│   └── simple_log/        # 日志模块
├── port_scanner/          # 端口扫描器模块
│   ├── test/              # 端口扫描器测试代码
│   └── CMakeLists.txt     # 模块构建配置
└── third-party/           # 第三方依赖库
```

## 快速开始

### 前置条件

- 支持 C++20 的编译器（如 MSVC 2019+、GCC 10+）
- CMake 3.10 及以上版本
- Boost 库（包含 boost.pft 组件）

### 构建步骤

1. **生成配置文件**：

   ```bash
   python script/generate_config.py
   ```

   该脚本会在 `config/` 目录下生成默认配置文件 `config.json`

2. **构建项目**：
   ```bash
   mkdir build
   cd build
   cmake ..
   cmake --build . --config Release
   ```

### 配置说明

配置文件 `config/config.json` 包含以下主要参数：

```json
{
  "base": {
    "base_port": 5600, // 基准端口（服务编号计算基准）
    "host": "127.0.0.1", // 网关监听地址
    "port": 11000 // 网关监听端口
  },
  "scanner": {
    "begin": 5600, // 扫描端口起始值
    "end": 5620, // 扫描端口结束值
    "scan_interval": 5 // 扫描间隔（秒）
  }
}
```

## API 接口

1. **服务列表查询**：

   - 路径：`/machine-list`
   - 方法：GET
   - 响应：返回当前活跃的服务列表（基于基准端口的偏移编号）

   ```json
   {
     "cnt": 2,
     "machine_list": [0, 1]
   }
   ```

2. **请求转发**：
   - 路径：`/*`（支持任意路径）
   - 方法：GET
   - 参数：`machine_no`（服务编号，基于基准端口的偏移量）
   - 功能：将请求转发到对应的内部服务端口（`base_port + machine_no`）

## 日志说明

- 日志同时输出到控制台和文件
- 日志文件命名格式：`[模块名]_simple_logger.log`
- 日志内容包含时间戳、模块名、日志级别和具体信息

## 测试

端口扫描器模块包含独立测试程序，构建后可在 `build/port_scanner/test/` 目录下运行 `Port_Scanner_Test` 进行测试。
