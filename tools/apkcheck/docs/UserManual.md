# APKCheck - APK签名工具用户手册

## 版本 1.0.0

---

## 目录

1. [简介](#简介)
2. [系统要求](#系统要求)
3. [安装指南](#安装指南)
4. [命令参考](#命令参考)
5. [使用示例](#使用示例)
6. [技术说明](#技术说明)
7. [故障排除](#故障排除)
8. [安全建议](#安全建议)

---

## 简介

APKCheck 是一个功能完整的 APK 签名工具，使用纯 C 语言开发，不依赖 jarsigner 或 keytool 等外部工具。

### 主要功能

- **密钥库生成**: 支持创建 JKS 格式的密钥库文件
- **APK签名**: 实现完整的 v1 签名算法
- **签名验证**: 验证已签名 APK 的签名信息
- **密钥管理**: 查看和管理密钥库中的密钥

### 支持的算法

- **摘要算法**: MD5, SHA-1, SHA-256, SHA-512
- **签名算法**: SHA1withRSA, SHA256withRSA, SHA512withRSA

---

## 系统要求

### 操作系统

- Linux (Ubuntu 18.04+, Debian 9+, CentOS 7+, RHEL 7+, 等主流发行版)

### 依赖库

- **libzip**: ZIP 文件处理库
- **OpenSSL**: 加密算法库

### 编译依赖

- **GCC** (推荐 7.0 或更高版本)
- **Make**
- **pkg-config**
- **libzip-dev** (开发头文件)
- **libssl-dev** 或 **openssl-devel** (开发头文件)

---

## 安装指南

### 安装依赖库

#### Debian/Ubuntu

```bash
sudo apt-get update
sudo apt-get install -y gcc make libzip-dev libssl-dev pkg-config
```

#### CentOS/RHEL

```bash
sudo yum install -y gcc make libzip-devel openssl-devel pkgconfig
```

#### Fedora

```bash
sudo dnf install -y gcc make libzip-devel openssl-devel pkgconfig
```

### 编译

1. 解压源代码包（如果适用）：
```bash
tar -xzvf apkcheck-1.0.0.tar.gz
cd apkcheck-1.0.0
```

2. 检查依赖库：
```bash
make check-libs
```

3. 编译：
```bash
make
```

编译后的可执行文件位于 `build/bin/apkcheck`。

### 其他编译选项

- **调试版本**: `make debug`
- **优化版本**: `make release`
- **静态链接**: `make static`（需要静态库）

### 安装到系统

```bash
sudo make install
```

默认安装到 `/usr/local/bin/apkcheck`。

### 卸载

```bash
sudo make uninstall
```

---

## 命令参考

### 通用命令格式

```bash
apkcheck <command> [options] [arguments]
```

### 全局选项

| 选项 | 长选项 | 说明 |
|------|--------|------|
| `-h` | `--help` | 显示帮助信息 |
| `-v` | `--version` | 显示版本信息 |
| `-V` | `--verbose` | 启用详细输出 |
| `-l FILE` | `--log-file FILE` | 将日志输出到文件 |

---

### keygen 命令 - 生成密钥库

创建一个新的 JKS 格式密钥库文件。

**命令格式**:
```bash
apkcheck keygen [options]
```

**选项**:

| 选项 | 长选项 | 必需 | 说明 | 默认值 |
|------|--------|------|------|--------|
| `-k FILE` | `--keystore FILE` | 是 | 密钥库文件路径 | - |
| `-a NAME` | `--alias NAME` | 否 | 密钥别名 | mykey |
| `-p PASS` | `--storepass PASS` | 是 | 密钥库密码 | - |
| `-P PASS` | `--keypass PASS` | 否 | 密钥密码 | 与 storepass 相同 |
| `-b BITS` | `--keysize BITS` | 否 | 密钥大小（位） | 2048 |
| `-d DAYS` | `--validity DAYS` | 否 | 有效期（天） | 9125 (25年) |
| `-c NAME` | `--cn NAME` | 否 | 通用名称 (CN) | APKCheck |
| `-u NAME` | `--ou NAME` | 否 | 组织单位 (OU) | - |
| `-O NAME` | `--organization NAME` | 否 | 组织名称 (O) | - |
| `-L NAME` | `--locality NAME` | 否 | 城市/地点 (L) | - |
| `-S NAME` | `--state NAME` | 否 | 州/省份 (ST) | - |
| `-C CODE` | `--country CODE` | 否 | 国家代码 (2位) | - |
| `-g ALG` | `--sigalg ALG` | 否 | 签名算法 | SHA256withRSA |
| `-f` | `--force` | 否 | 强制覆盖已存在的文件 | - |

**支持的签名算法**:
- `SHA1withRSA`
- `SHA256withRSA` (推荐)
- `SHA512withRSA`

---

### sign 命令 - 签名 APK

对 APK 文件进行签名。

**命令格式**:
```bash
apkcheck sign [options] <apk_file>
```

**选项**:

| 选项 | 长选项 | 必需 | 说明 |
|------|--------|------|------|
| `-k FILE` | `--keystore FILE` | 是 | 密钥库文件路径 |
| `-a NAME` | `--alias NAME` | 是 | 密钥别名 |
| `-p PASS` | `--storepass PASS` | 是 | 密钥库密码 |
| `-P PASS` | `--keypass PASS` | 是 | 密钥密码 |
| `-o FILE` | `--output FILE` | 否 | 输出签名 APK 路径（默认：原文件名-signed.apk） |
| `-g ALG` | `--sigalg ALG` | 否 | 签名算法（默认：SHA256withRSA） |
| `-n` | `--no-align-check` | 否 | 跳过 ZIP 对齐检查 |
| `-f` | `--force` | 否 | 强制覆盖输出文件 |

---

### verify 命令 - 验证签名

验证 APK 的签名信息。

**命令格式**:
```bash
apkcheck verify [options] <apk_file>
```

**选项**:

| 选项 | 长选项 | 说明 |
|------|--------|------|
| `-V` | `--verbose` | 显示详细的证书信息 |

**输出信息**:
- 签名版本检测（v1, v2, v3）
- 签名验证结果
- 证书信息（主题、颁发者、序列号、有效期）
- 证书指纹（SHA1, SHA256）

---

### list-keys 命令 - 列出密钥

列出密钥库中的所有密钥条目。

**命令格式**:
```bash
apkcheck list-keys [options]
```

**选项**:

| 选项 | 长选项 | 必需 | 说明 |
|------|--------|------|------|
| `-k FILE` | `--keystore FILE` | 是 | 密钥库文件路径 |
| `-p PASS` | `--storepass PASS` | 是 | 密钥库密码 |

**输出信息**:
- 密钥别名
- 条目类型（PrivateKeyEntry 或 TrustedCertEntry）
- 创建时间
- 证书详细信息（如果有）
- 证书指纹

---

### help 命令

显示帮助信息。

```bash
apkcheck help
```

---

### version 命令

显示版本信息。

```bash
apkcheck version
```

---

## 使用示例

### 示例 1: 创建密钥库

创建一个新的密钥库，用于签名 APK：

```bash
apkcheck keygen \
    -k myapp.keystore \
    -a myapp_key \
    -p "MyStrongPassword123!" \
    -P "MyKeyPassword456!" \
    -b 2048 \
    -d 36500 \
    -c "MyApp Inc." \
    -u "Development" \
    -O "MyApp Corporation" \
    -L "Beijing" \
    -S "Beijing" \
    -C "CN" \
    -g SHA256withRSA
```

### 示例 2: 签名 APK

使用已有的密钥库对 APK 进行签名：

```bash
apkcheck sign \
    -k myapp.keystore \
    -a myapp_key \
    -p "MyStrongPassword123!" \
    -P "MyKeyPassword456!" \
    -o myapp-release-signed.apk \
    myapp-release-unsigned.apk
```

### 示例 3: 验证签名

检查已签名 APK 的签名信息：

```bash
apkcheck verify -V myapp-release-signed.apk
```

### 示例 4: 查看密钥库内容

列出密钥库中的所有密钥：

```bash
apkcheck list-keys \
    -k myapp.keystore \
    -p "MyStrongPassword123!"
```

### 示例 5: 完整工作流程

```bash
# 1. 创建密钥库
apkcheck keygen -k release.keystore -a release -p "pass123" -c "My Company"

# 2. 签名 APK
apkcheck sign -k release.keystore -a release -p "pass123" -P "pass123" \
    -o app-signed.apk app-unsigned.apk

# 3. 验证签名
apkcheck verify app-signed.apk

# 4. 查看密钥库信息
apkcheck list-keys -k release.keystore -p "pass123"
```

---

## 技术说明

### JKS 密钥库格式

APKCheck 实现了完整的 JKS (Java KeyStore) 格式，兼容 Java keytool 生成的密钥库。

**JKS 格式特点**:
- 魔数: `0xFEEDFEED`
- 版本: 2
- 支持的条目类型:
  - `PrivateKeyEntry`: 私钥条目（包含证书链）
  - `TrustedCertEntry`: 受信任证书条目

### APK 签名流程

**v1 签名 (JAR 签名) 流程**:

1. **移除旧签名**: 删除 META-INF 目录下的签名文件
2. **生成 MANIFEST.MF**: 计算每个文件的摘要
3. **生成 CERT.SF**: 基于 MANIFEST.MF 生成签名文件
4. **生成 CERT.RSA**: 使用私钥签名 CERT.SF 内容
5. **创建签名 APK**: 将签名文件添加到 APK

**签名文件结构**:
```
META-INF/
├── MANIFEST.MF    # 包含所有条目的摘要
├── CERT.SF        # 包含 MANIFEST.MF 的摘要和签名
└── CERT.RSA       # PKCS7 格式的签名证书
```

### ZIP 对齐

APK 文件中的资源文件需要按 4 字节对齐，以优化运行时性能。APKCheck 在签名前会自动检查对齐情况。

**对齐检查内容**:
- 未压缩的文件（存储方法为 STORE）
- 文件数据偏移量必须是 4 的倍数

---

## 故障排除

### 常见错误

#### 1. "Keystore file not found"

**错误信息**:
```
Error: Failed to load keystore
```

**可能原因**:
- 密钥库路径不正确
- 文件权限不足

**解决方案**:
```bash
# 检查文件是否存在
ls -la /path/to/keystore

# 检查权限
chmod 600 /path/to/keystore
```

#### 2. "Invalid keystore format"

**错误信息**:
```
Error: Invalid JKS magic or version
```

**可能原因**:
- 文件不是有效的 JKS 格式
- 文件已损坏

**解决方案**:
```bash
# 使用 keytool 检查密钥库格式（如果有 Java）
keytool -list -keystore /path/to/keystore
```

#### 3. "Keystore password was incorrect"

**错误信息**:
```
Error: Failed to get private key
```

**可能原因**:
- 密钥库密码错误
- 密钥密码错误

**解决方案**:
- 确认密码正确
- 注意大小写和特殊字符

#### 4. "Failed to open APK"

**错误信息**:
```
Error: Failed to open APK
```

**可能原因**:
- APK 文件不存在
- 文件权限不足
- 文件不是有效的 ZIP 格式

**解决方案**:
```bash
# 检查文件
file /path/to/app.apk

# 使用 unzip 验证
unzip -t /path/to/app.apk
```

#### 5. 编译错误: "fatal error: zip.h: No such file or directory"

**解决方案**:
```bash
# Debian/Ubuntu
sudo apt-get install libzip-dev

# CentOS/RHEL
sudo yum install libzip-devel
```

#### 6. 编译错误: "fatal error: openssl/ssl.h: No such file or directory"

**解决方案**:
```bash
# Debian/Ubuntu
sudo apt-get install libssl-dev

# CentOS/RHEL
sudo yum install openssl-devel
```

### 调试模式

使用 `-V` 或 `--verbose` 选项获取详细输出：

```bash
apkcheck -V sign -k keystore -a alias app.apk
```

### 日志文件

使用 `-l` 选项将日志输出到文件：

```bash
apkcheck -l apkcheck.log sign ...
```

---

## 安全建议

### 密码安全

1. **使用强密码**:
   - 至少 12 个字符
   - 包含大小写字母、数字和特殊字符
   - 避免使用常见词或模式

2. **不要在命令行中使用密码**:
   ```bash
   # 不安全 - 密码会出现在进程列表中
   apkcheck keygen -p "MyPassword123!" ...
   
   # 建议 - 使用环境变量或交互输入
   ```

3. **保护密钥库文件**:
   ```bash
   # 设置严格的权限
   chmod 600 my.keystore
   
   # 限制访问用户
   chown root:root my.keystore
   ```

### 密钥管理

1. **定期更换密钥**:
   - 建议每 1-2 年更换签名密钥
   - 保留旧密钥以验证旧版本

2. **备份密钥库**:
   - 安全地存储备份
   - 使用加密存储介质

3. **分离开发和生产密钥**:
   - 使用不同的密钥库进行开发和发布
   - 生产密钥应严格控制访问

### 内存安全

APKCheck 已实现以下安全措施：
- 敏感数据使用后内存清零
- 密码在使用后立即清除
- 使用常量时间比较防止时序攻击

---

## 附录

### 与 jarsigner/keytool 的兼容性

APKCheck 生成的密钥库和签名与标准 Java 工具兼容：

```bash
# APKCheck 创建的密钥库可以用 keytool 查看
keytool -list -keystore my.keystore

# APKCheck 签名的 APK 可以用 jarsigner 验证
jarsigner -verify -verbose -certs app-signed.apk
```

### Makefile 目标

| 目标 | 说明 |
|------|------|
| `make` / `make all` | 构建可执行文件 |
| `make debug` | 构建调试版本 |
| `make release` | 构建优化版本 |
| `make static` | 构建静态链接版本 |
| `make clean` | 清理构建文件 |
| `make install` | 安装到系统 |
| `make uninstall` | 从系统卸载 |
| `make test` | 运行测试 |
| `make check-libs` | 检查依赖库 |

### 退出码

| 码值 | 含义 |
|------|------|
| 0 | 成功 |
| 1 | 一般错误 |
| -1 | 无效参数 |
| -2 | 文件未找到 |
| -3 | 权限不足 |
| -4 | I/O 错误 |
| -5 | 内存分配失败 |
| -6 | 无效格式 |
| -7 | 加密错误 |
| -8 | 签名错误 |
| -9 | ZIP 操作错误 |
| -10 | JKS 密钥库错误 |

---

## 版本历史

### v1.0.0 (2024)
- 初始版本
- JKS 密钥库生成和解析
- APK v1 签名
- 签名验证
- 密钥库管理

---

## 许可证

APKCheck 采用开源许可证发布。详见 LICENSE 文件。

---

## 联系方式

如有问题或建议，请联系项目维护者。

---

*本文档最后更新: 2024年*
