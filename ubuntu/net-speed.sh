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
        read -p "请选择要监控的网卡 (1-${#interfaces[@]}): " choice
        if [[ "$choice" =~ ^[0-9]+$ ]] && (( choice >= 1 && choice <= ${#interfaces[@]} )); then
            echo "${interfaces[$((choice-1))]}"
            return
        fi
        echo "无效的选择，请重试"
    done
}

cleanup() {
    echo -e "\n监控已停止"
    tput cnorm
    exit 0
}

main() {
    local interval=$DEFAULT_INTERVAL
    local target_iface=""
    if [[ ! -f "$PROC_NET_DEV" ]]; then
        echo "错误: 无法读取 $PROC_NET_DEV，文件不存在"
        exit 1
    fi
    if [[ ! -r "$PROC_NET_DEV" ]]; then
        echo "错误: 无法读取 $PROC_NET_DEV，权限不足"
        exit 1
    fi
    if [[ $# -ge 1 ]]; then
        target_iface=$1
        if ! interface_exists "$target_iface"; then
            echo "错误: 网卡 '$target_iface' 不存在或不是物理网卡"
            echo "可用的物理网卡: $(get_available_interfaces)"
            exit 1
        fi
    else
        target_iface=$(select_interface)
        if [[ -z "$target_iface" ]]; then
            echo "错误: 无法选择网卡"
            exit 1
        fi
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
    clear
    printf "%-20s | %-15s | %-20s | %-20s\n" "时间戳" "网卡名称" "接收速率 (RX)" "发送速率 (TX)"
    printf "%s\n" "-------------------------------------------------------------------------------------------------"
    local stats1=$(get_interface_stats "$target_iface")
    if [[ -z "$stats1" ]]; then
        echo "错误: 无法获取网卡 $target_iface 的统计信息"
        tput cnorm
        exit 1
    fi
    local rx1=$(echo "$stats1" | awk '{print $1}')
    local tx1=$(echo "$stats1" | awk '{print $2}')
    local time1=$(date +%s)
    while true; do
        sleep $interval
        local stats2=$(get_interface_stats "$target_iface")
        if [[ -z "$stats2" ]]; then
            echo -e "\n错误: 无法获取网卡 $target_iface 的统计信息"
            tput cnorm
            exit 1
        fi
        local rx2=$(echo "$stats2" | awk '{print $1}')
        local tx2=$(echo "$stats2" | awk '{print $2}')
        local time2=$(date +%s)
        local time_diff=$((time2 - time1))
        if (( time_diff == 0 )); then
            time_diff=1
        fi
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
        local timestamp=$(date "+%Y-%m-%d %H:%M:%S")
        local rx_formatted=$(format_speed $rx_rate)
        local tx_formatted=$(format_speed $tx_rate)
        printf "\r%-20s | %-15s | %-20s | %-20s" "$timestamp" "$target_iface" "$rx_formatted" "$tx_formatted"
        rx1=$rx2
        tx1=$tx2
        time1=$time2
    done
}

main "$@"