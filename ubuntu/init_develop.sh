#!/bin/bash

# Ubuntu 24.04 开发环境初始化脚本
# 功能：配置apt源、安装开发工具、配置环境等

set -e

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
PURPLE='\033[0;35m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

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

# 检查是否为root用户
check_root() {
    if [ "$EUID" -ne 0 ]; then
        print_error "请使用root权限运行此脚本"
        exit 1
    fi
}

# 1. 替换apt源为清华源
replace_apt_sources() {
    print_section "替换apt源为清华源"
    
    # 备份原文件
    if [ -f /etc/apt/sources.list ]; then
        cp /etc/apt/sources.list /etc/apt/sources.list.bak
        print_info "已备份原apt源文件到 /etc/apt/sources.list.bak"
    fi
    
    # 获取Ubuntu版本代号
    UBUNTU_VERSION=$(lsb_release -cs 2>/dev/null || echo "noble")
    
    # 清华源地址
    TSINGHUA_MIRROR="https://mirrors.tuna.tsinghua.edu.cn/ubuntu/"
    
    # 写入新的sources.list
    cat > /etc/apt/sources.list << EOF
# 默认注释了源码镜像以提高 apt update 速度，如有需要可自行取消注释
deb ${TSINGHUA_MIRROR} ${UBUNTU_VERSION} main restricted universe multiverse
# deb-src ${TSINGHUA_MIRROR} ${UBUNTU_VERSION} main restricted universe multiverse
deb ${TSINGHUA_MIRROR} ${UBUNTU_VERSION}-updates main restricted universe multiverse
# deb-src ${TSINGHUA_MIRROR} ${UBUNTU_VERSION}-updates main restricted universe multiverse
deb ${TSINGHUA_MIRROR} ${UBUNTU_VERSION}-backports main restricted universe multiverse
# deb-src ${TSINGHUA_MIRROR} ${UBUNTU_VERSION}-backports main restricted universe multiverse
deb ${TSINGHUA_MIRROR} ${UBUNTU_VERSION}-security main restricted universe multiverse
# deb-src ${TSINGHUA_MIRROR} ${UBUNTU_VERSION}-security main restricted universe multiverse
EOF
    
    print_success "apt源已替换为清华源"
    
    # 更新apt缓存
    print_info "更新apt缓存..."
    apt update -y
    print_success "apt缓存更新完成"
}

# 2. 安装开发环境常用组件
install_development_tools() {
    print_section "安装开发环境常用组件"
    
    DEV_TOOLS=(
        "build-essential"
        "gcc"
        "g++"
        "make"
        "cmake"
        "git"
        "curl"
        "wget"
        "software-properties-common"
        "apt-transport-https"
        "ca-certificates"
        "gnupg"
        "lsb-release"
    )
    
    print_info "安装以下开发工具：${DEV_TOOLS[*]}"
    
    for tool in "${DEV_TOOLS[@]}"; do
        if ! dpkg -l | grep -q "^ii  $tool"; then
            print_info "正在安装 $tool..."
            apt install -y "$tool"
        else
            print_warning "$tool 已安装，跳过"
        fi
    done
    
    print_success "开发工具安装完成"
}

# 3. 安装常用软件
install_common_software() {
    print_section "安装常用软件"
    
    COMMON_SOFTWARE=(
        "lrzsz"
        "vim"
        "net-tools"
        "unzip"
        "zip"
        "tar"
        "gzip"
        "htop"
        "tree"
        "telnet"
        "iputils-ping"
        "dnsutils"
        "rsync"
        "screen"
        "tmux"
    )
    
    print_info "安装以下常用软件：${COMMON_SOFTWARE[*]}"
    
    for software in "${COMMON_SOFTWARE[@]}"; do
        if ! dpkg -l | grep -q "^ii  $software"; then
            print_info "正在安装 $software..."
            apt install -y "$software"
        else
            print_warning "$software 已安装，跳过"
        fi
    done
    
    print_success "常用软件安装完成"
}

# 4. 安装Go环境
install_go() {
    print_section "安装Go环境"
    
    # 首先尝试从已知路径查找Go（解决多次执行时环境变量未生效的问题）
    GO_INSTALL_PATH="/usr/local/go/bin/go"
    
    # 如果Go二进制文件存在但不在PATH中，先添加到当前PATH
    if [ -f "$GO_INSTALL_PATH" ] && ! command -v go &> /dev/null; then
        export PATH=$PATH:/usr/local/go/bin
        print_info "已将Go路径添加到当前脚本的PATH中"
    fi
    
    # 检查Go是否已安装
    if command -v go &> /dev/null; then
        GO_VERSION=$(go version 2>/dev/null | awk '{print $3}')
        print_warning "Go $GO_VERSION 已安装，跳过安装"
        
        # 确保环境变量配置存在
        if ! grep -q "export PATH=\$PATH:/usr/local/go/bin" /etc/profile; then
            echo 'export PATH=$PATH:/usr/local/go/bin' >> /etc/profile
            echo 'export GOPATH=$HOME/go' >> /etc/profile
            echo 'export PATH=$PATH:$GOPATH/bin' >> /etc/profile
            print_info "已补全Go环境变量配置"
        fi
        
        # 确保Go国内代理配置存在
        if [ ! -f /etc/profile.d/go.sh ]; then
            cat > /etc/profile.d/go.sh << EOF
export GOPROXY=https://goproxy.cn,direct
export GOSUMDB=sum.golang.google.cn
EOF
            chmod +x /etc/profile.d/go.sh
            print_info "已配置Go国内代理"
        fi
        
        return
    fi
    
    # 下载并安装Go
    GO_VERSION="1.22.2"
    GO_TAR="go${GO_VERSION}.linux-amd64.tar.gz"
    
    # 定义多个下载源（国内镜像优先）
    GO_MIRRORS=(
        "https://mirrors.aliyun.com/golang/${GO_TAR}"
        "https://go.dev/dl/${GO_TAR}"
    )
    
    print_info "正在下载 Go $GO_VERSION..."
    
    # 尝试从多个镜像源下载
    DOWNLOAD_SUCCESS=false
    for mirror in "${GO_MIRRORS[@]}"; do
        print_info "尝试从 $mirror 下载..."
        if wget --timeout=30 --tries=2 "$mirror" -O "/tmp/${GO_TAR}" 2>/dev/null; then
            # 验证下载的文件是否有效（检查文件大小）
            if [ -f "/tmp/${GO_TAR}" ] && [ $(stat -c%s "/tmp/${GO_TAR}") -gt 10000000 ]; then
                DOWNLOAD_SUCCESS=true
                print_success "成功从 $mirror 下载 Go"
                break
            else
                print_warning "下载的文件无效，尝试下一个镜像源..."
                rm -f "/tmp/${GO_TAR}"
            fi
        else
            print_warning "从 $mirror 下载失败，尝试下一个镜像源..."
        fi
    done
    
    if [ "$DOWNLOAD_SUCCESS" = false ]; then
        print_error "所有镜像源都下载失败，Go安装失败"
        return 1
    fi
    
    print_info "正在安装 Go..."
    rm -rf /usr/local/go
    if ! tar -C /usr/local -xzf "/tmp/${GO_TAR}"; then
        print_error "解压Go失败，可能是下载的文件损坏"
        rm -f "/tmp/${GO_TAR}"
        return 1
    fi
    
    # 配置环境变量
    if ! grep -q "export PATH=\$PATH:/usr/local/go/bin" /etc/profile; then
        echo 'export PATH=$PATH:/usr/local/go/bin' >> /etc/profile
        echo 'export GOPATH=$HOME/go' >> /etc/profile
        echo 'export PATH=$PATH:$GOPATH/bin' >> /etc/profile
        print_info "已将Go添加到系统环境变量"
    fi
    
    # 配置Go国内代理
    if [ ! -f /etc/profile.d/go.sh ]; then
        cat > /etc/profile.d/go.sh << EOF
export GOPROXY=https://goproxy.cn,direct
export GOSUMDB=sum.golang.google.cn
EOF
        chmod +x /etc/profile.d/go.sh
        print_info "已配置Go国内代理"
    fi
    
    # 清理临时文件
    rm -f "/tmp/${GO_TAR}"
    
    # 验证安装
    export PATH=$PATH:/usr/local/go/bin
    if command -v go &> /dev/null; then
        INSTALLED_VERSION=$(go version | awk '{print $3}')
        print_success "Go $INSTALLED_VERSION 安装成功"
        
        # 显示更多信息
        go env GOPROXY GOSUMDB 2>/dev/null | while read -r line; do
            print_info "  $line"
        done
    else
        print_error "Go 安装失败"
        return 1
    fi
}

# 5. 安装Node.js环境
install_nodejs() {
    print_section "安装Node.js环境"
    
    # 检查Node.js是否已安装
    if command -v node &> /dev/null; then
        NODE_VERSION=$(node -v 2>/dev/null)
        print_warning "Node.js $NODE_VERSION 已安装，跳过安装"
        return
    fi
    
    # 使用NodeSource安装Node.js 20.x LTS
    print_info "正在添加NodeSource仓库..."
    curl -fsSL https://deb.nodesource.com/setup_20.x | bash -
    
    print_info "正在安装Node.js..."
    apt install -y nodejs
    
    # 验证安装
    if command -v node &> /dev/null; then
        NODE_VERSION=$(node -v)
        NPM_VERSION=$(npm -v)
        print_success "Node.js $NODE_VERSION 和 npm $NPM_VERSION 安装成功"
    else
        print_error "Node.js 安装失败"
    fi
}

# 6. 安装Python虚拟环境依赖
install_python_venv() {
    print_section "安装Python虚拟环境依赖"
    
    PYTHON_PACKAGES=(
        "python3"
        "python3-pip"
        "python3-venv"
        "python3-dev"
        "python3-setuptools"
        "python3-wheel"
    )
    
    print_info "安装以下Python包：${PYTHON_PACKAGES[*]}"
    
    for package in "${PYTHON_PACKAGES[@]}"; do
        if ! dpkg -l | grep -q "^ii  $package"; then
            print_info "正在安装 $package..."
            apt install -y "$package"
        else
            print_warning "$package 已安装，跳过"
        fi
    done
    
    # 升级pip
    print_info "升级pip..."
    # Ubuntu 24.04遵循PEP 668规范，需要使用--break-system-packages参数
    # 或者可以删除/usr/lib/python3.x/EXTERNALLY-MANAGED文件
    # 这里使用--break-system-packages参数更安全
    if python3 -m pip install --upgrade pip -i https://pypi.tuna.tsinghua.edu.cn/simple --break-system-packages 2>/dev/null; then
        print_success "pip升级成功"
    else
        # 如果升级失败，尝试删除EXTERNALLY-MANAGED文件后再升级
        print_warning "pip升级失败，尝试绕过PEP 668限制..."
        
        # 查找EXTERNALLY-MANAGED文件
        EXTERNALLY_MANAGED_FILE=$(find /usr/lib/python3* -name "EXTERNALLY-MANAGED" 2>/dev/null | head -1)
        
        if [ -n "$EXTERNALLY_MANAGED_FILE" ]; then
            print_info "找到EXTERNALLY-MANAGED文件: $EXTERNALLY_MANAGED_FILE"
            # 备份并删除该文件
            if [ -f "${EXTERNALLY_MANAGED_FILE}.bak" ]; then
                print_warning "备份文件已存在，跳过备份"
            else
                cp "$EXTERNALLY_MANAGED_FILE" "${EXTERNALLY_MANAGED_FILE}.bak"
                print_info "已备份EXTERNALLY-MANAGED文件到 ${EXTERNALLY_MANAGED_FILE}.bak"
            fi
            rm -f "$EXTERNALLY_MANAGED_FILE"
            print_info "已删除EXTERNALLY-MANAGED文件"
            
            # 再次尝试升级pip
            if python3 -m pip install --upgrade pip -i https://pypi.tuna.tsinghua.edu.cn/simple 2>/dev/null; then
                print_success "pip升级成功"
            else
                print_warning "pip升级失败，将使用系统默认版本"
            fi
        else
            print_warning "未找到EXTERNALLY-MANAGED文件，pip升级失败，将使用系统默认版本"
        fi
    fi
    
    # 配置pip源为清华源
    print_info "配置pip源为清华源..."
    mkdir -p ~/.pip
    cat > ~/.pip/pip.conf << EOF
[global]
index-url = https://pypi.tuna.tsinghua.edu.cn/simple
[install]
trusted-host = pypi.tuna.tsinghua.edu.cn
EOF
    
    # 验证安装
    PYTHON_VERSION=$(python3 --version 2>/dev/null || echo "未知")
    PIP_VERSION=$(pip3 --version 2>/dev/null | awk '{print $2}' || echo "未知")
    print_success "Python $PYTHON_VERSION 和 pip $PIP_VERSION 安装成功"
}

# 7. 安装Docker
install_docker() {
    print_section "安装Docker"
    
    # 检查Docker是否已安装
    if command -v docker &> /dev/null; then
        DOCKER_VERSION=$(docker --version 2>/dev/null | awk '{print $3}' | sed 's/,//')
        print_warning "Docker $DOCKER_VERSION 已安装，跳过安装"
        return
    fi
    
    # 卸载旧版本
    print_info "卸载旧版本Docker..."
    for pkg in docker.io docker-doc docker-compose podman-docker containerd runc; do
        apt-get remove -y $pkg 2>/dev/null || true
    done
    
    # 添加Docker官方GPG密钥
    print_info "添加Docker官方GPG密钥..."
    install -m 0755 -d /etc/apt/keyrings
    curl -fsSL https://download.docker.com/linux/ubuntu/gpg | gpg --dearmor -o /etc/apt/keyrings/docker.gpg
    chmod a+r /etc/apt/keyrings/docker.gpg
    
    # 设置Docker仓库
    print_info "设置Docker仓库..."
    echo \
      "deb [arch=$(dpkg --print-architecture) signed-by=/etc/apt/keyrings/docker.gpg] https://download.docker.com/linux/ubuntu \
      $(. /etc/os-release && echo "$VERSION_CODENAME") stable" | \
      tee /etc/apt/sources.list.d/docker.list > /dev/null
    
    # 更新apt缓存
    apt update -y
    
    # 安装Docker组件
    DOCKER_PACKAGES=(
        "docker-ce"
        "docker-ce-cli"
        "containerd.io"
        "docker-buildx-plugin"
        "docker-compose-plugin"
        "docker-compose"
    )
    
    print_info "安装以下Docker组件：${DOCKER_PACKAGES[*]}"
    apt install -y "${DOCKER_PACKAGES[@]}"
    
    # 启动Docker服务
    print_info "启动Docker服务..."
    systemctl enable docker
    systemctl start docker
    
    # 验证安装
    if command -v docker &> /dev/null; then
        DOCKER_VERSION=$(docker --version | awk '{print $3}' | sed 's/,//')
        DOCKER_COMPOSE_VERSION=$(docker compose version 2>/dev/null || echo "未知")
        print_success "Docker $DOCKER_VERSION 安装成功"
        print_success "Docker Compose: $DOCKER_COMPOSE_VERSION"
    else
        print_error "Docker 安装失败"
    fi
}

# 8. 扩充磁盘剩余空间到根目录
expand_disk_space() {
    print_section "扩充磁盘剩余空间到根目录"
    
    # 检查是否在容器中运行
    if [ -f /.dockerenv ]; then
        print_warning "检测到在Docker容器中运行，跳过磁盘扩充操作"
        return
    fi
    
    # 检查是否有可用的磁盘空间
    print_info "检查磁盘空间..."
    
    # 显示当前磁盘使用情况
    df -h /
    
    # 尝试扩展LVM逻辑卷（如果使用LVM）
    if command -v lvs &> /dev/null && command -v vgs &> /dev/null; then
        print_info "检测到LVM，尝试扩展根分区..."
        
        # 获取根分区的逻辑卷路径
        ROOT_LV=$(df / | tail -1 | awk '{print $1}')
        
        # 如果是LVM逻辑卷
        if [[ "$ROOT_LV" == /dev/mapper/* ]]; then
            # 获取卷组名称
            VG_NAME=$(vgs --noheadings -o vg_name 2>/dev/null | head -1 | xargs)
            
            if [ -n "$VG_NAME" ]; then
                # 检查卷组是否有空闲空间
                VG_FREE=$(vgs --noheadings -o vg_free 2>/dev/null | head -1 | xargs)
                
                if [ "$VG_FREE" != "0" ] && [ -n "$VG_FREE" ]; then
                    print_info "卷组 $VG_NAME 有 $VG_FREE 空闲空间"
                    
                    # 扩展逻辑卷
                    print_info "扩展逻辑卷..."
                    lvextend -l +100%FREE "$ROOT_LV" 2>/dev/null || true
                    
                    # 调整文件系统大小
                    print_info "调整文件系统大小..."
                    resize2fs "$ROOT_LV" 2>/dev/null || xfs_growfs / 2>/dev/null || true
                    
                    print_success "磁盘空间扩展完成"
                else
                    print_warning "卷组没有空闲空间可扩展"
                fi
            fi
        fi
    else
        print_warning "未检测到LVM，磁盘扩展需要手动操作"
    fi
    
    # 显示扩展后的磁盘使用情况
    print_info "当前磁盘使用情况："
    df -h /
}

# 9. 采集设备基础信息并打印
collect_system_info() {
    print_section "设备基础信息"
    
    echo -e "${PURPLE}========================================${NC}"
    echo -e "${PURPLE}        系统信息报告${NC}"
    echo -e "${PURPLE}========================================${NC}"
    echo ""
    
    # 系统信息
    echo -e "${CYAN}【系统信息】${NC}"
    if [ -f /etc/os-release ]; then
        . /etc/os-release
        echo -e "  操作系统: ${GREEN}$PRETTY_NAME${NC}"
    fi
    echo -e "  内核版本: ${GREEN}$(uname -r)${NC}"
    echo -e "  架构: ${GREEN}$(uname -m)${NC}"
    echo -e "  主机名: ${GREEN}$(hostname)${NC}"
    echo ""
    
    # CPU信息
    echo -e "${CYAN}【CPU信息】${NC}"
    CPU_MODEL=$(lscpu 2>/dev/null | grep "Model name" | cut -d: -f2 | xargs || echo "未知")
    CPU_CORES=$(nproc 2>/dev/null || echo "未知")
    echo -e "  型号: ${GREEN}$CPU_MODEL${NC}"
    echo -e "  核心数: ${GREEN}$CPU_CORES${NC}"
    echo ""
    
    # 内存信息
    echo -e "${CYAN}【内存信息】${NC}"
    if command -v free &> /dev/null; then
        MEM_INFO=$(free -h 2>/dev/null)
        TOTAL_MEM=$(echo "$MEM_INFO" | grep "Mem:" | awk '{print $2}')
        USED_MEM=$(echo "$MEM_INFO" | grep "Mem:" | awk '{print $3}')
        FREE_MEM=$(echo "$MEM_INFO" | grep "Mem:" | awk '{print $4}')
        echo -e "  总内存: ${GREEN}$TOTAL_MEM${NC}"
        echo -e "  已使用: ${GREEN}$USED_MEM${NC}"
        echo -e "  空闲: ${GREEN}$FREE_MEM${NC}"
    fi
    echo ""
    
    # 磁盘信息
    echo -e "${CYAN}【磁盘信息】${NC}"
    if command -v df &> /dev/null; then
        df -h / 2>/dev/null | tail -1 | while read -r line; do
            FS=$(echo "$line" | awk '{print $1}')
            SIZE=$(echo "$line" | awk '{print $2}')
            USED=$(echo "$line" | awk '{print $3}')
            AVAIL=$(echo "$line" | awk '{print $4}')
            USE_PERCENT=$(echo "$line" | awk '{print $5}')
            MOUNT=$(echo "$line" | awk '{print $6}')
            
            echo -e "  文件系统: ${GREEN}$FS${NC}"
            echo -e "  挂载点: ${GREEN}$MOUNT${NC}"
            echo -e "  总大小: ${GREEN}$SIZE${NC}"
            echo -e "  已使用: ${GREEN}$USED${NC}"
            echo -e "  可用: ${GREEN}$AVAIL${NC}"
            echo -e "  使用率: ${GREEN}$USE_PERCENT${NC}"
        done
    fi
    echo ""
    
    # 网络信息
    echo -e "${CYAN}【网络信息】${NC}"
    if command -v ip &> /dev/null; then
        # 获取IPv4地址（排除lo和docker接口）
        IP_ADDRS=$(ip -4 addr show 2>/dev/null | grep -v "127.0.0.1" | grep -v "docker" | grep "inet" | awk '{print $2}' | cut -d/ -f1)
        if [ -n "$IP_ADDRS" ]; then
            for ip in $IP_ADDRS; do
                echo -e "  IPv4地址: ${GREEN}$ip${NC}"
            done
        else
            echo -e "  IPv4地址: ${YELLOW}未检测到${NC}"
        fi
    fi
    
    # 已安装的开发工具版本
    echo ""
    echo -e "${CYAN}【已安装的开发工具】${NC}"
    
    if command -v gcc &> /dev/null; then
        GCC_VERSION=$(gcc --version 2>/dev/null | head -1 | awk '{print $NF}')
        echo -e "  GCC: ${GREEN}$GCC_VERSION${NC}"
    else
        echo -e "  GCC: ${RED}未安装${NC}"
    fi
    
    if command -v go &> /dev/null; then
        GO_VERSION=$(go version 2>/dev/null | awk '{print $3}')
        echo -e "  Go: ${GREEN}$GO_VERSION${NC}"
    else
        echo -e "  Go: ${RED}未安装${NC}"
    fi
    
    if command -v node &> /dev/null; then
        NODE_VERSION=$(node -v 2>/dev/null)
        NPM_VERSION=$(npm -v 2>/dev/null)
        echo -e "  Node.js: ${GREEN}$NODE_VERSION${NC} (npm: ${GREEN}$NPM_VERSION${NC})"
    else
        echo -e "  Node.js: ${RED}未安装${NC}"
    fi
    
    if command -v python3 &> /dev/null; then
        PYTHON_VERSION=$(python3 --version 2>/dev/null | awk '{print $2}')
        PIP_VERSION=$(pip3 --version 2>/dev/null | awk '{print $2}' || echo "未知")
        echo -e "  Python: ${GREEN}$PYTHON_VERSION${NC} (pip: ${GREEN}$PIP_VERSION${NC})"
    else
        echo -e "  Python: ${RED}未安装${NC}"
    fi
    
    if command -v docker &> /dev/null; then
        DOCKER_VERSION=$(docker --version 2>/dev/null | awk '{print $3}' | sed 's/,//')
        echo -e "  Docker: ${GREEN}$DOCKER_VERSION${NC}"
    else
        echo -e "  Docker: ${RED}未安装${NC}"
    fi
    
    echo ""
    echo -e "${PURPLE}========================================${NC}"
    echo -e "${PURPLE}        信息报告结束${NC}"
    echo -e "${PURPLE}========================================${NC}"
}

# 主函数
main() {
    print_section "Ubuntu 24.04 开发环境初始化脚本"
    
    # 检查root权限
    check_root
    
    # 执行各个安装步骤
    replace_apt_sources
    install_development_tools
    install_common_software
    install_go
    install_nodejs
    install_python_venv
    install_docker
    expand_disk_space
    collect_system_info
    
    print_section "初始化完成"
    print_success "Ubuntu开发环境初始化完成！"
    
    # 重新加载环境变量
    print_info "正在重新加载环境变量..."
    if [ -f /etc/profile ]; then
        # 在当前shell中无法直接source /etc/profile并影响父进程
        # 但可以在当前脚本上下文中生效
        export PATH=$PATH:/usr/local/go/bin
        if [ -d /etc/profile.d ]; then
            for profile_file in /etc/profile.d/*.sh; do
                if [ -r "$profile_file" ]; then
                    . "$profile_file"
                fi
            done
        fi
        print_success "环境变量已在当前脚本上下文中更新"
    fi
    print_info "请重新登录或运行 'source /etc/profile' 以在新终端中应用所有环境变量"
}

# 运行主函数
main "$@"
