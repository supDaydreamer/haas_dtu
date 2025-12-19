#!/usr/bin/env bash
#
# 在当前 shell 会话中启用工程内置的 uClibc toolchain（方案1）。
#
# 用法（必须 source）：
#   source tools/use_uclibc_toolchain.sh
#
set -u

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

toolchain_base="$repo_root/toolchains/OpenWrt-Toolchain-ramips-mt7688_gcc-4.8-linaro_uClibc-0.9.33.2.Linux-x86_64/toolchain-mipsel_24kec+dsp_gcc-4.8-linaro_uClibc-0.9.33.2"
toolchain_bin="$toolchain_base/bin"

if [[ ! -d "$toolchain_bin" ]]; then
	echo "未找到 toolchain bin 目录：$toolchain_bin" >&2
	echo "请确认已解压 OpenWrt-Toolchain-ramips-mt7688_uclibc.tgz 到 $repo_root/toolchains/ 下。" >&2
	return 1
fi

# 放到 PATH 最前，确保覆盖系统里可能存在的 musl 工具链
export PATH="$toolchain_bin:$PATH"

echo "[OK] 已启用 uClibc toolchain:"
echo "  PATH[0] = $toolchain_bin"
echo "  which mipsel-openwrt-linux-gcc = $(command -v mipsel-openwrt-linux-gcc)"
echo -n "  dumpmachine = "
mipsel-openwrt-linux-gcc -dumpmachine
echo -n "  version     = "
mipsel-openwrt-linux-gcc --version | head -n 1

