#!/bin/bash

# 简化的语法测试脚本

# 测试颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m'

# 测试数组定义
test_tools=(
    "dmidecode:硬件信息检测"
    "lscpu:CPU信息检测"
)

# 测试关联数组
declare -A test_status
test_status["dmidecode"]="available"
test_status["lscpu"]="unavailable"

# 测试函数定义
test_function() {
    echo "测试函数执行"
    
    # 测试for循环
    for tool_info in "${test_tools[@]}"; do
        local tool_name="${tool_info%%:*}"
        local tool_desc="${tool_info#*:}"
        echo "工具: $tool_name, 描述: $tool_desc"
    done
    
    # 测试关联数组遍历
    for tool in "${!test_status[@]}"; do
        echo "工具: $tool, 状态: ${test_status[$tool]}"
    done
    
    # 测试if条件
    if [ "${#test_status[@]}" -gt 0 ]; then
        echo "关联数组有 ${#test_status[@]} 个元素"
    fi
    
    # 测试case语句
    local cmd="python3"
    case "$cmd" in
        python3|python)
            echo "检测到Python"
            ;;
        java)
            echo "检测到Java"
            ;;
        *)
            echo "其他语言"
            ;;
    esac
}

# 测试主函数
main() {
    echo "语法测试开始"
    test_function
    echo "语法测试完成"
}

# 运行主函数
main "$@"