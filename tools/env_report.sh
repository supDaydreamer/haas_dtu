#!/usr/bin/env bash
#
# 生成一份“可对比”的构建环境报告（不依赖网络）。
#
# 用法：
#   bash tools/env_report.sh
#   bash tools/env_report.sh -o env_A.txt
#
# 然后把两台机器生成的报告做 diff：
#   diff -u env_A.txt env_B.txt | less
#
set -u
set -o pipefail

OUT_FILE=""

usage() {
	cat <<'EOF'
用法: env_report.sh [-o 输出文件]

选项:
  -o FILE   将报告同时保存到 FILE
  -h        显示帮助

可通过环境变量覆盖交叉工具链命令:
  CC (默认: mipsel-openwrt-linux-gcc)
EOF
}

while getopts "o:h" opt; do
	case "$opt" in
	o) OUT_FILE="$OPTARG" ;;
	h)
		usage
		exit 0
		;;
	*)
		usage
		exit 2
		;;
	esac
done

CC="${CC:-mipsel-openwrt-linux-gcc}"

if [[ -n "$OUT_FILE" ]]; then
	mkdir -p "$(dirname "$OUT_FILE")" 2>/dev/null || true
	exec > >(tee "$OUT_FILE") 2>&1
fi

section() {
	echo
	echo "========== $* =========="
}

kv() {
	printf "%-24s %s\n" "$1" "$2"
}

run() {
	echo "+ $*"
	# 不要因为单个命令失败中断整份报告
	"$@" 2>&1 || echo "(命令失败, exit=$?)"
}

exists() {
	command -v "$1" >/dev/null 2>&1
}

check_header() {
	local header="$1"
	printf "%-24s " "header<$header>"
	if printf '#include <%s>\n' "$header" | "$CC" -E -xc - >/dev/null 2>&1; then
		echo "OK"
	else
		echo "MISSING"
	fi
}

check_lib() {
	local lib="$1"
	local so
	local a
	so="$("$CC" -print-file-name="lib${lib}.so" 2>/dev/null || true)"
	a="$("$CC" -print-file-name="lib${lib}.a" 2>/dev/null || true)"

	if [[ "$so" != "lib${lib}.so" && -n "$so" && -e "$so" ]]; then
		printf "%-24s %s\n" "lib${lib}.so" "$so"
	else
		printf "%-24s %s\n" "lib${lib}.so" "NOT FOUND"
	fi

	if [[ "$a" != "lib${lib}.a" && -n "$a" && -e "$a" ]]; then
		printf "%-24s %s\n" "lib${lib}.a" "$a"
	else
		printf "%-24s %s\n" "lib${lib}.a" "NOT FOUND"
	fi
}

section "基本信息"
kv "date" "$(date -Iseconds 2>/dev/null || date)"
kv "hostname" "$(hostname 2>/dev/null || echo N/A)"
kv "pwd" "$(pwd)"
kv "user" "${USER:-N/A}"
kv "shell" "${SHELL:-N/A}"
echo
run uname -a
if [[ -r /etc/os-release ]]; then
	run cat /etc/os-release
fi

section "工具版本"
run make --version | head -n 2
run bash --version | head -n 2
run "$CC" --version | head -n 2

section "交叉工具链定位"
kv "CC" "$CC"
if exists "$CC"; then
	kv "CC(path)" "$(command -v "$CC")"
	run readlink -f "$(command -v "$CC")"
else
	echo "未找到 CC: $CC"
fi

section "GCC 关键信息"
run "$CC" -dumpmachine
run "$CC" -dumpversion
run "$CC" -dumpfullversion
run "$CC" -print-sysroot
run "$CC" -print-search-dirs
echo
echo "# gcc -v (使用预处理方式避免链接 main 失败)"
printf '' | "$CC" -v -E -xc - >/dev/null

section "链接器/归档器"
run "$CC" -print-prog-name=ld
run "$CC" -print-prog-name=ar
LD_BIN="$("$CC" -print-prog-name=ld 2>/dev/null || true)"
AR_BIN="$("$CC" -print-prog-name=ar 2>/dev/null || true)"
if [[ -n "$LD_BIN" ]] && exists "$LD_BIN"; then
	run "$LD_BIN" --version | head -n 2
fi
if [[ -n "$AR_BIN" ]] && exists "$AR_BIN"; then
	run "$AR_BIN" --version | head -n 2
fi

section "libc/动态链接器(用于识别 musl/glibc/uClibc)"
LIBC_SO="$("$CC" -print-file-name=libc.so 2>/dev/null || true)"
LIBC_A="$("$CC" -print-file-name=libc.a 2>/dev/null || true)"
kv "libc.so" "$LIBC_SO"
kv "libc.a" "$LIBC_A"
if [[ -n "$LIBC_SO" && -e "$LIBC_SO" ]] && exists file; then
	run file "$LIBC_SO"
fi
if [[ -n "$LIBC_A" && -e "$LIBC_A" ]] && exists file; then
	run file "$LIBC_A"
fi
if exists "$CC"; then
	# musl 动态链接器名字常见为 ld-musl-*.so.1
	LD_MUSL="$("$CC" -print-file-name=ld-musl-mipsel-sf.so.1 2>/dev/null || true)"
	if [[ -n "$LD_MUSL" && -e "$LD_MUSL" ]]; then
		kv "ld-musl" "$LD_MUSL"
		if exists file; then run file "$LD_MUSL"; fi
	fi
fi

section "头文件可用性(会影响是否能编过)"
check_header "pthread.h"
check_header "zlib.h"
check_header "curl/curl.h"
check_header "openssl/ssl.h"

section "常用库可用性(会影响链接是否成功)"
check_lib "pthread"
check_lib "dl"
check_lib "z"
check_lib "curl"

section "项目相关：new_paho 静态库依赖检查"
PAHO_LIB="src/lib/new_paho/lib/libpaho-mqtt3a.a"
if [[ -e "$PAHO_LIB" ]]; then
	kv "paho(lib)" "$PAHO_LIB"
	if exists file; then run file "$PAHO_LIB"; fi
	if exists mipsel-openwrt-linux-nm; then
		echo "# 未定义符号(节选)"
		mipsel-openwrt-linux-nm -A "$PAHO_LIB" 2>/dev/null | awk '$2=="U"{print $3}' | sort -u | head -n 60
		echo
		echo "# 是否引用 __ctype_b ?"
		# 注意：脚本启用了 pipefail，若使用 grep -q 会导致上游被 SIGPIPE 退出从而误判
		if mipsel-openwrt-linux-nm -A "$PAHO_LIB" 2>/dev/null | awk '$2=="U"{print $3}' | grep -x "__ctype_b" >/dev/null; then
			echo "__ctype_b: YES"
		else
			echo "__ctype_b: NO/UNKNOWN"
		fi
	fi
else
	echo "未找到 $PAHO_LIB"
fi

section "结束"
echo "报告生成完毕。"
