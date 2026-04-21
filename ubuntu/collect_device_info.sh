#!/bin/bash

# Linux 系统设备信息采集脚本
# 功能：采集硬件信息、软件信息、系统运行状态，并输出到Markdown文档

# 注意：不使用 set -e，因为我们希望脚本在部分工具不可用时继续运行
# 而是使用适当的错误处理和回退机制

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m'

# 输出文件信息
OUTPUT_DIR="."
DEVICE_NAME=$(hostname 2>/dev/null || echo "unknown")
TIMESTAMP=$(date +"%Y%m%d_%H%M%S")
OUTPUT_FILE="${OUTPUT_DIR}/dev-${DEVICE_NAME}-${TIMESTAMP}.md"

# 工具可用性跟踪
declare -A TOOL_STATUS
declare -A SKIPPED_SECTIONS

# 打印信息函数
print_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

print_section() {
    echo -e "${CYAN}========================================${NC}"
    echo -e "${CYAN}  $1${NC}"
    echo -e "${CYAN}========================================${NC}"
}

# 初始化输出文件
init_output_file() {
    print_info "初始化输出文件: $OUTPUT_FILE"
    
    mkdir -p "$OUTPUT_DIR"
    
    cat > "$OUTPUT_FILE" << EOF
# Linux 系统设备信息报告

> 生成时间: $(date +"%Y-%m-%d %H:%M:%S")
> 主机名: $DEVICE_NAME

---

EOF
    print_success "输出文件初始化完成"
}

# 添加内容到输出文件
append_to_file() {
    echo -e "$1" >> "$OUTPUT_FILE"
}

# 添加标题到输出文件
add_section() {
    local title="$1"
    append_to_file "\n## $title\n"
}

# 添加子标题
add_subsection() {
    local title="$1"
    append_to_file "\n### $title\n"
}

# 添加表格行
add_table_row() {
    local key="$1"
    local value="$2"
    append_to_file "| $key | $value |"
}

# 工具检测函数
check_tools() {
    print_section "检测必要工具"
    
    # 定义需要检测的工具及其用途
    local tools=(
        "dmidecode:硬件信息检测(主板/内存/CPU)"
        "lscpu:CPU信息检测"
        "lsblk:磁盘信息检测"
        "fdisk:磁盘分区信息"
        "ip:网络接口信息"
        "ethtool:网卡速率检测"
        "free:内存使用信息"
        "df:磁盘使用信息"
        "top:CPU使用率检测"
        "vmstat:系统状态检测"
        "docker:容器信息检测"
        "mysql:MySQL数据库检测"
        "psql:PostgreSQL数据库检测"
        "mongod:MongoDB数据库检测"
        "redis-cli:Redis数据库检测"
        "netstat:网络连接信息"
        "ss:网络连接信息(替代netstat)"
    )
    
    append_to_file "## 工具检测报告\n"
    append_to_file "| 工具名称 | 用途描述 | 检测状态 |"
    append_to_file "|---------|---------|---------|"
    
    for tool_info in "${tools[@]}"; do
        local tool_name="${tool_info%%:*}"
        local tool_desc="${tool_info#*:}"
        
        if command -v "$tool_name" &> /dev/null; then
            TOOL_STATUS["$tool_name"]="available"
            print_success "✓ $tool_name - 可用"
            add_table_row "$tool_name" "$tool_desc" "✅ 可用"
        else
            TOOL_STATUS["$tool_name"]="unavailable"
            print_warning "✗ $tool_name - 不可用 (将跳过相关检测)"
            add_table_row "$tool_name" "$tool_desc" "❌ 不可用"
        fi
    done
    
    append_to_file ""
    print_info "工具检测完成"
}

# 检查工具是否可用
is_tool_available() {
    local tool="$1"
    if [ "${TOOL_STATUS[$tool]}" = "available" ]; then
        return 0
    else
        return 1
    fi
}

# 记录跳过的检测
record_skip() {
    local section="$1"
    local reason="$2"
    SKIPPED_SECTIONS["$section"]="$reason"
    print_warning "跳过检测: $section - $reason"
}

# ==================== 硬件信息检测模块 ====================

# 1. 主板信息检测
collect_motherboard_info() {
    add_section "主板信息"
    
    if ! is_tool_available "dmidecode"; then
        record_skip "主板信息" "dmidecode工具不可用"
        append_to_file "> ⚠️  主板信息检测已跳过：dmidecode工具不可用\n"
        return
    fi
    
    # 需要root权限运行dmidecode
    if [ "$EUID" -ne 0 ]; then
        record_skip "主板信息" "需要root权限运行dmidecode"
        append_to_file "> ⚠️  主板信息检测已跳过：需要root权限运行dmidecode\n"
        return
    fi
    
    append_to_file "| 项目 | 值 |"
    append_to_file "|-----|-----|"
    
    # 检测主板信息
    local mb_manufacturer=$(dmidecode -s baseboard-manufacturer 2>/dev/null || echo "未知")
    local mb_product=$(dmidecode -s baseboard-product-name 2>/dev/null || echo "未知")
    local mb_serial=$(dmidecode -s baseboard-serial-number 2>/dev/null || echo "未知")
    local mb_version=$(dmidecode -s baseboard-version 2>/dev/null || echo "未知")
    
    # 系统信息
    local sys_manufacturer=$(dmidecode -s system-manufacturer 2>/dev/null || echo "未知")
    local sys_product=$(dmidecode -s system-product-name 2>/dev/null || echo "未知")
    local sys_serial=$(dmidecode -s system-serial-number 2>/dev/null || echo "未知")
    local sys_uuid=$(dmidecode -s system-uuid 2>/dev/null || echo "未知")
    
    add_table_row "主板制造商" "$mb_manufacturer"
    add_table_row "主板型号" "$mb_product"
    add_table_row "主板序列号" "$mb_serial"
    add_table_row "主板版本" "$mb_version"
    add_table_row "系统制造商" "$sys_manufacturer"
    add_table_row "系统型号" "$sys_product"
    add_table_row "系统序列号" "$sys_serial"
    add_table_row "系统UUID" "$sys_uuid"
    
    append_to_file ""
    print_success "主板信息采集完成"
}

# 2. CPU信息检测
collect_cpu_info() {
    add_section "CPU信息"
    
    append_to_file "| 项目 | 值 |"
    append_to_file "|-----|-----|"
    
    # 使用lscpu获取CPU信息
    if is_tool_available "lscpu"; then
        local cpu_info=$(lscpu 2>/dev/null)
        
        local cpu_model=$(echo "$cpu_info" | grep "Model name" | cut -d: -f2 | xargs || echo "未知")
        local cpu_vendor=$(echo "$cpu_info" | grep "Vendor ID" | cut -d: -f2 | xargs || echo "未知")
        local cpu_arch=$(echo "$cpu_info" | grep "Architecture" | cut -d: -f2 | xargs || echo "未知")
        local cpu_cores=$(echo "$cpu_info" | grep "^CPU(s):" | cut -d: -f2 | xargs || echo "未知")
        local cpu_sockets=$(echo "$cpu_info" | grep "Socket(s)" | cut -d: -f2 | xargs || echo "未知")
        local cpu_threads=$(echo "$cpu_info" | grep "Thread(s) per core" | cut -d: -f2 | xargs || echo "未知")
        local cpu_cores_per_socket=$(echo "$cpu_info" | grep "Core(s) per socket" | cut -d: -f2 | xargs || echo "未知")
        local cpu_max_mhz=$(echo "$cpu_info" | grep "CPU max MHz" | cut -d: -f2 | xargs || echo "未知")
        local cpu_min_mhz=$(echo "$cpu_info" | grep "CPU min MHz" | cut -d: -f2 | xargs || echo "未知")
        
        add_table_row "CPU型号" "$cpu_model"
        add_table_row "制造商" "$cpu_vendor"
        add_table_row "架构" "$cpu_arch"
        add_table_row "逻辑CPU总数" "$cpu_cores"
        add_table_row "物理CPU个数" "$cpu_sockets"
        add_table_row "每核线程数" "$cpu_threads"
        add_table_row "每Socket核数" "$cpu_cores_per_socket"
        add_table_row "最大频率" "${cpu_max_mhz} MHz"
        add_table_row "最小频率" "${cpu_min_mhz} MHz"
    else
        record_skip "CPU详细信息" "lscpu工具不可用"
        add_table_row "CPU信息" "⚠️  lscpu不可用，部分信息可能缺失"
    fi
    
    # 尝试使用dmidecode获取CPU序列号等信息
    if is_tool_available "dmidecode" && [ "$EUID" -eq 0 ]; then
        local cpu_serial=$(dmidecode -s processor-serial-number 2>/dev/null | head -1 || echo "未知")
        local cpu_version=$(dmidecode -s processor-version 2>/dev/null | head -1 || echo "未知")
        local cpu_voltage=$(dmidecode -s processor-voltage 2>/dev/null | head -1 || echo "未知")
        local cpu_speed=$(dmidecode -s processor-frequency 2>/dev/null | head -1 || echo "未知")
        
        add_table_row "CPU序列号" "$cpu_serial"
        add_table_row "CPU版本" "$cpu_version"
        add_table_row "CPU电压" "$cpu_voltage"
        add_table_row "CPU频率" "$cpu_speed"
    fi
    
    # 从/proc/cpuinfo获取更多信息
    if [ -f /proc/cpuinfo ]; then
        local cpu_count=$(grep -c "^processor" /proc/cpuinfo 2>/dev/null || echo "未知")
        local physical_count=$(grep "^physical id" /proc/cpuinfo | sort -u | wc -l 2>/dev/null || echo "未知")
        
        if [ "$cpu_count" != "未知" ]; then
            add_table_row "处理器个数(/proc)" "$cpu_count"
        fi
        if [ "$physical_count" != "未知" ]; then
            add_table_row "物理CPU个数(/proc)" "$physical_count"
        fi
    fi
    
    append_to_file ""
    print_success "CPU信息采集完成"
}

# 3. 内存信息检测
collect_memory_info() {
    add_section "内存信息"
    
    # 先获取总内存信息
    add_subsection "内存概览"
    
    append_to_file "| 项目 | 值 |"
    append_to_file "|-----|-----|"
    
    if is_tool_available "free"; then
        local mem_info=$(free -h 2>/dev/null)
        local total_mem=$(echo "$mem_info" | grep "Mem:" | awk '{print $2}')
        local used_mem=$(echo "$mem_info" | grep "Mem:" | awk '{print $3}')
        local free_mem=$(echo "$mem_info" | grep "Mem:" | awk '{print $4}')
        local shared_mem=$(echo "$mem_info" | grep "Mem:" | awk '{print $5}')
        local buffer_mem=$(echo "$mem_info" | grep "Mem:" | awk '{print $6}')
        local available_mem=$(echo "$mem_info" | grep "Mem:" | awk '{print $7}')
        local swap_total=$(echo "$mem_info" | grep "Swap:" | awk '{print $2}')
        local swap_used=$(echo "$mem_info" | grep "Swap:" | awk '{print $3}')
        local swap_free=$(echo "$mem_info" | grep "Swap:" | awk '{print $4}')
        
        add_table_row "总内存" "$total_mem"
        add_table_row "已使用" "$used_mem"
        add_table_row "空闲" "$free_mem"
        add_table_row "共享内存" "$shared_mem"
        add_table_row "缓存" "$buffer_mem"
        add_table_row "可用内存" "$available_mem"
        add_table_row "交换分区总计" "$swap_total"
        add_table_row "交换分区已用" "$swap_used"
        add_table_row "交换分区空闲" "$swap_free"
    else
        record_skip "内存概览" "free工具不可用"
        add_table_row "内存信息" "⚠️  free工具不可用"
    fi
    
    append_to_file ""
    
    # 详细内存模块信息
    add_subsection "内存模块详细信息"
    
    if is_tool_available "dmidecode" && [ "$EUID" -eq 0 ]; then
        # 获取所有内存设备信息
        local mem_devices=$(dmidecode -t memory 2>/dev/null)
        
        # 统计内存插槽数
        local total_slots=$(echo "$mem_devices" | grep -c "Memory Device" 2>/dev/null || echo "0")
        local populated_slots=$(echo "$mem_devices" | grep -A 20 "Memory Device" | grep -c "Size:.*MB\|Size:.*GB" 2>/dev/null || echo "0")
        
        append_to_file "**内存插槽统计:** 总计 $total_slots 个插槽，已使用 $populated_slots 个\n"
        
        # 详细列出每个内存模块
        local idx=1
        while IFS= read -r device; do
            if echo "$device" | grep -q "Size:"; then
                local size=$(echo "$device" | grep "Size:" | head -1 | cut -d: -f2 | xargs)
                if [ "$size" != "No Module Installed" ]; then
                    append_to_file "**内存条 #$idx:**\n"
                    append_to_file "| 项目 | 值 |"
                    append_to_file "|-----|-----|"
                    
                    local form_factor=$(echo "$device" | grep "Form Factor:" | head -1 | cut -d: -f2 | xargs || echo "未知")
                    local type=$(echo "$device" | grep "Type:" | head -1 | cut -d: -f2 | xargs || echo "未知")
                    local type_detail=$(echo "$device" | grep "Type Detail:" | head -1 | cut -d: -f2 | xargs || echo "未知")
                    local speed=$(echo "$device" | grep "Speed:" | head -1 | cut -d: -f2 | xargs || echo "未知")
                    local manufacturer=$(echo "$device" | grep "Manufacturer:" | head -1 | cut -d: -f2 | xargs || echo "未知")
                    local serial=$(echo "$device" | grep "Serial Number:" | head -1 | cut -d: -f2 | xargs || echo "未知")
                    local part_number=$(echo "$device" | grep "Part Number:" | head -1 | cut -d: -f2 | xargs || echo "未知")
                    local rank=$(echo "$device" | grep "Rank:" | head -1 | cut -d: -f2 | xargs || echo "未知")
                    local config_clock=$(echo "$device" | grep "Configured Memory Speed:" | head -1 | cut -d: -f2 | xargs || echo "未知")
                    
                    add_table_row "容量" "$size"
                    add_table_row "规格" "$form_factor"
                    add_table_row "类型" "$type"
                    add_table_row "类型详情" "$type_detail"
                    add_table_row "速率" "$speed"
                    add_table_row "配置速率" "$config_clock"
                    add_table_row "制造商" "$manufacturer"
                    add_table_row "序列号" "$serial"
                    add_table_row "部件号" "$part_number"
                    add_table_row "Rank" "$rank"
                    
                    append_to_file ""
                    ((idx++))
                fi
            fi
        done < <(echo "$mem_devices" | awk '/Memory Device/,/^$/' RS=)
    else
        record_skip "内存模块详细信息" "需要dmidecode工具和root权限"
        append_to_file "> ⚠️  内存模块详细信息检测已跳过：需要dmidecode工具和root权限\n"
    fi
    
    print_success "内存信息采集完成"
}

# 4. 磁盘信息检测
collect_disk_info() {
    add_section "磁盘信息"
    
    # 磁盘概览
    add_subsection "磁盘概览"
    
    append_to_file "| 项目 | 值 |"
    append_to_file "|-----|-----|"
    
    local disk_count=0
    
    # 使用lsblk获取磁盘信息
    if is_tool_available "lsblk"; then
        local disks=$(lsblk -d -o NAME,SIZE,MODEL,VENDOR,SERIAL,TYPE,TRAN -n 2>/dev/null)
        
        disk_count=$(echo "$disks" | grep -v "^$" | wc -l)
        add_table_row "磁盘设备总数" "$disk_count"
        append_to_file ""
        
        # 详细列出每个磁盘
        add_subsection "磁盘设备详情"
        
        while IFS= read -r line; do
            if [ -n "$line" ]; then
                local name=$(echo "$line" | awk '{print $1}')
                local size=$(echo "$line" | awk '{print $2}')
                local model=$(echo "$line" | awk '{print $3}' | tr '_' ' ' || echo "未知")
                local vendor=$(echo "$line" | awk '{print $4}' || echo "未知")
                local serial=$(echo "$line" | awk '{print $5}' || echo "未知")
                local type=$(echo "$line" | awk '{print $6}' || echo "未知")
                local tran=$(echo "$line" | awk '{print $7}' || echo "未知")
                
                append_to_file "**磁盘设备: /dev/$name**\n"
                append_to_file "| 项目 | 值 |"
                append_to_file "|-----|-----|"
                
                add_table_row "设备名" "/dev/$name"
                add_table_row "容量" "$size"
                add_table_row "型号" "$model"
                add_table_row "制造商" "$vendor"
                add_table_row "序列号" "$serial"
                add_table_row "类型" "$type"
                add_table_row "传输协议" "$tran"
                
                # 获取挂载点和分区信息
                local mount_points=$(lsblk -o NAME,MOUNTPOINT -n /dev/"$name" 2>/dev/null | grep -v "NAME" | grep -v "^$name " | awk '{print $2}' | grep -v "^$" | sort -u)
                if [ -n "$mount_points" ]; then
                    add_table_row "挂载点" "$(echo "$mount_points" | tr '\n' ', ')"
                fi
                
                # 使用hdparm获取更多信息（需要root）
                if [ "$EUID" -eq 0 ] && command -v hdparm &>/dev/null; then
                    local hdparm_info=$(hdparm -I /dev/"$name" 2>/dev/null || true)
                    local fw_rev=$(echo "$hdparm_info" | grep "Firmware Revision" | cut -d: -f2 | xargs || echo "未知")
                    local ata_version=$(echo "$hdparm_info" | grep "ATA Version" | head -1 | cut -d: -f2 | xargs || echo "未知")
                    
                    add_table_row "固件版本" "$fw_rev"
                    add_table_row "ATA版本" "$ata_version"
                fi
                
                append_to_file ""
            fi
        done < <(echo "$disks")
    else
        record_skip "磁盘信息" "lsblk工具不可用"
        add_table_row "磁盘信息" "⚠️  lsblk工具不可用"
        append_to_file ""
    fi
    
    # 磁盘使用情况
    add_subsection "磁盘使用情况"
    
    if is_tool_available "df"; then
        append_to_file "| 文件系统 | 总大小 | 已使用 | 可用 | 使用率 | 挂载点 |"
        append_to_file "|---------|-------|-------|-----|-------|-------|"
        
        df -h --output=source,size,used,avail,pcent,target 2>/dev/null | tail -n +2 | while IFS= read -r line; do
            if [ -n "$line" ]; then
                local fs=$(echo "$line" | awk '{print $1}')
                local size=$(echo "$line" | awk '{print $2}')
                local used=$(echo "$line" | awk '{print $3}')
                local avail=$(echo "$line" | awk '{print $4}')
                local pcent=$(echo "$line" | awk '{print $5}')
                local target=$(echo "$line" | awk '{print $6}')
                
                append_to_file "| $fs | $size | $used | $avail | $pcent | $target |"
            fi
        done
    else
        record_skip "磁盘使用情况" "df工具不可用"
        append_to_file "> ⚠️  磁盘使用情况检测已跳过：df工具不可用\n"
    fi
    
    append_to_file ""
    print_success "磁盘信息采集完成"
}

# 5. 网卡信息检测
collect_network_info() {
    add_section "网卡信息"
    
    add_subsection "网络接口概览"
    
    append_to_file "| 项目 | 值 |"
    append_to_file "|-----|-----|"
    
    local interface_count=0
    
    # 使用ip命令获取网卡信息
    if is_tool_available "ip"; then
        local interfaces=$(ip -br link show 2>/dev/null | grep -v "LOOPBACK" | awk '{print $1}')
        interface_count=$(echo "$interfaces" | grep -v "^$" | wc -l)
        
        add_table_row "网络接口总数" "$interface_count"
        append_to_file ""
        
        # 详细列出每个网卡
        add_subsection "网络接口详情"
        
        while IFS= read -r iface; do
            if [ -n "$iface" ]; then
                append_to_file "**网络接口: $iface**\n"
                append_to_file "| 项目 | 值 |"
                append_to_file "|-----|-----|"
                
                # 获取接口基本信息
                local link_info=$(ip -s link show "$iface" 2>/dev/null)
                local mac_addr=$(ip link show "$iface" 2>/dev/null | grep "link/ether" | awk '{print $2}' || echo "未知")
                local state=$(ip -br link show "$iface" 2>/dev/null | awk '{print $2}' || echo "未知")
                local mtu=$(ip link show "$iface" 2>/dev/null | grep "mtu" | awk '{print $5}' || echo "未知")
                
                # 获取IP地址
                local ipv4_addrs=$(ip -4 addr show "$iface" 2>/dev/null | grep "inet" | awk '{print $2}' | tr '\n' ', ' || echo "无")
                local ipv6_addrs=$(ip -6 addr show "$iface" 2>/dev/null | grep "inet6" | grep -v "fe80" | awk '{print $2}' | tr '\n' ', ' || echo "无")
                local link_local=$(ip -6 addr show "$iface" 2>/dev/null | grep "inet6" | grep "fe80" | awk '{print $2}' | head -1 || echo "无")
                
                add_table_row "接口名称" "$iface"
                add_table_row "MAC地址" "$mac_addr"
                add_table_row "状态" "$state"
                add_table_row "MTU" "$mtu"
                add_table_row "IPv4地址" "$ipv4_addrs"
                add_table_row "IPv6地址" "$ipv6_addrs"
                add_table_row "链路本地地址" "$link_local"
                
                # 使用ethtool获取网卡速率和驱动信息
                if is_tool_available "ethtool"; then
                    local ethtool_info=$(ethtool "$iface" 2>/dev/null || true)
                    local speed=$(echo "$ethtool_info" | grep "Speed:" | cut -d: -f2 | xargs || echo "未知")
                    local duplex=$(echo "$ethtool_info" | grep "Duplex:" | cut -d: -f2 | xargs || echo "未知")
                    local port=$(echo "$ethtool_info" | grep "Port:" | cut -d: -f2 | xargs || echo "未知")
                    local auto_negotiation=$(echo "$ethtool_info" | grep "Auto-negotiation:" | cut -d: -f2 | xargs || echo "未知")
                    
                    add_table_row "速率" "$speed"
                    add_table_row "双工模式" "$duplex"
                    add_table_row "端口类型" "$port"
                    add_table_row "自动协商" "$auto_negotiation"
                    
                    # 获取驱动信息
                    local driver_info=$(ethtool -i "$iface" 2>/dev/null || true)
                    local driver=$(echo "$driver_info" | grep "driver:" | cut -d: -f2 | xargs || echo "未知")
                    local driver_version=$(echo "$driver_info" | grep "version:" | cut -d: -f2 | xargs || echo "未知")
                    local firmware_version=$(echo "$driver_info" | grep "firmware-version:" | cut -d: -f2 | xargs || echo "未知")
                    local bus_info=$(echo "$driver_info" | grep "bus-info:" | cut -d: -f2 | xargs || echo "未知")
                    
                    add_table_row "驱动名称" "$driver"
                    add_table_row "驱动版本" "$driver_version"
                    add_table_row "固件版本" "$firmware_version"
                    add_table_row "总线信息" "$bus_info"
                fi
                
                # 获取统计信息
                local rx_bytes=$(echo "$link_info" | grep -A 1 "RX:" | tail -1 | awk '{print $1}' || echo "0")
                local tx_bytes=$(echo "$link_info" | grep -A 1 "TX:" | tail -1 | awk '{print $1}' || echo "0")
                local rx_packets=$(echo "$link_info" | grep "RX:" | head -1 | awk '{print $3}' || echo "0")
                local tx_packets=$(echo "$link_info" | grep "TX:" | head -1 | awk '{print $3}' || echo "0")
                local rx_errors=$(echo "$link_info" | grep "RX:" | head -1 | awk '{print $4}' || echo "0")
                local tx_errors=$(echo "$link_info" | grep "TX:" | head -1 | awk '{print $4}' || echo "0")
                local rx_dropped=$(echo "$link_info" | grep "RX:" | head -1 | awk '{print $5}' || echo "0")
                local tx_dropped=$(echo "$link_info" | grep "TX:" | head -1 | awk '{print $5}' || echo "0")
                
                # 转换字节为可读格式
                local rx_human=""
                local tx_human=""
                if [ "$rx_bytes" -gt 1073741824 ]; then
                    rx_human="$(echo "scale=2; $rx_bytes/1073741824" | bc 2>/dev/null) GB"
                elif [ "$rx_bytes" -gt 1048576 ]; then
                    rx_human="$(echo "scale=2; $rx_bytes/1048576" | bc 2>/dev/null) MB"
                elif [ "$rx_bytes" -gt 1024 ]; then
                    rx_human="$(echo "scale=2; $rx_bytes/1024" | bc 2>/dev/null) KB"
                else
                    rx_human="${rx_bytes} Bytes"
                fi
                
                if [ "$tx_bytes" -gt 1073741824 ]; then
                    tx_human="$(echo "scale=2; $tx_bytes/1073741824" | bc 2>/dev/null) GB"
                elif [ "$tx_bytes" -gt 1048576 ]; then
                    tx_human="$(echo "scale=2; $tx_bytes/1048576" | bc 2>/dev/null) MB"
                elif [ "$tx_bytes" -gt 1024 ]; then
                    tx_human="$(echo "scale=2; $tx_bytes/1024" | bc 2>/dev/null) KB"
                else
                    tx_human="${tx_bytes} Bytes"
                fi
                
                add_table_row "接收字节数" "$rx_human"
                add_table_row "发送字节数" "$tx_human"
                add_table_row "接收数据包" "$rx_packets"
                add_table_row "发送数据包" "$tx_packets"
                add_table_row "接收错误" "$rx_errors"
                add_table_row "发送错误" "$tx_errors"
                add_table_row "接收丢弃" "$rx_dropped"
                add_table_row "发送丢弃" "$tx_dropped"
                
                append_to_file ""
            fi
        done < <(echo "$interfaces")
    else
        record_skip "网卡信息" "ip工具不可用"
        add_table_row "网卡信息" "⚠️  ip工具不可用"
        append_to_file ""
    fi
    
    # 路由信息
    add_subsection "路由信息"
    
    if is_tool_available "ip"; then
        append_to_file "| 目标网络 | 网关 | 接口 | 协议 | 来源 |"
        append_to_file "|---------|-----|-----|-----|-----|"
        
        ip route show 2>/dev/null | while IFS= read -r line; do
            if [ -n "$line" ]; then
                local target=$(echo "$line" | awk '{print $1}')
                local gw=$(echo "$line" | grep "via" | awk '{print $3}' || echo "直连")
                local dev=$(echo "$line" | grep "dev" | awk '{for(i=1;i<=NF;i++) if($i=="dev") print $(i+1)}' || echo "未知")
                local proto=$(echo "$line" | grep "proto" | awk '{for(i=1;i<=NF;i++) if($i=="proto") print $(i+1)}' || echo "未知")
                local src=$(echo "$line" | grep "src" | awk '{for(i=1;i<=NF;i++) if($i=="src") print $(i+1)}' || echo "-")
                
                append_to_file "| $target | $gw | $dev | $proto | $src |"
            fi
        done
    else
        append_to_file "> ⚠️  路由信息检测已跳过：ip工具不可用\n"
    fi
    
    append_to_file ""
    
    # DNS配置
    add_subsection "DNS配置"
    
    if [ -f /etc/resolv.conf ]; then
        append_to_file "| 类型 | 值 |"
        append_to_file "|-----|-----|"
        
        local nameservers=$(grep "^nameserver" /etc/resolv.conf 2>/dev/null | awk '{print $2}' || echo "无")
        local search=$(grep "^search" /etc/resolv.conf 2>/dev/null | awk '{print $2}' || echo "无")
        local domain=$(grep "^domain" /etc/resolv.conf 2>/dev/null | awk '{print $2}' || echo "无")
        
        add_table_row "DNS服务器" "$(echo "$nameservers" | tr '\n' ', ')"
        add_table_row "搜索域" "$search"
        add_table_row "本地域" "$domain"
    else
        append_to_file "> ⚠️  未找到/etc/resolv.conf文件\n"
    fi
    
    append_to_file ""
    print_success "网卡信息采集完成"
}

# 6. 其他硬件信息
collect_other_hardware_info() {
    add_section "其他硬件信息"
    
    add_subsection "PCI设备"
    
    if command -v lspci &>/dev/null; then
        append_to_file "| 设备类型 | 设备描述 |"
        append_to_file "|---------|---------|"
        
        # 分类显示PCI设备
        local categories=(
            "VGA compatible controller:显卡"
            "Audio device:音频设备"
            "Network controller:无线网卡"
            "Ethernet controller:有线网卡"
            "USB controller:USB控制器"
            "SATA controller:SATA控制器"
            "RAID bus controller:RAID控制器"
            "Serial controller:串口控制器"
            "Bridge:桥接器"
            "Memory controller:内存控制器"
        )
        
        for cat_info in "${categories[@]}"; do
            local cat_key="${cat_info%%:*}"
            local cat_name="${cat_info#*:}"
            
            local devices=$(lspci 2>/dev/null | grep -i "$cat_key" | head -5)
            if [ -n "$devices" ]; then
                while IFS= read -r dev; do
                    local desc=$(echo "$dev" | cut -d: -f3- | xargs)
                    append_to_file "| $cat_name | $desc |"
                done < <(echo "$devices")
            fi
        done
        
        # 显示所有PCI设备（简要）
        append_to_file ""
        append_to_file "**完整PCI设备列表:**\n"
        append_to_file "\`\`\`"
        lspci -tv 2>/dev/null >> "$OUTPUT_FILE" || echo "无法获取PCI树" >> "$OUTPUT_FILE"
        append_to_file "\`\`\`"
    else
        record_skip "PCI设备信息" "lspci工具不可用"
        append_to_file "> ⚠️  PCI设备信息检测已跳过：lspci工具不可用\n"
    fi
    
    append_to_file ""
    
    # USB设备
    add_subsection "USB设备"
    
    if command -v lsusb &>/dev/null; then
        append_to_file "| 总线 | 设备 | 厂商ID | 描述 |"
        append_to_file "|-----|-----|-------|-----|"
        
        lsusb 2>/dev/null | while IFS= read -r line; do
            if [ -n "$line" ]; then
                local bus=$(echo "$line" | awk '{print $2}')
                local device=$(echo "$line" | awk '{print $4}' | tr -d ':')
                local vendor=$(echo "$line" | awk '{print $6}' | cut -d: -f1)
                local product=$(echo "$line" | awk '{print $6}' | cut -d: -f2)
                local desc=$(echo "$line" | cut -d' ' -f7- | xargs)
                
                append_to_file "| $bus | $device | ${vendor}:${product} | $desc |"
            fi
        done
        
        append_to_file ""
        append_to_file "**USB设备树:**\n"
        append_to_file "\`\`\`"
        lsusb -t 2>/dev/null >> "$OUTPUT_FILE" || echo "无法获取USB树" >> "$OUTPUT_FILE"
        append_to_file "\`\`\`"
    else
        record_skip "USB设备信息" "lsusb工具不可用"
        append_to_file "> ⚠️  USB设备信息检测已跳过：lsusb工具不可用\n"
    fi
    
    append_to_file ""
    
    # BIOS信息
    add_subsection "BIOS信息"
    
    if is_tool_available "dmidecode" && [ "$EUID" -eq 0 ]; then
        append_to_file "| 项目 | 值 |"
        append_to_file "|-----|-----|"
        
        local bios_vendor=$(dmidecode -s bios-vendor 2>/dev/null || echo "未知")
        local bios_version=$(dmidecode -s bios-version 2>/dev/null || echo "未知")
        local bios_release=$(dmidecode -s bios-release-date 2>/dev/null || echo "未知")
        local bios_rom_size=$(dmidecode -s bios-rom-size 2>/dev/null || echo "未知")
        local bios_revision=$(dmidecode -s bios-revision 2>/dev/null || echo "未知")
        local firmware_revision=$(dmidecode -s firmware-revision 2>/dev/null || echo "未知")
        
        add_table_row "BIOS厂商" "$bios_vendor"
        add_table_row "BIOS版本" "$bios_version"
        add_table_row "BIOS发布日期" "$bios_release"
        add_table_row "BIOS ROM大小" "$bios_rom_size"
        add_table_row "BIOS修订版本" "$bios_revision"
        add_table_row "固件修订版本" "$firmware_revision"
    else
        record_skip "BIOS信息" "需要dmidecode工具和root权限"
        append_to_file "> ⚠️  BIOS信息检测已跳过：需要dmidecode工具和root权限\n"
    fi
    
    append_to_file ""
    print_success "其他硬件信息采集完成"
}

# ==================== 软件信息检测模块 ====================

# 1. 操作系统信息
collect_os_info() {
    add_section "操作系统信息"
    
    append_to_file "| 项目 | 值 |"
    append_to_file "|-----|-----|"
    
    # 从/etc/os-release获取信息
    if [ -f /etc/os-release ]; then
        local pretty_name=$(grep "^PRETTY_NAME=" /etc/os-release | cut -d= -f2 | tr -d '"' || echo "未知")
        local name=$(grep "^NAME=" /etc/os-release | cut -d= -f2 | tr -d '"' || echo "未知")
        local version=$(grep "^VERSION=" /etc/os-release | cut -d= -f2 | tr -d '"' || echo "未知")
        local version_id=$(grep "^VERSION_ID=" /etc/os-release | cut -d= -f2 | tr -d '"' || echo "未知")
        local id=$(grep "^ID=" /etc/os-release | cut -d= -f2 | tr -d '"' || echo "未知")
        local id_like=$(grep "^ID_LIKE=" /etc/os-release | cut -d= -f2 | tr -d '"' || echo "未知")
        local home_url=$(grep "^HOME_URL=" /etc/os-release | cut -d= -f2 | tr -d '"' || echo "未知")
        
        add_table_row "操作系统名称" "$pretty_name"
        add_table_row "系统名称" "$name"
        add_table_row "系统版本" "$version"
        add_table_row "版本ID" "$version_id"
        add_table_row "系统ID" "$id"
        add_table_row "衍生自" "$id_like"
        add_table_row "官方网站" "$home_url"
    fi
    
    # 内核信息
    local kernel_version=$(uname -r 2>/dev/null || echo "未知")
    local kernel_release=$(uname -v 2>/dev/null || echo "未知")
    local arch=$(uname -m 2>/dev/null || echo "未知")
    local hostname=$(uname -n 2>/dev/null || echo "未知")
    
    add_table_row "内核版本" "$kernel_version"
    add_table_row "内核发布信息" "$kernel_release"
    add_table_row "系统架构" "$arch"
    add_table_row "主机名" "$hostname"
    
    # 启动时间
    if command -v uptime &>/dev/null; then
        local uptime_info=$(uptime -s 2>/dev/null || echo "未知")
        add_table_row "系统启动时间" "$uptime_info"
    fi
    
    # 运行时间
    if [ -f /proc/uptime ]; then
        local uptime_sec=$(awk '{print $1}' /proc/uptime)
        local days=$((uptime_sec / 86400))
        local hours=$(( (uptime_sec % 86400) / 3600 ))
        local mins=$(( (uptime_sec % 3600) / 60 ))
        add_table_row "运行时间" "${days}天 ${hours}小时 ${mins}分钟"
    fi
    
    # 语言环境
    local lang=$(echo "$LANG" 2>/dev/null || echo "未知")
    add_table_row "默认语言" "$lang"
    
    # 时区
    if [ -f /etc/timezone ]; then
        local timezone=$(cat /etc/timezone 2>/dev/null || echo "未知")
        add_table_row "时区" "$timezone"
    elif [ -f /etc/sysconfig/clock ]; then
        local timezone=$(grep "^ZONE=" /etc/sysconfig/clock | cut -d= -f2 | tr -d '"' 2>/dev/null || echo "未知")
        add_table_row "时区" "$timezone"
    fi
    
    append_to_file ""
    print_success "操作系统信息采集完成"
}

# 2. 数据库信息检测
collect_database_info() {
    add_section "数据库信息"
    
    add_subsection "MySQL/MariaDB"
    
    if is_tool_available "mysql"; then
        append_to_file "| 项目 | 值 |"
        append_to_file "|-----|-----|"
        
        # 检测MySQL版本
        local mysql_version=$(mysql --version 2>/dev/null || echo "未知")
        add_table_row "客户端版本" "$mysql_version"
        
        # 尝试连接检测服务状态
        if mysqladmin ping 2>/dev/null; then
            add_table_row "服务状态" "✅ 运行中"
            
            # 获取更多信息（需要无密码或配置文件）
            local mysql_info=$(mysql -e "SHOW VARIABLES LIKE 'version%'; SHOW VARIABLES LIKE 'basedir%'; SHOW VARIABLES LIKE 'datadir%';" 2>/dev/null || true)
            local version=$(echo "$mysql_info" | grep "version\s" | awk '{print $2}' || echo "未知")
            local version_comment=$(echo "$mysql_info" | grep "version_comment" | awk '{print substr($0, index($0,$2))}' || echo "未知")
            local basedir=$(echo "$mysql_info" | grep "basedir" | awk '{print $2}' || echo "未知")
            local datadir=$(echo "$mysql_info" | grep "datadir" | awk '{print $2}' || echo "未知")
            
            add_table_row "数据库版本" "$version"
            add_table_row "版本说明" "$version_comment"
            add_table_row "安装目录" "$basedir"
            add_table_row "数据目录" "$datadir"
        else
            add_table_row "服务状态" "❌ 无法连接或未运行"
        fi
    else
        append_to_file "> ⚠️  未检测到MySQL/MariaDB客户端\n"
    fi
    
    append_to_file ""
    
    add_subsection "PostgreSQL"
    
    if is_tool_available "psql"; then
        append_to_file "| 项目 | 值 |"
        append_to_file "|-----|-----|"
        
        local psql_version=$(psql --version 2>/dev/null || echo "未知")
        add_table_row "客户端版本" "$psql_version"
        
        # 检测服务
        if pg_isready 2>/dev/null; then
            add_table_row "服务状态" "✅ 运行中"
        else
            add_table_row "服务状态" "❌ 无法连接或未运行"
        fi
    else
        append_to_file "> ⚠️  未检测到PostgreSQL客户端\n"
    fi
    
    append_to_file ""
    
    add_subsection "MongoDB"
    
    if is_tool_available "mongod"; then
        append_to_file "| 项目 | 值 |"
        append_to_file "|-----|-----|"
        
        local mongo_version=$(mongod --version 2>/dev/null | head -1 || echo "未知")
        add_table_row "服务端版本" "$mongo_version"
        
        # 检测服务
        if command -v mongosh &>/dev/null; then
            if mongosh --eval "db.adminCommand('ping')" 2>/dev/null; then
                add_table_row "服务状态" "✅ 运行中"
            else
                add_table_row "服务状态" "❌ 无法连接或未运行"
            fi
        elif command -v mongo &>/dev/null; then
            if mongo --eval "db.adminCommand('ping')" 2>/dev/null; then
                add_table_row "服务状态" "✅ 运行中"
            else
                add_table_row "服务状态" "❌ 无法连接或未运行"
            fi
        fi
    else
        append_to_file "> ⚠️  未检测到MongoDB服务端\n"
    fi
    
    append_to_file ""
    
    add_subsection "Redis"
    
    if is_tool_available "redis-cli"; then
        append_to_file "| 项目 | 值 |"
        append_to_file "|-----|-----|"
        
        local redis_version=$(redis-cli --version 2>/dev/null || echo "未知")
        add_table_row "客户端版本" "$redis_version"
        
        # 检测服务
        if redis-cli ping 2>/dev/null | grep -q "PONG"; then
            add_table_row "服务状态" "✅ 运行中"
            
            # 获取更多信息
            local info=$(redis-cli info server 2>/dev/null || true)
            local version=$(echo "$info" | grep "redis_version:" | cut -d: -f2 | tr -d '\r' || echo "未知")
            local mode=$(echo "$info" | grep "redis_mode:" | cut -d: -f2 | tr -d '\r' || echo "未知")
            local os=$(echo "$info" | grep "os:" | cut -d: -f2 | tr -d '\r' || echo "未知")
            local arch_bits=$(echo "$info" | grep "arch_bits:" | cut -d: -f2 | tr -d '\r' || echo "未知")
            local gcc_version=$(echo "$info" | grep "gcc_version:" | cut -d: -f2 | tr -d '\r' || echo "未知")
            
            add_table_row "服务端版本" "$version"
            add_table_row "运行模式" "$mode"
            add_table_row "操作系统" "$os"
            add_table_row "架构" "${arch_bits}位"
            add_table_row "GCC版本" "$gcc_version"
        else
            add_table_row "服务状态" "❌ 无法连接或未运行"
        fi
    else
        append_to_file "> ⚠️  未检测到Redis客户端\n"
    fi
    
    append_to_file ""
    
    # 检测其他数据库进程
    add_subsection "其他数据库进程检测"
    
    append_to_file "| 数据库类型 | 进程状态 | 进程信息 |"
    append_to_file "|---------|---------|---------|"
    
    local db_processes=(
        "mysqld:MySQL/MariaDB"
        "mariadbd:MariaDB"
        "postgres:PostgreSQL"
        "mongod:MongoDB"
        "redis-server:Redis"
        "oracle:Oracle"
        "db2sysc:DB2"
        "sqlservr:SQL Server"
    )
    
    for proc_info in "${db_processes[@]}"; do
        local proc_name="${proc_info%%:*}"
        local db_name="${proc_info#*:}"
        
        local proc_count=$(pgrep -c "$proc_name" 2>/dev/null || echo "0")
        if [ "$proc_count" -gt 0 ]; then
            local proc_details=$(pgrep -a "$proc_name" 2>/dev/null | head -1 | cut -d' ' -f2- || echo "运行中")
            append_to_file "| $db_name | ✅ 运行中 ($proc_count个进程) | $proc_details |"
        fi
    done
    
    append_to_file ""
    print_success "数据库信息采集完成"
}

# 3. 容器组件信息检测
collect_container_info() {
    add_section "容器组件信息"
    
    add_subsection "Docker"
    
    if is_tool_available "docker"; then
        append_to_file "| 项目 | 值 |"
        append_to_file "|-----|-----|"
        
        # Docker版本信息
        local docker_version=$(docker --version 2>/dev/null || echo "未知")
        add_table_row "版本" "$docker_version"
        
        # 检测Docker服务状态
        if docker info 2>/dev/null; then
            add_table_row "服务状态" "✅ 运行中"
            
            # 获取详细信息
            local docker_info=$(docker info 2>/dev/null || true)
            local containers_running=$(echo "$docker_info" | grep "Running:" | head -1 | awk '{print $2}' || echo "0")
            local containers_paused=$(echo "$docker_info" | grep "Paused:" | head -1 | awk '{print $2}' || echo "0")
            local containers_stopped=$(echo "$docker_info" | grep "Stopped:" | head -1 | awk '{print $2}' || echo "0")
            local images=$(echo "$docker_info" | grep "Images:" | head -1 | awk '{print $2}' || echo "0")
            local server_version=$(echo "$docker_info" | grep "Server Version:" | head -1 | awk '{print $3}' || echo "未知")
            local storage_driver=$(echo "$docker_info" | grep "Storage Driver:" | head -1 | awk '{print $3}' || echo "未知")
            local cgroup_driver=$(echo "$docker_info" | grep "Cgroup Driver:" | head -1 | awk '{print $3}' || echo "未知")
            local docker_root_dir=$(echo "$docker_info" | grep "Docker Root Dir:" | head -1 | awk '{print $4}' || echo "未知")
            
            add_table_row "服务端版本" "$server_version"
            add_table_row "存储驱动" "$storage_driver"
            add_table_row "Cgroup驱动" "$cgroup_driver"
            add_table_row "数据目录" "$docker_root_dir"
            add_table_row "镜像数量" "$images"
            add_table_row "运行中容器" "$containers_running"
            add_table_row "暂停容器" "$containers_paused"
            add_table_row "停止容器" "$containers_stopped"
            
            append_to_file ""
            
            # 列出运行中的容器
            add_subsection "运行中的Docker容器"
            
            local running_containers=$(docker ps --format "table {{.Names}}\t{{.Image}}\t{{.Status}}\t{{.Ports}}" 2>/dev/null)
            if [ -n "$running_containers" ]; then
                append_to_file "\`\`\`"
                echo "$running_containers" >> "$OUTPUT_FILE"
                append_to_file "\`\`\`"
            else
                append_to_file "> 暂无运行中的容器\n"
            fi
            
            append_to_file ""
            
            # 列出所有容器
            add_subsection "所有Docker容器"
            
            local all_containers=$(docker ps -a --format "table {{.Names}}\t{{.Image}}\t{{.Status}}\t{{.Ports}}" 2>/dev/null)
            if [ -n "$all_containers" ]; then
                append_to_file "\`\`\`"
                echo "$all_containers" >> "$OUTPUT_FILE"
                append_to_file "\`\`\`"
            else
                append_to_file "> 暂无容器\n"
            fi
            
            append_to_file ""
            
            # 列出镜像
            add_subsection "Docker镜像列表"
            
            local images=$(docker images --format "table {{.Repository}}\t{{.Tag}}\t{{.Size}}\t{{.CreatedAt}}" 2>/dev/null)
            if [ -n "$images" ]; then
                append_to_file "\`\`\`"
                echo "$images" >> "$OUTPUT_FILE"
                append_to_file "\`\`\`"
            else
                append_to_file "> 暂无镜像\n"
            fi
        else
            add_table_row "服务状态" "❌ 未运行或无法连接"
        fi
    else
        append_to_file "> ⚠️  未检测到Docker\n"
    fi
    
    append_to_file ""
    
    add_subsection "Docker Compose"
    
    if command -v docker-compose &>/dev/null; then
        append_to_file "| 项目 | 值 |"
        append_to_file "|-----|-----|"
        
        local compose_version=$(docker-compose --version 2>/dev/null || echo "未知")
        add_table_row "版本" "$compose_version"
        append_to_file ""
    elif docker compose version 2>/dev/null; then
        append_to_file "| 项目 | 值 |"
        append_to_file "|-----|-----|"
        
        local compose_version=$(docker compose version 2>/dev/null || echo "未知")
        add_table_row "版本(Docker Plugin)" "$compose_version"
        append_to_file ""
    else
        append_to_file "> ⚠️  未检测到Docker Compose\n"
    fi
    
    append_to_file ""
    
    add_subsection "Kubernetes"
    
    # 检测kubectl
    if command -v kubectl &>/dev/null; then
        append_to_file "| 项目 | 值 |"
        append_to_file "|-----|-----|"
        
        local kubectl_version=$(kubectl version --client --short 2>/dev/null || kubectl version --client 2>/dev/null || echo "未知")
        add_table_row "kubectl客户端版本" "$kubectl_version"
        
        # 尝试连接集群
        if kubectl cluster-info 2>/dev/null; then
            add_table_row "集群连接状态" "✅ 已连接"
            
            append_to_file ""
            add_subsection "Kubernetes集群信息"
            
            append_to_file "\`\`\`"
            kubectl cluster-info 2>/dev/null >> "$OUTPUT_FILE" || echo "无法获取集群信息" >> "$OUTPUT_FILE"
            append_to_file "\`\`\`"
            
            append_to_file ""
            add_subsection "Kubernetes节点信息"
            
            append_to_file "\`\`\`"
            kubectl get nodes -o wide 2>/dev/null >> "$OUTPUT_FILE" || echo "无法获取节点信息" >> "$OUTPUT_FILE"
            append_to_file "\`\`\`"
            
            append_to_file ""
            add_subsection "Kubernetes Pod信息"
            
            append_to_file "\`\`\`"
            kubectl get pods -A -o wide 2>/dev/null >> "$OUTPUT_FILE" || echo "无法获取Pod信息" >> "$OUTPUT_FILE"
            append_to_file "\`\`\`"
        else
            add_table_row "集群连接状态" "❌ 未连接或无权限"
        fi
    else
        append_to_file "> ⚠️  未检测到kubectl\n"
    fi
    
    append_to_file ""
    
    # 检测其他容器运行时
    add_subsection "其他容器运行时检测"
    
    append_to_file "| 运行时 | 状态 | 版本信息 |"
    append_to_file "|-------|------|---------|"
    
    local container_runtimes=(
        "containerd:containerd"
        "cri-o:CRI-O"
        "podman:Podman"
        "runc:runc"
    )
    
    for runtime_info in "${container_runtimes[@]}"; do
        local runtime_cmd="${runtime_info%%:*}"
        local runtime_name="${runtime_info#*:}"
        
        if command -v "$runtime_cmd" &>/dev/null; then
            local version=$("$runtime_cmd" --version 2>/dev/null | head -1 || echo "检测到")
            append_to_file "| $runtime_name | ✅ 已安装 | $version |"
        fi
    done
    
    append_to_file ""
    print_success "容器组件信息采集完成"
}

# 4. 其他关键软件信息
collect_other_software_info() {
    add_section "其他关键软件信息"
    
    add_subsection "Web服务器"
    
    append_to_file "| 软件 | 状态 | 版本 | 配置路径 | 进程状态 |"
    append_to_file "|-----|------|-----|---------|---------|"
    
    local web_servers=(
        "nginx:Nginx:/etc/nginx/nginx.conf"
        "apache2:Apache2:/etc/apache2/apache2.conf"
        "httpd:HTTPD:/etc/httpd/conf/httpd.conf"
        "lighttpd:Lighttpd:/etc/lighttpd/lighttpd.conf"
        "caddy:Caddy:/etc/caddy/Caddyfile"
    )
    
    for server_info in "${web_servers[@]}"; do
        local cmd="${server_info%%:*}"
        local name="${server_info#*:}"
        name="${name%%:*}"
        local config_path="${server_info##*:}"
        
        if command -v "$cmd" &>/dev/null; then
            local version=$("$cmd" -v 2>/dev/null || "$cmd" -version 2>/dev/null || "$cmd" --version 2>/dev/null | head -1 || echo "未知")
            local proc_count=$(pgrep -c "$cmd" 2>/dev/null || echo "0")
            local proc_status="❌ 未运行"
            if [ "$proc_count" -gt 0 ]; then
                proc_status="✅ 运行中 ($proc_count个进程)"
            fi
            
            local config_exists="❌ 不存在"
            if [ -f "$config_path" ]; then
                config_exists="✅ $config_path"
            fi
            
            append_to_file "| $name | ✅ 已安装 | $version | $config_exists | $proc_status |"
        fi
    done
    
    append_to_file ""
    
    add_subsection "编程语言环境"
    
    append_to_file "| 语言 | 状态 | 版本 | 安装路径 |"
    append_to_file "|-----|------|-----|---------|"
    
    local languages=(
        "python3:Python 3"
        "python:Python"
        "java:Java"
        "javac:Java Compiler"
        "go:Go"
        "node:Node.js"
        "npm:npm"
        "php:PHP"
        "ruby:Ruby"
        "perl:Perl"
        "rustc:Rust"
        "swift:Swift"
        "kotlin:Kotlin"
        "scala:Scala"
    )
    
    for lang_info in "${languages[@]}"; do
        local cmd="${lang_info%%:*}"
        local name="${lang_info#*:}"
        
        if command -v "$cmd" &>/dev/null; then
            local version="未知"
            case "$cmd" in
                python3|python)
                    version=$("$cmd" --version 2>&1 | head -1)
                    ;;
                java)
                    version=$("$cmd" -version 2>&1 | head -1)
                    ;;
                javac)
                    version=$("$cmd" -version 2>&1)
                    ;;
                go)
                    version=$("$cmd" version 2>/dev/null | awk '{print $3}')
                    ;;
                node)
                    version=$("$cmd" -v 2>/dev/null)
                    ;;
                npm)
                    version=$("$cmd" -v 2>/dev/null)
                    ;;
                php)
                    version=$("$cmd" -v 2>/dev/null | head -1)
                    ;;
                ruby)
                    version=$("$cmd" -v 2>/dev/null)
                    ;;
                perl)
                    version=$("$cmd" -v 2>/dev/null | head -2 | tail -1)
                    ;;
                rustc)
                    version=$("$cmd" --version 2>/dev/null)
                    ;;
                swift)
                    version=$("$cmd" --version 2>/dev/null | head -1)
                    ;;
                kotlin)
                    version=$("$cmd" -version 2>/dev/null | head -1)
                    ;;
                scala)
                    version=$("$cmd" -version 2>/dev/null 2>&1 | head -1)
                    ;;
                *)
                    version=$("$cmd" --version 2>/dev/null || "$cmd" -version 2>/dev/null | head -1)
                    ;;
            esac
            
            local install_path=$(command -v "$cmd" 2>/dev/null || echo "未知")
            append_to_file "| $name | ✅ 已安装 | $version | $install_path |"
        fi
    done
    
    append_to_file ""
    
    add_subsection "开发工具与构建工具"
    
    append_to_file "| 工具 | 状态 | 版本 | 安装路径 |"
    append_to_file "|-----|------|-----|---------|"
    
    local dev_tools=(
        "gcc:GCC"
        "g++:G++"
        "make:Make"
        "cmake:CMake"
        "autoconf:Autoconf"
        "automake:Automake"
        "git:Git"
        "svn:Subversion"
        "maven:Maven"
        "gradle:Gradle"
        "ant:Ant"
        "yarn:Yarn"
        "pip3:pip3"
        "pip:pip"
        "cargo:Cargo"
    )
    
    for tool_info in "${dev_tools[@]}"; do
        local cmd="${tool_info%%:*}"
        local name="${tool_info#*:}"
        
        if command -v "$cmd" &>/dev/null; then
            local version="未知"
            case "$cmd" in
                gcc|g++)
                    version=$("$cmd" --version 2>/dev/null | head -1)
                    ;;
                make|cmake|autoconf|automake|git|svn|ant|yarn|cargo)
                    version=$("$cmd" --version 2>/dev/null | head -1)
                    ;;
                maven)
                    version=$("$cmd" -v 2>/dev/null | head -1)
                    ;;
                gradle)
                    version=$("$cmd" -v 2>/dev/null | grep "Gradle" | head -1)
                    ;;
                pip3|pip)
                    version=$("$cmd" --version 2>/dev/null | awk '{print $2}')
                    ;;
                *)
                    version=$("$cmd" --version 2>/dev/null || "$cmd" -version 2>/dev/null | head -1)
                    ;;
            esac
            
            local install_path=$(command -v "$cmd" 2>/dev/null || echo "未知")
            append_to_file "| $name | ✅ 已安装 | $version | $install_path |"
        fi
    done
    
    append_to_file ""
    
    add_subsection "已安装的包管理器检测"
    
    append_to_file "| 包管理器 | 状态 | 版本 |"
    append_to_file "|---------|------|-----|"
    
    local package_managers=(
        "apt:APT (Debian/Ubuntu)"
        "yum:YUM (RHEL/CentOS)"
        "dnf:DNF (Fedora/RHEL 8+)"
        "zypper:Zypper (openSUSE)"
        "pacman:Pacman (Arch Linux)"
        "snap:Snap"
        "flatpak:Flatpak"
        "apk:APK (Alpine)"
    )
    
    for pm_info in "${package_managers[@]}"; do
        local cmd="${pm_info%%:*}"
        local name="${pm_info#*:}"
        
        if command -v "$cmd" &>/dev/null; then
            local version="未知"
            case "$cmd" in
                apt)
                    version=$("$cmd" --version 2>/dev/null | head -1)
                    ;;
                yum|dnf)
                    version=$("$cmd" --version 2>/dev/null | head -1)
                    ;;
                zypper)
                    version=$("$cmd" --version 2>/dev/null | head -1)
                    ;;
                pacman)
                    version=$("$cmd" --version 2>/dev/null | head -1)
                    ;;
                snap)
                    version=$("$cmd" version 2>/dev/null | head -1)
                    ;;
                flatpak)
                    version=$("$cmd" --version 2>/dev/null)
                    ;;
                apk)
                    version=$("$cmd" --version 2>/dev/null)
                    ;;
                *)
                    version=$("$cmd" --version 2>/dev/null || "$cmd" -version 2>/dev/null | head -1)
                    ;;
            esac
            append_to_file "| $name | ✅ 已安装 | $version |"
        fi
    done
    
    append_to_file ""
    
    # 列出已安装的服务
    add_subsection "系统服务状态"
    
    if command -v systemctl &>/dev/null; then
        append_to_file "**运行中的服务:**\n"
        append_to_file "\`\`\`"
        systemctl list-units --type=service --state=running 2>/dev/null | head -50 >> "$OUTPUT_FILE" || echo "无法获取服务列表" >> "$OUTPUT_FILE"
        append_to_file "\`\`\`"
    elif command -v service &>/dev/null; then
        append_to_file "**服务状态:**\n"
        append_to_file "\`\`\`"
        service --status-all 2>/dev/null | head -50 >> "$OUTPUT_FILE" || echo "无法获取服务列表" >> "$OUTPUT_FILE"
        append_to_file "\`\`\`"
    fi
    
    append_to_file ""
    
    # 计划任务
    add_subsection "计划任务 (Cron)"
    
    append_to_file "**系统Cron任务:**\n"
    
    # 检查系统cron目录
    local cron_dirs=(
        "/etc/crontab"
        "/etc/cron.d/"
        "/etc/cron.daily/"
        "/etc/cron.hourly/"
        "/etc/cron.weekly/"
        "/etc/cron.monthly/"
    )
    
    append_to_file "\`\`\`"
    for cron_dir in "${cron_dirs[@]}"; do
        if [ -f "$cron_dir" ]; then
            echo "=== $cron_dir ===" >> "$OUTPUT_FILE"
            cat "$cron_dir" 2>/dev/null >> "$OUTPUT_FILE"
            echo "" >> "$OUTPUT_FILE"
        elif [ -d "$cron_dir" ]; then
            echo "=== $cron_dir (目录) ===" >> "$OUTPUT_FILE"
            ls -la "$cron_dir" 2>/dev/null >> "$OUTPUT_FILE"
            echo "" >> "$OUTPUT_FILE"
        fi
    done
    append_to_file "\`\`\`"
    
    # 检查用户crontab（需要root）
    if [ "$EUID" -eq 0 ]; then
        append_to_file "**用户Cron任务:**\n"
        append_to_file "\`\`\`"
        
        # 列出所有用户的crontab
        if [ -d /var/spool/cron/crontabs ]; then
            echo "=== 用户crontabs目录 ===" >> "$OUTPUT_FILE"
            ls -la /var/spool/cron/crontabs 2>/dev/null >> "$OUTPUT_FILE"
        fi
        append_to_file "\`\`\`"
    fi
    
    append_to_file ""
    print_success "其他关键软件信息采集完成"
}

# ==================== 系统运行状态检测模块 ====================

# 1. CPU使用率检测
collect_cpu_usage() {
    add_subsection "CPU使用率"
    
    append_to_file "| 指标 | 当前值 |"
    append_to_file "|------|-------|"
    
    # 使用top获取CPU使用率
    if is_tool_available "top"; then
        local cpu_usage=$(top -bn1 2>/dev/null | grep "Cpu(s)" | head -1)
        if [ -n "$cpu_usage" ]; then
            local user=$(echo "$cpu_usage" | awk '{print $2}' | cut -d'%' -f1)
            local system=$(echo "$cpu_usage" | awk '{print $4}' | cut -d'%' -f1)
            local nice=$(echo "$cpu_usage" | awk '{print $6}' | cut -d'%' -f1)
            local idle=$(echo "$cpu_usage" | awk '{print $8}' | cut -d'%' -f1)
            local wait=$(echo "$cpu_usage" | awk '{print $10}' | cut -d'%' -f1)
            local hi=$(echo "$cpu_usage" | awk '{print $12}' | cut -d'%' -f1)
            local si=$(echo "$cpu_usage" | awk '{print $14}' | cut -d'%' -f1)
            
            local total_used=$(echo "scale=2; 100 - $idle" | bc 2>/dev/null || echo "计算中")
            
            add_table_row "用户空间使用率" "${user}%"
            add_table_row "系统空间使用率" "${system}%"
            add_table_row "Nice进程使用率" "${nice}%"
            add_table_row "等待IO" "${wait}%"
            add_table_row "硬中断" "${hi}%"
            add_table_row "软中断" "${si}%"
            add_table_row "空闲率" "${idle}%"
            add_table_row "总使用率" "${total_used}%"
        fi
    fi
    
    # 使用vmstat获取更准确的CPU使用率
    if is_tool_available "vmstat"; then
        local vmstat_output=$(vmstat 1 2 2>/dev/null | tail -1)
        if [ -n "$vmstat_output" ]; then
            local us=$(echo "$vmstat_output" | awk '{print $13}')
            local sy=$(echo "$vmstat_output" | awk '{print $14}')
            local id=$(echo "$vmstat_output" | awk '{print $15}')
            local wa=$(echo "$vmstat_output" | awk '{print $16}')
            local st=$(echo "$vmstat_output" | awk '{print $17}')
            
            local total=$(echo "scale=2; $us + $sy" | bc 2>/dev/null || echo "计算中")
            
            append_to_file ""
            append_to_file "**vmstat采样 (1秒平均):**"
            append_to_file "| 指标 | 值 |"
            append_to_file "|------|----|"
            add_table_row "用户态(us)" "${us}%"
            add_table_row "系统态(sy)" "${sy}%"
            add_table_row "空闲(id)" "${id}%"
            add_table_row "等待IO(wa)" "${wa}%"
            add_table_row "被偷取(st)" "${st}%"
            add_table_row "总使用率" "${total}%"
        fi
    fi
    
    # 负载均衡信息
    add_subsection "系统负载"
    
    append_to_file "| 时间范围 | 负载值 |"
    append_to_file "|---------|-------|"
    
    if [ -f /proc/loadavg ]; then
        local load=$(cat /proc/loadavg 2>/dev/null)
        local load1=$(echo "$load" | awk '{print $1}')
        local load5=$(echo "$load" | awk '{print $2}')
        local load15=$(echo "$load" | awk '{print $3}')
        local running=$(echo "$load" | awk '{print $4}')
        
        add_table_row "1分钟" "$load1"
        add_table_row "5分钟" "$load5"
        add_table_row "15分钟" "$load15"
        add_table_row "运行/总进程" "$running"
    fi
    
    append_to_file ""
    
    # 每个CPU核心的使用情况
    add_subsection "各CPU核心使用情况"
    
    if [ -f /proc/stat ]; then
        append_to_file "\`\`\`"
        grep "^cpu" /proc/stat 2>/dev/null >> "$OUTPUT_FILE"
        append_to_file "\`\`\`"
    fi
    
    append_to_file ""
    print_success "CPU使用率采集完成"
}

# 2. 内存使用率检测
collect_memory_usage() {
    add_subsection "内存使用率"
    
    append_to_file "| 指标 | 值 | 使用率 |"
    append_to_file "|------|----|-------|"
    
    if is_tool_available "free"; then
        local mem_info=$(free -b 2>/dev/null)
        
        local total_mem=$(echo "$mem_info" | grep "Mem:" | awk '{print $2}')
        local used_mem=$(echo "$mem_info" | grep "Mem:" | awk '{print $3}')
        local free_mem=$(echo "$mem_info" | grep "Mem:" | awk '{print $4}')
        local shared_mem=$(echo "$mem_info" | grep "Mem:" | awk '{print $5}')
        local buffer_mem=$(echo "$mem_info" | grep "Mem:" | awk '{print $6}')
        local available_mem=$(echo "$mem_info" | grep "Mem:" | awk '{print $7}')
        
        # 转换为可读格式
        local total_human=$(numfmt --to=iec --suffix=B "$total_mem" 2>/dev/null || echo "${total_mem}B")
        local used_human=$(numfmt --to=iec --suffix=B "$used_mem" 2>/dev/null || echo "${used_mem}B")
        local free_human=$(numfmt --to=iec --suffix=B "$free_mem" 2>/dev/null || echo "${free_mem}B")
        local shared_human=$(numfmt --to=iec --suffix=B "$shared_mem" 2>/dev/null || echo "${shared_mem}B")
        local buffer_human=$(numfmt --to=iec --suffix=B "$buffer_mem" 2>/dev/null || echo "${buffer_mem}B")
        local available_human=$(numfmt --to=iec --suffix=B "$available_mem" 2>/dev/null || echo "${available_mem}B")
        
        # 计算使用率
        local used_percent=$(echo "scale=2; $used_mem * 100 / $total_mem" | bc 2>/dev/null || echo "0")
        local free_percent=$(echo "scale=2; $free_mem * 100 / $total_mem" | bc 2>/dev/null || echo "0")
        local available_percent=$(echo "scale=2; $available_mem * 100 / $total_mem" | bc 2>/dev/null || echo "0")
        
        add_table_row "总内存" "$total_human" "100%"
        add_table_row "已使用" "$used_human" "${used_percent}%"
        add_table_row "空闲" "$free_human" "${free_percent}%"
        add_table_row "共享内存" "$shared_human" "-"
        add_table_row "缓存" "$buffer_human" "-"
        add_table_row "可用内存" "$available_human" "${available_percent}%"
    fi
    
    append_to_file ""
    
    # 交换分区信息
    add_subsection "交换分区使用情况"
    
    append_to_file "| 指标 | 值 | 使用率 |"
    append_to_file "|------|----|-------|"
    
    if is_tool_available "free"; then
        local swap_info=$(free -b 2>/dev/null | grep "Swap:")
        
        if [ -n "$swap_info" ]; then
            local total_swap=$(echo "$swap_info" | awk '{print $2}')
            local used_swap=$(echo "$swap_info" | awk '{print $3}')
            local free_swap=$(echo "$swap_info" | awk '{print $4}')
            
            if [ "$total_swap" -gt 0 ]; then
                local total_human=$(numfmt --to=iec --suffix=B "$total_swap" 2>/dev/null || echo "${total_swap}B")
                local used_human=$(numfmt --to=iec --suffix=B "$used_swap" 2>/dev/null || echo "${used_swap}B")
                local free_human=$(numfmt --to=iec --suffix=B "$free_swap" 2>/dev/null || echo "${free_swap}B")
                
                local used_percent=$(echo "scale=2; $used_swap * 100 / $total_swap" | bc 2>/dev/null || echo "0")
                local free_percent=$(echo "scale=2; $free_swap * 100 / $total_swap" | bc 2>/dev/null || echo "0")
                
                add_table_row "总交换分区" "$total_human" "100%"
                add_table_row "已使用" "$used_human" "${used_percent}%"
                add_table_row "空闲" "$free_human" "${free_percent}%"
            else
                append_to_file "> 系统未配置交换分区\n"
            fi
        fi
    fi
    
    append_to_file ""
    
    # 详细内存信息
    add_subsection "详细内存信息 (/proc/meminfo)"
    
    if [ -f /proc/meminfo ]; then
        append_to_file "\`\`\`"
        head -30 /proc/meminfo 2>/dev/null >> "$OUTPUT_FILE"
        append_to_file "\`\`\`"
    fi
    
    append_to_file ""
    
    # 进程内存使用TOP10
    add_subsection "内存使用TOP 10进程"
    
    if command -v ps &>/dev/null; then
        append_to_file "| PID | 用户 | 进程名 | RSS内存 | %MEM | 命令 |"
        append_to_file "|-----|------|--------|--------|------|------|"
        
        ps aux --sort=-%mem 2>/dev/null | head -11 | tail -10 | while IFS= read -r line; do
            if [ -n "$line" ]; then
                local pid=$(echo "$line" | awk '{print $2}')
                local user=$(echo "$line" | awk '{print $1}')
                local rss=$(echo "$line" | awk '{print $6}')
                local mem=$(echo "$line" | awk '{print $4}')
                local cmd=$(echo "$line" | awk '{print $11}')
                local full_cmd=$(echo "$line" | cut -d' ' -f11- | xargs | cut -c1-50)
                
                local rss_human=$(numfmt --to=iec --suffix=B $((rss * 1024)) 2>/dev/null || echo "${rss}KB")
                
                append_to_file "| $pid | $user | $cmd | $rss_human | ${mem}% | $full_cmd |"
            fi
        done
    fi
    
    append_to_file ""
    print_success "内存使用率采集完成"
}

# 3. 磁盘使用率检测
collect_disk_usage() {
    add_subsection "磁盘使用率"
    
    append_to_file "| 文件系统 | 类型 | 总大小 | 已使用 | 可用 | 使用率 | 挂载点 |"
    append_to_file "|---------|------|--------|--------|------|-------|-------|"
    
    if is_tool_available "df"; then
        # 获取所有挂载的文件系统（排除tmpfs等）
        df -hT --exclude-type=tmpfs --exclude-type=devtmpfs --exclude-type=sysfs --exclude-type=proc 2>/dev/null | tail -n +2 | while IFS= read -r line; do
            if [ -n "$line" ]; then
                local fs=$(echo "$line" | awk '{print $1}')
                local fstype=$(echo "$line" | awk '{print $2}')
                local size=$(echo "$line" | awk '{print $3}')
                local used=$(echo "$line" | awk '{print $4}')
                local avail=$(echo "$line" | awk '{print $5}')
                local pcent=$(echo "$line" | awk '{print $6}')
                local mount=$(echo "$line" | awk '{print $7}')
                
                # 标记高使用率
                local pcent_num=$(echo "$pcent" | tr -d '%')
                if [ "$pcent_num" -ge 90 ]; then
                    pcent="⚠️ ${pcent}"
                elif [ "$pcent_num" -ge 80 ]; then
                    pcent="🔶 ${pcent}"
                fi
                
                append_to_file "| $fs | $fstype | $size | $used | $avail | $pcent | $mount |"
            fi
        done
    fi
    
    append_to_file ""
    
    # inode使用情况
    add_subsection "Inode使用率"
    
    append_to_file "| 文件系统 | 总Inode | 已使用 | 可用 | 使用率 | 挂载点 |"
    append_to_file "|---------|---------|--------|------|-------|-------|"
    
    if is_tool_available "df"; then
        df -hi --exclude-type=tmpfs --exclude-type=devtmpfs 2>/dev/null | tail -n +2 | while IFS= read -r line; do
            if [ -n "$line" ]; then
                local fs=$(echo "$line" | awk '{print $1}')
                local total=$(echo "$line" | awk '{print $2}')
                local used=$(echo "$line" | awk '{print $3}')
                local avail=$(echo "$line" | awk '{print $4}')
                local pcent=$(echo "$line" | awk '{print $5}')
                local mount=$(echo "$line" | awk '{print $6}')
                
                append_to_file "| $fs | $total | $used | $avail | $pcent | $mount |"
            fi
        done
    fi
    
    append_to_file ""
    
    # 目录使用情况
    add_subsection "大目录检测 (/) "
    
    if command -v du &>/dev/null && [ "$EUID" -eq 0 ]; then
        append_to_file "\`\`\`"
        du -sh /* 2>/dev/null | sort -hr | head -20 >> "$OUTPUT_FILE" || echo "无法获取目录大小" >> "$OUTPUT_FILE"
        append_to_file "\`\`\`"
    else
        append_to_file "> ⚠️  需要root权限才能检测目录大小\n"
    fi
    
    append_to_file ""
    
    # 磁盘I/O状态
    add_subsection "磁盘I/O状态"
    
    if command -v iostat &>/dev/null; then
        append_to_file "\`\`\`"
        iostat -x 1 2 2>/dev/null >> "$OUTPUT_FILE" || echo "iostat不可用" >> "$OUTPUT_FILE"
        append_to_file "\`\`\`"
    else
        append_to_file "> ⚠️  iostat不可用（需要安装sysstat包）\n"
    fi
    
    append_to_file ""
    print_success "磁盘使用率采集完成"
}

# 4. 网卡速率检测
collect_network_speed() {
    add_subsection "网卡速率与流量"
    
    append_to_file "| 接口 | 状态 | 速率 | 双工 | 接收字节 | 发送字节 | 接收包 | 发送包 | 错误 | 丢弃 |"
    append_to_file "|------|------|------|------|---------|---------|--------|--------|------|------|"
    
    if is_tool_available "ip"; then
        local interfaces=$(ip -br link show 2>/dev/null | awk '{print $1}')
        
        while IFS= read -r iface; do
            if [ -n "$iface" ]; then
                # 获取接口状态
                local state=$(ip -br link show "$iface" 2>/dev/null | awk '{print $2}')
                local state_icon="❌"
                if [ "$state" = "UP" ] || [ "$state" = "UNKNOWN" ]; then
                    state_icon="✅"
                fi
                
                # 使用ethtool获取速率
                local speed="未知"
                local duplex="未知"
                if is_tool_available "ethtool"; then
                    local ethtool_out=$(ethtool "$iface" 2>/dev/null || true)
                    speed=$(echo "$ethtool_out" | grep "Speed:" | cut -d: -f2 | xargs || echo "未知")
                    duplex=$(echo "$ethtool_out" | grep "Duplex:" | cut -d: -f2 | xargs || echo "未知")
                fi
                
                # 获取统计信息
                local link_info=$(ip -s link show "$iface" 2>/dev/null)
                local rx_bytes=$(echo "$link_info" | grep -A 1 "RX:" | tail -1 | awk '{print $1}' || echo "0")
                local tx_bytes=$(echo "$link_info" | grep -A 1 "TX:" | tail -1 | awk '{print $1}' || echo "0")
                local rx_packets=$(echo "$link_info" | grep "RX:" | head -1 | awk '{print $3}' || echo "0")
                local tx_packets=$(echo "$link_info" | grep "TX:" | head -1 | awk '{print $3}' || echo "0")
                local rx_errors=$(echo "$link_info" | grep "RX:" | head -1 | awk '{print $4}' || echo "0")
                local tx_errors=$(echo "$link_info" | grep "TX:" | head -1 | awk '{print $4}' || echo "0")
                local rx_dropped=$(echo "$link_info" | grep "RX:" | head -1 | awk '{print $5}' || echo "0")
                local tx_dropped=$(echo "$link_info" | grep "TX:" | head -1 | awk '{print $5}' || echo "0")
                
                # 转换为可读格式
                local rx_human=$(numfmt --to=iec --suffix=B "$rx_bytes" 2>/dev/null || echo "${rx_bytes}B")
                local tx_human=$(numfmt --to=iec --suffix=B "$tx_bytes" 2>/dev/null || echo "${tx_bytes}B")
                
                # 标记错误
                local has_error=""
                if [ "$rx_errors" -gt 0 ] || [ "$tx_errors" -gt 0 ] || [ "$rx_dropped" -gt 0 ] || [ "$tx_dropped" -gt 0 ]; then
                    has_error="⚠️"
                fi
                
                append_to_file "| $iface $state_icon | $state | $speed | $duplex | $rx_human | $tx_human | $rx_packets | $tx_packets | $rx_errors/$tx_errors $has_error | $rx_dropped/$tx_dropped $has_error |"
            fi
        done < <(echo "$interfaces")
    fi
    
    append_to_file ""
    
    # 网络连接状态
    add_subsection "网络连接状态"
    
    if is_tool_available "ss"; then
        append_to_file "**TCP连接统计:**\n"
        append_to_file "| 状态 | 数量 |"
        append_to_file "|------|------|"
        
        local states=("ESTAB" "LISTEN" "TIME-WAIT" "CLOSE-WAIT" "FIN-WAIT-1" "FIN-WAIT-2" "SYN-SENT" "SYN-RECV" "LAST-ACK" "CLOSING")
        
        for state in "${states[@]}"; do
            local count=$(ss -t state "$state" 2>/dev/null | wc -l)
            if [ "$count" -gt 1 ]; then
                count=$((count - 1))
                append_to_file "| $state | $count |"
            fi
        done
        
        append_to_file ""
        
        # 监听端口
        add_subsection "监听端口"
        
        append_to_file "| 协议 | 端口 | 地址 | 进程 |"
        append_to_file "|------|------|------|------|"
        
        ss -tuln 2>/dev/null | tail -n +2 | while IFS= read -r line; do
            if [ -n "$line" ]; then
                local proto=$(echo "$line" | awk '{print $1}')
                local addr=$(echo "$line" | awk '{print $5}')
                local port=$(echo "$addr" | rev | cut -d: -f1 | rev)
                
                append_to_file "| $proto | $port | $addr | - |"
            fi
        done
    elif is_tool_available "netstat"; then
        append_to_file "**网络连接统计 (netstat):**\n"
        append_to_file "\`\`\`"
        netstat -tuln 2>/dev/null >> "$OUTPUT_FILE" || echo "netstat执行失败" >> "$OUTPUT_FILE"
        append_to_file "\`\`\`"
    else
        append_to_file "> ⚠️  ss和netstat都不可用\n"
    fi
    
    append_to_file ""
    
    # 防火墙状态
    add_subsection "防火墙状态"
    
    if command -v ufw &>/dev/null; then
        append_to_file "**UFW防火墙状态:**\n"
        append_to_file "\`\`\`"
        ufw status verbose 2>/dev/null >> "$OUTPUT_FILE" || echo "ufw状态获取失败" >> "$OUTPUT_FILE"
        append_to_file "\`\`\`"
    elif command -v iptables &>/dev/null; then
        append_to_file "**iptables规则:**\n"
        append_to_file "\`\`\`"
        iptables -L -n -v 2>/dev/null >> "$OUTPUT_FILE" || echo "iptables规则获取失败" >> "$OUTPUT_FILE"
        append_to_file "\`\`\`"
    else
        append_to_file "> ⚠️  未检测到UFW或iptables\n"
    fi
    
    append_to_file ""
    print_success "网卡速率采集完成"
}

# 系统运行状态总入口
collect_system_status() {
    add_section "系统运行状态"
    
    collect_cpu_usage
    collect_memory_usage
    collect_disk_usage
    collect_network_speed
}

# ==================== 汇总报告模块 ====================

# 生成跳过检测汇总报告
generate_skip_report() {
    if [ ${#SKIPPED_SECTIONS[@]} -gt 0 ]; then
        add_section "跳过的检测项目汇总"
        
        append_to_file "| 检测项目 | 跳过原因 |"
        append_to_file "|---------|---------|"
        
        for section in "${!SKIPPED_SECTIONS[@]}"; do
            add_table_row "$section" "${SKIPPED_SECTIONS[$section]}"
        done
        
        append_to_file ""
    fi
}

# 生成检测摘要
generate_summary() {
    add_section "检测摘要"
    
    append_to_file "## 执行时间线\n"
    
    append_to_file "| 时间 | 事件 |"
    append_to_file "|------|------|"
    add_table_row "$(date +"%Y-%m-%d %H:%M:%S")" "脚本开始执行"
    add_table_row "$(date +"%Y-%m-%d %H:%M:%S")" "工具检测完成"
    add_table_row "$(date +"%Y-%m-%d %H:%M:%S")" "信息采集完成"
    add_table_row "$(date +"%Y-%m-%d %H:%M:%S")" "报告生成完成"
    
    append_to_file ""
    
    # 权限信息
    add_subsection "执行权限信息"
    
    append_to_file "| 项目 | 状态 |"
    append_to_file "|------|------|"
    
    if [ "$EUID" -eq 0 ]; then
        add_table_row "执行用户" "root (UID: $EUID)"
        add_table_row "权限级别" "✅ 超级用户"
        add_table_row "完整信息采集" "✅ 已启用"
    else
        add_table_row "执行用户" "普通用户 (UID: $EUID)"
        add_table_row "权限级别" "⚠️  普通用户"
        add_table_row "完整信息采集" "❌ 部分功能受限"
    fi
    
    append_to_file ""
    
    # 输出文件信息
    add_subsection "输出文件信息"
    
    append_to_file "| 项目 | 值 |"
    append_to_file "|------|----|"
    add_table_row "文件名" "$(basename "$OUTPUT_FILE")"
    add_table_row "完整路径" "$OUTPUT_FILE"
    add_table_row "文件大小" "$(stat -c%s "$OUTPUT_FILE" 2>/dev/null || echo "未知") 字节"
    
    append_to_file ""
    
    # 注意事项
    add_subsection "注意事项"
    
    append_to_file "- 为获得最完整的设备信息，建议使用 **root 权限** 运行此脚本"
    append_to_file "- 某些硬件信息需要 `dmidecode` 工具支持，请确保已安装"
    append_to_file "- 数据库连接检测需要相应的客户端工具和访问权限"
    append_to_file "- Docker容器信息需要Docker服务运行且当前用户有访问权限"
    append_to_file "- 大目录检测需要root权限以遍历所有目录"
    
    append_to_file ""
}

# ==================== 主函数 ====================

main() {
    print_section "Linux 系统设备信息采集脚本"
    
    # 1. 初始化输出文件
    init_output_file
    
    # 2. 检测必要工具
    check_tools
    
    # 3. 采集操作系统基础信息
    print_section "采集操作系统信息"
    collect_os_info
    
    # 4. 采集硬件信息
    print_section "采集硬件信息"
    collect_motherboard_info
    collect_cpu_info
    collect_memory_info
    collect_disk_info
    collect_network_info
    collect_other_hardware_info
    
    # 5. 采集软件信息
    print_section "采集软件信息"
    collect_database_info
    collect_container_info
    collect_other_software_info
    
    # 6. 采集系统运行状态
    print_section "采集系统运行状态"
    collect_system_status
    
    # 7. 生成汇总报告
    print_section "生成汇总报告"
    generate_skip_report
    generate_summary
    
    # 8. 完成
    print_section "采集完成"
    print_success "设备信息采集完成！"
    print_info "输出文件: $OUTPUT_FILE"
    
    # 显示文件大小
    if [ -f "$OUTPUT_FILE" ]; then
        local file_size=$(stat -c%s "$OUTPUT_FILE" 2>/dev/null)
        local file_size_human=$(numfmt --to=iec --suffix=B "$file_size" 2>/dev/null || echo "${file_size}B")
        print_info "文件大小: $file_size_human"
    fi
    
    # 检查是否有跳过的检测
    if [ ${#SKIPPED_SECTIONS[@]} -gt 0 ]; then
        print_warning "有 ${#SKIPPED_SECTIONS[@]} 项检测被跳过，请查看报告中的跳过项目汇总"
        print_warning "建议使用root权限运行以获取最完整的信息"
    fi
}

# 运行主函数
main "$@"