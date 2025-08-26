#!/bin/bash
# run_perf.sh
# 用法:
#   ./run_perf.sh 0                # 删除 square/mutliply 的 record.txt
#   ./run_perf.sh <square_x> <mutliply_x>   # 启动采样进程
#
# 示例:
#   ./run_perf.sh c d
#   ./run_perf.sh 0

if [[ $# -eq 1 && $1 == "0" ]]; then
    echo "删除记录文件..."
    rm -f ./square/record.txt ./mutliply/record.txt
    echo "完成"
    exit 0
fi

if [[ $# -ne 2 ]]; then
    echo "用法: $0 0   或   $0 <square_x> <mutliply_x>"
    exit 1
fi

SQUARE_X=$1
MUTLIPLY_X=$2

# 启动 square 下的采样程序
cd square/ || exit 1
../../user_perf_record/pcie_reader_pci_fast 0000:fe:0${SQUARE_X}.0 0x440 6 &
PID1=$!

# 启动 mutliply 下的采样程序
cd ../mutliply/ || exit 1
../../user_perf_record/pcie_reader_pci_fast 0000:fe:0${MUTLIPLY_X}.0 0x440 5 &
PID2=$!

# 定义退出清理函数
cleanup() {
    echo "终止采样进程..."
    kill $PID1 $PID2 2>/dev/null
}
trap cleanup EXIT

# 等待子进程执行，避免脚本直接退出
wait

