#!/bin/bash

if [ "$EUID" -ne 0 ]; then
    echo "错误：此脚本需要以root权限运行"
    exit 1
fi

if [ -z "$1" ]; then
    echo "用法: $0 <gpio_pin>"
    echo "示例: $0 18"
    exit 1
fi

GPIO_PIN=$1
DRIVER_DIR=$(dirname "$(readlink -f "$0")")
MODULE_NAME="gpio_driver"

echo "======================================"
echo "GPIO驱动测试脚本"
echo "GPIO引脚: $GPIO_PIN"
echo "======================================"

echo ""
echo "[1/6] 检查编译环境..."
if [ ! -d "/lib/modules/$(uname -r)/build" ]; then
    echo "错误: 内核头文件未安装"
    echo "请运行: sudo apt install linux-headers-$(uname -r)"
    exit 1
fi
echo "内核头文件已找到"

echo ""
echo "[2/6] 编译内核模块..."
cd "$DRIVER_DIR" || exit 1
make clean > /dev/null 2>&1
if ! make; then
    echo "错误: 内核模块编译失败"
    exit 1
fi
echo "内核模块编译成功"

echo ""
echo "[3/6] 加载内核模块..."
if lsmod | grep -q "$MODULE_NAME"; then
    echo "模块已加载，正在卸载旧模块..."
    rmmod "$MODULE_NAME"
fi

if ! insmod "${MODULE_NAME}.ko" gpio_pin="$GPIO_PIN"; then
    echo "错误: 无法加载内核模块"
    dmesg | tail -5
    exit 1
fi
echo "内核模块加载成功"
echo "查看内核日志:"
dmesg | grep "gpio_driver" | tail -3

echo ""
echo "[4/6] 验证设备节点和sysfs接口..."
DEVICE_NODE="/dev/gpio${GPIO_PIN}"
SYSFS_NODE="/sys/class/gpio_driver/gpio${GPIO_PIN}/value"

if [ -e "$DEVICE_NODE" ]; then
    echo "设备节点已创建: $DEVICE_NODE"
else
    echo "警告: 设备节点未找到"
fi

if [ -e "$SYSFS_NODE" ]; then
    echo "Sysfs节点已创建: $SYSFS_NODE"
else
    echo "警告: Sysfs节点未找到"
fi

echo ""
echo "[5/6] 测试读取GPIO值..."
echo ""
echo "通过字符设备读取 ($DEVICE_NODE):"
if [ -e "$DEVICE_NODE" ]; then
    VALUE=$(cat "$DEVICE_NODE")
    echo "  GPIO值: $VALUE"
else
    echo "  无法读取: 设备节点不存在"
fi

echo ""
echo "通过sysfs接口读取 ($SYSFS_NODE):"
if [ -e "$SYSFS_NODE" ]; then
    VALUE=$(cat "$SYSFS_NODE")
    echo "  GPIO值: $VALUE"
else
    echo "  无法读取: sysfs节点不存在"
fi

echo ""
echo "[6/6] 测试完成，是否卸载模块? (y/n)"
read -r answer
if [ "$answer" = "y" ] || [ "$answer" = "Y" ]; then
    echo "卸载内核模块..."
    rmmod "$MODULE_NAME"
    echo "内核模块已卸载"
    dmesg | grep "gpio_driver" | tail -1
    
    echo "清理编译文件..."
    make clean > /dev/null 2>&1
    echo "清理完成"
else
    echo "模块保持加载状态"
    echo "手动卸载命令: sudo rmmod $MODULE_NAME"
fi

echo ""
echo "======================================"
echo "测试完成!"
echo "======================================"