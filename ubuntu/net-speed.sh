#!/bin/bash

PROC_NET_DEV="/proc/net/dev"
DEFAULT_INTERVAL=1
SCRIPT_NAME="net-speed.sh"

format_speed() {
    local bytes=$1
    if (( bytes < 1024 )); then
        printf "%.2f B/s" "$bytes"
    elif (( bytes < 1048576 )); then
        printf "%.2f KB/s" "$(echo "scale=2; $bytes / 1024" | bc)"
    elif (( bytes < 1073741824 )); then
        printf "%.2f MB/s" "$(echo "scale=2; $bytes / 1048576" | bc)"
    else
        printf "%.2f GB/s" "$(echo "scale=2; $bytes / 1073741824" | bc)"
    fi
}

is_physical_interface() {
    local iface=$1
    if [[ $iface == "lo" ]]; then
        return 1
    fi
    if [[ $iface == docker* ]] || [[ $iface == veth* ]] || [[ $iface == br-* ]] || [[ $iface == virbr* ]] || [[ $iface == vboxnet* ]] || [[ $iface == wg* ]] || [[ $iface == tun* ]] || [[ $iface == tap* ]]; then
        return 1
    fi
    return 0
}

get_available_interfaces() {
    local interfaces=()
    if [[ ! -f "$PROC_NET_DEV" ]]; then
        echo "错误: 无法读取 $PROC_NET_DEV"
        exit 1
    fi
    while IFS= read -r line; do
        if [[ "$line" == *:* ]]; then
            local iface=$(echo "$line" | cut -d: -f1 | tr -d ' ')
            if is_physical_interface "$iface"; then
                interfaces+=("$iface")
            fi
        fi
    done < <(tail -n +3 "$PROC_NET_DEV")
    echo "${interfaces[@]}"
}

interface_exists() {
    local iface=$1
    local interfaces=$(get_available_interfaces)
    for i in $interfaces; do
        if [[ "$i" == "$iface" ]]; then
            return 0
        fi
    done
    return 1
}

get_interface_stats() {
    local iface=$1
    local stats_line=$(grep -E "^[[:space:]]*${iface}:" "$PROC_NET_DEV")
    if [[ -z "$stats_line" ]]; then
        return 1
    fi
    local rx_bytes=$(echo "$stats_line" | awk '{print $2}')
    local tx_bytes=$(echo "$stats_line" | awk '{print $10}')
    echo "$rx_bytes $tx_bytes"
}

select_interface() {
    local interfaces=($(get_available_interfaces))
    if [[ ${#interfaces[@]} -eq 0 ]]; then
        echo "错误: 未找到可用的物理网卡"
        exit 1
    fi
    if [[ ${#interfaces[@]} -eq 1 ]]; then
        echo "${interfaces[0]}"
        return
    fi
    echo "可用的物理网卡列表:"
    for i in "${!interfaces[@]}"; do
        echo "  $((i+1)). ${interfaces[$i]}"
    done
    while true; do
        read -p "请输入要监控的网卡编号或名称: " choice
        if [[ "$choice" =~ ^[0-9]+$ ]]; then
            if (( choice >= 1 && choice <= ${#interfaces[@]} )); then
                echo "${interfaces[$((choice-1))]}"
                return
            fi
        else
            for iface in "${interfaces[@]}"; do
                if [[ "$iface" == "$choice" ]]; then
                    echo "$iface"
                    return
                fi
            done
        fi
        echo "无效的选择，请输入有效的编号或网卡名称"
    done
}

cleanup() {
    echo -e "\n监控已停止"
    tput cnorm
    exit 0
}

get_all_interface_stats() {
    local interfaces=("$@")
    local result=""
    for iface in "${interfaces[@]}"; do
        local stats=$(get_interface_stats "$iface")
        if [[ -z "$stats" ]]; then
            echo ""
            return
        fi
        result+="$iface:$stats "
    done
    echo "$result"
}

main() {
    local interval=$DEFAULT_INTERVAL
    local interfaces=()
    local monitor_all=false
    
    if [[ ! -f "$PROC_NET_DEV" ]]; then
        echo "错误: 无法读取 $PROC_NET_DEV，文件不存在"
        exit 1
    fi
    if [[ ! -r "$PROC_NET_DEV" ]]; then
        echo "错误: 无法读取 $PROC_NET_DEV，权限不足"
        exit 1
    fi
    
    if [[ $# -ge 1 ]]; then
        local target_iface=$1
        if ! interface_exists "$target_iface"; then
            echo "错误: 网卡 '$target_iface' 不存在或不是物理网卡"
            echo "可用的物理网卡: $(get_available_interfaces)"
            exit 1
        fi
        interfaces=("$target_iface")
    else
        monitor_all=true
        local all_interfaces=($(get_available_interfaces))
        if [[ ${#all_interfaces[@]} -eq 0 ]]; then
            echo "错误: 未找到可用的物理网卡"
            exit 1
        fi
        interfaces=("${all_interfaces[@]}")
    fi
    
    if [[ $# -ge 2 ]]; then
        if [[ "$2" =~ ^[0-9]+([.][0-9]+)?$ ]] && (( $(echo "$2 > 0" | bc -l) )); then
            interval=$2
        else
            echo "警告: 无效的采样间隔 '$2'，使用默认值 $DEFAULT_INTERVAL 秒"
        fi
    fi
    
    trap cleanup SIGINT SIGTERM
    tput civis
    
    if $monitor_all; then
        echo "正在监控所有物理网卡: ${interfaces[*]} (按 Ctrl+C 退出)"
    else
        echo "正在监控网卡: ${interfaces[0]} (按 Ctrl+C 退出)"
    fi
    echo ""
    printf "%-20s | %-15s | %-20s | %-20s\n" "时间戳" "网卡名称" "接收速率 (RX)" "发送速率 (TX)"
    printf "%s\n" "-------------------------------------------------------------------------------------------------"
    
    declare -A rx_prev
    declare -A tx_prev
    local stats_str=$(get_all_interface_stats "${interfaces[@]}")
    if [[ -z "$stats_str" ]]; then
        echo "错误: 无法获取网卡统计信息"
        tput cnorm
        exit 1
    fi
    
    for entry in $stats_str; do
        local iface=$(echo "$entry" | cut -d: -f1)
        local rx=$(echo "$entry" | cut -d: -f2 | awk '{print $1}')
        local tx=$(echo "$entry" | cut -d: -f2 | awk '{print $2}')
        rx_prev["$iface"]=$rx
        tx_prev["$iface"]=$tx
    done
    
    local time_prev=$(date +%s)
    local first_iteration=true
    local num_interfaces=${#interfaces[@]}
    
    tput sc
    
    while true; do
        sleep $interval
        
        stats_str=$(get_all_interface_stats "${interfaces[@]}")
        if [[ -z "$stats_str" ]]; then
            echo -e "\n错误: 无法获取网卡统计信息"
            tput cnorm
            exit 1
        fi
        
        declare -A rx_curr
        declare -A tx_curr
        for entry in $stats_str; do
            local iface=$(echo "$entry" | cut -d: -f1)
            local rx=$(echo "$entry" | cut -d: -f2 | awk '{print $1}')
            local tx=$(echo "$entry" | cut -d: -f2 | awk '{print $2}')
            rx_curr["$iface"]=$rx
            tx_curr["$iface"]=$tx
        done
        
        local time_curr=$(date +%s)
        local time_diff=$((time_curr - time_prev))
        if (( time_diff == 0 )); then
            time_diff=1
        fi
        
        local timestamp=$(date "+%Y-%m-%d %H:%M:%S")
        
        if ! $first_iteration; then
            tput rc
            tput ed
        fi
        
        first_iteration=false
        
        for iface in "${interfaces[@]}"; do
            local rx1=${rx_prev["$iface"]}
            local tx1=${tx_prev["$iface"]}
            local rx2=${rx_curr["$iface"]}
            local tx2=${tx_curr["$iface"]}
            
            local rx_diff=$((rx2 - rx1))
            local tx_diff=$((tx2 - tx1))
            
            if (( rx_diff < 0 )); then
                rx_diff=0
            fi
            if (( tx_diff < 0 )); then
                tx_diff=0
            fi
            
            local rx_rate=$((rx_diff / time_diff))
            local tx_rate=$((tx_diff / time_diff))
            
            local rx_formatted=$(format_speed $rx_rate)
            local tx_formatted=$(format_speed $tx_rate)
            
            printf "%-20s | %-15s | %-20s | %-20s\n" "$timestamp" "$iface" "$rx_formatted" "$tx_formatted"
            
            rx_prev["$iface"]=$rx2
            tx_prev["$iface"]=$tx2
        done
        
        time_prev=$time_curr
    done
}

main "$@"