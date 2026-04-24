# GPIO字符设备驱动

## 功能概述

本驱动是一个Linux内核模块（字符设备驱动），实现以下功能：
1. 读取指定GPIO引脚的电平状态
2. 通过字符设备接口在用户空间导出节点
3. 通过sysfs接口在用户空间导出节点
4. 允许用户通过文件系统操作读取该GPIO的值

## 文件结构

```
driver/gpio/
├── gpio_driver.c   # 内核模块源代码
├── Makefile        # 编译脚本
├── test_gpio.sh    # 测试脚本
└── README.md       # 本文档
```

## 编译环境要求

- Linux内核头文件（与当前运行内核版本匹配）
- make工具
- gcc编译器

安装依赖（以Debian/Ubuntu为例）：
```bash
sudo apt install linux-headers-$(uname -r) build-essential
```

## 编译模块

进入驱动目录并编译：
```bash
cd driver/gpio
make
```

编译成功后会生成以下文件：
- `gpio_driver.ko` - 内核模块文件
- `gpio_driver.mod.c`
- `gpio_driver.mod.o`
- `gpio_driver.o`
- `modules.order`
- `Module.symvers`

## 加载模块

加载模块时必须指定要监控的GPIO引脚号：
```bash
sudo insmod gpio_driver.ko gpio_pin=18
```

其中`gpio_pin=18`表示要监控GPIO 18引脚。

### 验证模块加载

查看内核日志确认模块加载成功：
```bash
dmesg | grep gpio_driver
```

成功加载后会显示类似信息：
```
gpio_driver: Module loaded successfully for GPIO pin 18
gpio_driver: Device node: /dev/gpio18
gpio_driver: Sysfs interface: /sys/class/gpio_driver/gpio18/value
```

查看已加载的模块：
```bash
lsmod | grep gpio_driver
```

## 用户空间接口

本驱动提供两种用户空间接口：

### 1. 字符设备接口

- **设备节点路径**: `/dev/gpio<引脚号>` （例如 `/dev/gpio18`）
- **权限**: 通常需要root权限读取
- **读取方式**: 标准文件读取操作

### 2. Sysfs接口

- **Sysfs路径**: `/sys/class/gpio_driver/gpio<引脚号>/value`
- **权限**: 所有用户可读（S_IRUGO）
- **读取方式**: 标准文件读取操作

## 使用方法

### 通过字符设备读取

```bash
# 需要root权限
sudo cat /dev/gpio18
```

返回值：
- `0` - GPIO引脚为低电平
- `1` - GPIO引脚为高电平

### 通过Sysfs读取

```bash
# 所有用户均可读取
cat /sys/class/gpio_driver/gpio18/value
```

返回值同上。

### 在程序中使用

#### C语言示例

```c
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

int main() {
    int fd;
    char buf[4];
    ssize_t len;
    
    // 方式1: 通过字符设备
    fd = open("/dev/gpio18", O_RDONLY);
    if (fd >= 0) {
        len = read(fd, buf, sizeof(buf)-1);
        if (len > 0) {
            buf[len] = '\0';
            printf("GPIO值 (字符设备): %s", buf);
        }
        close(fd);
    }
    
    // 方式2: 通过sysfs
    fd = open("/sys/class/gpio_driver/gpio18/value", O_RDONLY);
    if (fd >= 0) {
        len = read(fd, buf, sizeof(buf)-1);
        if (len > 0) {
            buf[len] = '\0';
            printf("GPIO值 (sysfs): %s", buf);
        }
        close(fd);
    }
    
    return 0;
}
```

#### Python示例

```python
# 方式1: 通过字符设备（需要root权限）
try:
    with open('/dev/gpio18', 'r') as f:
        value = f.read().strip()
        print(f"GPIO值 (字符设备): {value}")
except PermissionError:
    print("需要root权限访问字符设备")

# 方式2: 通过sysfs（所有用户可读）
with open('/sys/class/gpio_driver/gpio18/value', 'r') as f:
    value = f.read().strip()
    print(f"GPIO值 (sysfs): {value}")
```

## 运行测试脚本

提供的`test_gpio.sh`脚本可以自动完成编译、加载、测试和清理的全过程。

### 运行测试

```bash
sudo ./test_gpio.sh 18
```

其中`18`是要测试的GPIO引脚号，请根据实际情况修改。

### 测试脚本功能

测试脚本会执行以下步骤：
1. 检查编译环境（内核头文件）
2. 编译内核模块
3. 加载内核模块（指定GPIO引脚）
4. 验证设备节点和sysfs接口是否创建成功
5. 通过两种接口读取GPIO值
6. 询问是否卸载模块并清理编译文件

## 卸载模块

```bash
sudo rmmod gpio_driver
```

### 验证卸载

查看内核日志：
```bash
dmesg | grep gpio_driver | tail -1
```

成功卸载后显示：
```
gpio_driver: Module unloaded
```

## 清理编译文件

```bash
make clean
```

## 注意事项

1. **GPIO引脚选择**: 确保使用的GPIO引脚在当前硬件上是可用的，并且没有被其他驱动占用。
2. **权限**: 
   - 加载/卸载内核模块需要root权限
   - 访问字符设备节点（`/dev/gpioX`）需要root权限
   - 访问sysfs接口（`/sys/class/gpio_driver/...`）所有用户可读
3. **引脚状态**: 本驱动将GPIO配置为输入模式，读取的是当前引脚的电平状态。
4. **内核版本**: 本驱动适用于Linux 3.10+版本，已在常见的嵌入式平台（如Raspberry Pi）测试。

## 常见问题

### Q1: 加载模块时提示"Invalid GPIO pin"
**A**: 请检查指定的GPIO引脚号是否正确，并且该引脚在当前硬件上可用。

### Q2: 加载模块时提示"Failed to request GPIO"
**A**: 该GPIO可能已被其他驱动或系统占用。请检查：
```bash
cat /sys/kernel/debug/gpio
```

### Q3: 读取设备节点时提示"Permission denied"
**A**: 字符设备节点需要root权限访问，可以使用`sudo`或者通过sysfs接口读取（无需root）。

### Q4: 编译时提示"no such file or directory"
**A**: 请确保已安装与当前内核版本匹配的内核头文件：
```bash
sudo apt install linux-headers-$(uname -r)
```

## 技术细节

### 驱动架构

1. **字符设备注册**:
   - 使用`alloc_chrdev_region`分配设备号
   - 使用`cdev_init`和`cdev_add`注册字符设备
   - 实现`file_operations`结构体（open, read）

2. **Sysfs接口**:
   - 使用`class_create`创建设备类
   - 使用`device_create`创建设备文件
   - 使用`DEVICE_ATTR`宏定义sysfs属性
   - 实现`show`函数用于读取属性值

3. **GPIO操作**:
   - 使用`gpio_request`请求GPIO引脚
   - 使用`gpio_direction_input`配置为输入模式
   - 使用`gpio_get_value`读取引脚电平
   - 使用`gpio_free`释放GPIO引脚

### 模块参数

- **gpio_pin**: int类型，指定要监控的GPIO引脚号
- **权限**: S_IRUGO（所有用户可读）
- **默认值**: -1（必须在加载时指定）

## 许可证

GPL v2

## 作者

SoloCoder
