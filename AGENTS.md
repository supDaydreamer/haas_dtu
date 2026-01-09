# Repository Guidelines

## 项目结构与模块组织
- `src/`：主程序与协议实现（如 `mqtt.c`、`modbus-*.c`、`uart.c`、`udp.c`），入口为 `src/main.c`。
- `src/mcu_sdk/`：MCU 相关接口与协议封装。
- `src/lib/`：内置第三方库（cJSON、mongoose、paho.mqtt.embedded-c、new_paho）。
- `src_1/`：历史版本/参考实现，默认不改动，若需同步请说明原因。
- `build/` 与 `mk.obj/`：构建产物与中间文件；`build/haas_dtu` 为最终输出。
- `config.h`、`src/config.h`：由 configure/autoheader 生成的 libmodbus 配置头，避免手工编辑。

## 构建、测试与开发命令
- `make`：使用交叉编译器构建，输出 `build/haas_dtu`，同时生成 `build/version`。
- `make CROSS=mipsel-openwrt-linux-`：显式指定交叉编译前缀（默认已在 `Makefile` 中）。
- `make IS_DEBUG=0`：关闭调试宏与调试优化。
- `make clean`：清理 `build/` 与 `mk.obj/`。

## 编码风格与命名规范
- 语言为 C（GNU99）；保持与现有代码一致的缩进（tab）与大括号风格。
- 文件命名多为 `lower_snake_case`，协议变体使用 `-tcp`、`-rtu` 等后缀；保持 `.c/.h` 成对出现。
- 私有头文件使用 `*-private.h` 或 `_private.h` 约定，尽量减少全局符号与跨模块耦合。

## 测试指南
- 当前无统一测试框架；可使用 `src/bfbr.test.sh` 进行简单模块测试：
  `cd src && sh bfbr.test.sh`（生成并运行 `test.bfbr`，依赖 `gcc` 与 `pthread`）。
- 对 MQTT、串口、Modbus 等改动建议在目标设备或仿真环境中补充手工验证并记录日志。

## 提交与合并请求规范
- 当前目录未包含 `.git`，无法从历史提交中提炼既有规范。
- 建议使用简短祈使语气并可选模块前缀，例如：`mqtt: fix reconnect`、`modbus: guard timeout`。
- 提交/PR 描述需包含：改动概述、受影响模块、构建命令与测试结果；协议或设备行为变更请附日志或抓包说明。

## 配置与依赖提示
- 构建依赖交叉工具链与系统库（如 `pthread`、`curl`、`zlib`）；如本地缺失需先配置环境。
- `src/lib/new_paho/lib/` 提供静态库；若替换版本请同步头文件与链接选项。
