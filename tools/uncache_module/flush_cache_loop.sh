#!/usr/bin/env bash
#
# flush_cache_loop.sh — 每 0.1 s 触发一次 wbinvd
#

DEVICE=/dev/flush_cache_all   # 目标设备
INTERVAL=0.1                  # 间隔（秒）

# 捕获 Ctrl-C / kill，优雅退出
trap 'echo -e "\n[+] Stopped"; exit 0' INT TERM

while true; do
    cat "$DEVICE" > /dev/null
    sleep "$INTERVAL"
done

