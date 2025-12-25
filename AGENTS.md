# Repository Guidelines

## 项目结构与模块组织
- `src/`：主工程源码，入口为 `src/main.c`，包含 MQTT/UDP/UART/Modbus 等模块。
- `src/lib/`：第三方或内嵌库（如 `cJSON`、`mongoose`、paho）。
- `src/mcu_sdk/`：设备侧 SDK 与协议适配层。
- `tools/`：辅助脚本（如 `tools/use_uclibc_toolchain.sh`、`tools/env_report.sh`）。
- `build/`：编译产物目录（输出 `build/haas_dtu`、`build/version`）。
- 顶层 `mbtcp_*.c`：历史/工具代码，合入主程序前需确认编译入口与调用关系。

## 构建、测试与开发命令
- `make`：交叉编译生成 `build/` 产物。
- `make clean`：清理 `build/` 与对象文件目录。
- `source tools/use_uclibc_toolchain.sh`：启用内置 uClibc 工具链，确保 `mipsel-openwrt-linux-gcc` 可用。
- `bash tools/env_report.sh -o env.txt`：生成构建环境报告，便于排查环境差异。

## 编码风格与命名约定
- 语言为 C（`-std=gnu99`），使用 4 空格缩进与 K&R 大括号风格。
- 模块文件以功能命名（如 `uart.c`/`mqtt.c`），头文件与实现同名。
- 宏常量全大写（如 `BF_VERSION`）。新增代码避免未使用变量/函数与格式化警告。

## 测试指南
- 当前以脚本式自测为主：`bash src/bfbr.test.sh`（会生成并运行 `test.bfbr`）。
- 新增可独立验证的模块，优先提供类似 `*.test.sh` 的可重复执行用例，保持可离线运行。

## 提交与拉取请求指南
- 当前未检测到 Git 历史，建议提交信息格式：`<module>: <summary>`，如 `mqtt: fix reconnect backoff`。
- PR 需说明变更目的与影响范围（模块/协议），提供本地验证方式（命令与日志片段）。
- 涉及配置/网络行为变更请注明默认值与回滚方式。

## 安全与配置提示
- 避免将设备密钥/Token 直接写入源码或提交，优先通过配置文件或环境变量注入。
- 交叉工具链相关问题优先使用 `tools/env_report.sh` 采集环境信息再排查。
