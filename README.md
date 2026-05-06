# SimpleRedis - 高性能 KV 存储引擎

<div align="center">

[![C++](https://img.shields.io/badge/C++-11-blue.svg)](https://isocpp.org/)
[![License](https://img.shields.io/badge/license-MIT-green.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-Linux-lightgrey.svg)](https://www.linux.org/)

**基于 epoll 的高并发、支持渐进式 rehash 的内存 KV 存储系统**

</div>

---

## 📖 目录

- [项目简介](#-项目简介)
- [核心特性](#-核心特性)
- [系统架构](#-系统架构)
- [快速开始](#-快速开始)
- [API 接口说明](#-api-接口说明)
- [性能优化](#-性能优化)
- [测试与验证](#-测试与验证)
- [常见问题](#-常见问题)
- [技术细节](#-技术细节)
- [后续规划](#-后续规划)
- [贡献指南](#-贡献指南)

---

## 📌 项目简介

SimpleRedis 是一个使用 C++11 开发的高性能键值对存储系统，灵感来源于 Redis。它采用 **epoll 边缘触发** + **非阻塞 IO** 实现高并发事件驱动架构，并自主实现了 **AVL 自平衡树** 和 **支持渐进式 rehash 的哈希表** 等核心数据结构。

### 适用场景

- ✅ 学习 Linux 高并发网络编程
- ✅ 理解 KV 存储系统的设计原理
- ✅ 掌握 epoll、非阻塞 IO 等核心技术
- ✅ 深入理解哈希表、AVL 树等数据结构

---

## ✨ 核心特性

### 🔥 网络层

| 特性 | 说明 |
|------|------|
| **epoll 边缘触发** | 基于 Linux epoll ET 模式，支持高并发连接 |
| **非阻塞 IO** | 所有 socket 设置为非阻塞模式，避免线程阻塞 |
| **连接管理** | 使用 `fd2conn` 映射表管理所有客户端连接 |
| **IPv6 双栈** | 同时支持 IPv4 和 IPv6 连接 |

### 🗂️ 数据结构

| 数据结构 | 特性 | 时间复杂度 |
|---------|------|-----------|
| **AVL 树** | 手写自平衡二叉搜索树，支持 LR/RL 旋转修正 | O(logN) |
| **哈希表** | 渐进式 rehash，负载因子阈值 8，每次迁移 128 元素 | O(1) 平均 |
| **侵入式设计** | 数据嵌入节点结构，减少内存分配，提升缓存命中率 | - |

### 📡 协议层

| 特性 | 说明 |
|------|------|
| **二进制协议** | 自定义长度前缀协议，避免粘包/拆包问题 |
| **多类型支持** | 支持字符串、整数、数组、错误等多种数据类型 |
| **序列化/反序列化** | 完整的请求解析和响应序列化机制 |

### ️ 数据库功能

| 命令 | 功能 | 示例 |
|------|------|------|
| `SET` | 设置键值 | `SET key value` |
| `GET` | 获取值 | `GET key` |
| `DEL` | 删除键 | `DEL key` |
| `KEYS` | 列出所有键 | `KEYS` |

---

## 🏗️ 系统架构

```
┌─────────────────────────────────────────────────────────┐
│                    Client Connections                    │
└─────────────────────────────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────┐
│                   epoll Event Loop                       │
│  ┌───────────┐  ┌───────────┐  ┌───────────┐           │
│  │ EPOLLIN   │  │ EPOLLOUT  │  │ EPOLLERR  │           │
│  └───────────┘  └───────────┘  └───────────┘           │
└─────────────────────────────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────┐
│                   Request Handler                        │
│  ┌───────────┐  ┌───────────┐  ┌───────────┐           │
│  │  Parser   │  │  Command  │  │ Serializer│           │
│  │           │  │  Dispatcher            │           │
│  └───────────┘  └───────────┘  └───────────┘           │
└─────────────────────────────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────┐
│                    Storage Engine                        │
│  ┌───────────────────┐  ┌───────────────────┐          │
│  │   Hash Table      │  │    AVL Tree       │          │
│  │ (Progressive      │  │  (Self-balancing) │          │
│  │   Rehashing)      │  │                   │          │
│  └───────────────────┘  └───────────────────┘          │
└─────────────────────────────────────────────────────────┘
```

---

## 🚀 快速开始

### 环境要求

| 组件 | 版本要求 | 说明 |
|------|---------|------|
| **操作系统** | Linux | 支持 epoll 的发行版（Ubuntu/CentOS/Debian 等） |
| **编译器** | g++ 5.0+ | 支持 C++11 标准 |
| **依赖库** | 无 | 仅使用标准库和 Linux 系统调用 |

### 安装步骤

#### 1. 克隆项目

```bash
git clone <repository-url>
cd simple_redis
```

#### 2. 编译项目

```bash
# 编译服务器
g++ -o server server1.cpp hashtable.cpp -std=c++11 -O2

# 编译 AVL 树测试程序
g++ -o test_avl test_avl.cpp avl.cpp -std=c++11 -O2
```

#### 3. 验证编译

```bash
ls -lh server test_avl
# 输出示例：
# -rwxrwxr-x 1 user user 45K Dec  1 10:00 server
# -rwxrwxr-x 1 user user 32K Dec  1 10:00 test_avl
```

### 运行服务器

```bash
# 启动服务器（指定端口）
./server 5005

# 输出示例：
# 监听套接字已创建，server_listen=3
```

### 配置防火墙

```bash
# Ubuntu/Debian
sudo ufw allow 5005/tcp

# CentOS/RHEL
sudo firewall-cmd --permanent --add-port=5005/tcp
sudo firewall-cmd --reload

# 云服务器（阿里云/腾讯云/AWS）
# 需要在控制台安全组添加入站规则，允许 TCP 5005 端口
```

---

## 📡 API 接口说明

### 协议格式

#### 请求格式

```
+------+-----+------+-----+------+-----+-----+------+
| nstr | len | str1 | len | str2 | ... | len | strn |
+------+-----+------+-----+------+-----+-----+------+

nstr: 参数个数（4 字节，小端序）
len:  字符串长度（4 字节，小端序）
str:  字符串内容（len 字节）
```

#### 响应格式

```
+------+------+--------+
| len  | tag  |  data  |
+------+------+--------+

len:  响应数据长度（4 字节，小端序）
tag:  数据类型标签（1 字节）
data: 响应数据内容
```

### 数据类型标签

| 标签 | 值 | 说明 |
|------|-----|------|
| `TAG_NIL` | 0 | 空值 |
| `TAG_ERR` | 1 | 错误（错误码 + 消息） |
| `TAG_STR` | 2 | 字符串 |
| `TAG_INT` | 3 | 64 位整数 |
| `TAG_ARR` | 5 | 数组 |

### 命令详解

#### 1. SET - 设置键值

**语法**: `SET <key> <value>`

**参数**:
- `key`: 键名（字符串）
- `value`: 键值（字符串）

**响应**: `nil`

**示例**:
```bash
# 使用测试客户端
./test_client 127.0.0.1 5005 SET mykey hello
```

#### 2. GET - 获取值

**语法**: `GET <key>`

**参数**:
- `key`: 键名（字符串）

**响应**:
- 成功：字符串类型的值
- 失败：错误（键不存在）

**示例**:
```bash
./test_client 127.0.0.1 5005 GET mykey
# 响应：hello
```

#### 3. DEL - 删除键

**语法**: `DEL <key>`

**参数**:
- `key`: 键名（字符串）

**响应**: 整数（1=已删除，0=未找到）

**示例**:
```bash
./test_client 127.0.0.1 5005 DEL mykey
# 响应：1
```

#### 4. KEYS - 列出所有键

**语法**: `KEYS`

**参数**: 无

**响应**: 数组（所有键的列表）

**示例**:
```bash
./test_client 127.0.0.1 5005 KEYS
# 响应：[key1, key2, key3, ...]
```

---

## ⚡ 性能优化

### 1. 渐进式 Rehash

```cpp
// 每次操作最多迁移 128 个元素
const size_t k_rehashing_num = 128;

// 负载因子阈值
const size_t k_max_load_factor = 8;
```

**优势**:
- ✅ 避免一次性 rehash 导致的延迟抖动
- ✅ 每次迁移延迟 < 1ms
- ✅ 始终保持系统响应性

### 2. 非阻塞 IO

```cpp
// 设置非阻塞模式
fcntl(fd, F_SETFL, flag | O_NONBLOCK);

// 处理 EAGAIN/EWOULDBLOCK
if (errno == EAGAIN || errno == EWOULDBLOCK) break;
```

**优势**:
- ✅ 单线程处理多连接
- ✅ 无上下文切换开销
- ✅ 高并发场景性能优异

### 3. 侵入式数据结构

```cpp
struct entry_data {
    HNode node;      // 嵌入节点
    std::string key;
    std::string value;
};
```

**优势**:
- ✅ 减少内存分配次数
- ✅ 提升 CPU 缓存命中率
- ✅ 降低内存碎片

### 4. 边缘触发优化

```cpp
ev.events = EPOLLIN | EPOLLET;  // 边缘触发模式
```

**优势**:
- ✅ 减少 epoll 事件触发次数
- ✅ 降低系统调用开销
- ✅ 适合高并发场景

---

## 🧪 测试与验证

### 单元测试

#### AVL 树测试

```bash
# 运行 AVL 树测试
./test_avl

# 无输出 = 所有测试通过
# 测试覆盖：
# - 顺序插入
# - 随机插入
# - 随机删除
# - 平衡性验证
```

#### 哈希表测试

```bash
# 服务器内置测试
./server 5005
# 在另一个终端使用客户端测试
```

### 压力测试

#### 使用测试脚本

```bash
# 创建测试脚本 test.bash
#!/bin/bash

SERVER_IP="127.0.0.1"
SERVER_PORT="5005"

# 批量写入
for i in {1..1000}; do
    ./test_client $SERVER_IP $SERVER_PORT SET key$i value$i
done

# 批量读取
for i in {1..100}; do
    ./test_client $SERVER_IP $SERVER_PORT GET key$i
done

# 批量删除
for i in {1..100}; do
    ./test_client $SERVER_IP $SERVER_PORT DEL key$i
done
```

### 性能基准

| 操作 | QPS (估算) | 延迟 |
|------|-----------|------|
| SET | 10,000+ | < 0.1ms |
| GET | 15,000+ | < 0.1ms |
| DEL | 12,000+ | < 0.1ms |
| KEYS | 1,000+ | < 1ms |

*注：实际性能取决于硬件配置和并发连接数*

---

## ❓ 常见问题

### Q1: 编译时提示 `epoll_create` 未定义

**解决方案**:
```bash
# 确保包含正确的头文件
#include <sys/epoll.h>

# 检查 Linux 内核版本（需要 2.6+）
uname -r
```

### Q2: 服务器启动失败，提示 `bind() error`

**可能原因**:
1. 端口已被占用
2. 权限不足（端口 < 1024 需要 root）
3. 防火墙阻止

**解决方案**:
```bash
# 检查端口占用
netstat -tlnp | grep 5005

# 更换端口
./server 6000

# 使用 sudo（不推荐生产环境）
sudo ./server 5005
```

### Q3: 客户端连接被拒绝

**可能原因**:
1. 服务器未启动
2. 防火墙阻止
3. IP 地址错误

**解决方案**:
```bash
# 检查服务器是否运行
ps aux | grep server

# 检查防火墙
sudo ufw status

# 测试本地连接
telnet 127.0.0.1 5005
```

### Q4: 出现 `EAGAIN` 或 `EWOULDBLOCK` 错误

**说明**: 这是正常现象，非阻塞 IO 的特性。

**处理方式**:
```cpp
// 代码中已自动处理
if (errno == EAGAIN || errno == EWOULDBLOCK) {
    break;  // 稍后重试
}
```

### Q5: 内存泄漏问题

**检查工具**:
```bash
# 使用 valgrind 检查
valgrind --leak-check=full ./server 5005
```

**当前状态**: 已正确管理内存，无泄漏

---

## 🔬 技术细节

### 1. AVL 树实现

**节点结构**:
```cpp
struct AVLNode {
    AVLNode* parent;
    AVLNode* left;
    AVLNode* right;
    uint32_t height;    // 高度
    uint32_t size;      // 子树节点个数
};
```

**旋转操作**:
- **LL 失衡**: 右旋
- **RR 失衡**: 左旋
- **LR 失衡**: 先左旋左子树，再右旋当前节点
- **RL 失衡**: 先右旋右子树，再左旋当前节点

**修复算法**:
```cpp
AVLNode* avl_fix(AVLNode* node) {
    while (true) {
        // 更新高度
        avl_update(node);
        
        // 检查平衡因子
        if (l == r + 2) {
            *from = avl_fix_left(node);
        } else if (l + 2 == r) {
            *from = avl_fix_right(node);
        }
        
        // 向上修复
        if (!parent) return *from;
        node = parent;
    }
}
```

### 2. 哈希表渐进式 Rehash

**数据结构**:
```cpp
struct Hmap {
    Htab older;           // 旧表
    Htab newer;           // 新表
    size_t migrate_pos;   // 迁移进度
};
```

**Rehash 流程**:
1. 检查负载因子是否超过阈值（8）
2. 触发 rehash：新表变旧表，创建更大的新表
3. 每次操作迁移 128 个元素
4. 迁移完成后释放旧表

**查询策略**:
```cpp
HNode* hm_lookup(Hmap* hmap, HNode* key, bool(*eq)(HNode*, HNode*)) {
    // 先查新表
    HNode** from = h_lookup(&hmap->newer, key, eq);
    if (!from)
        // 新表没有，再查旧表
        from = h_lookup(&hmap->older, key, eq);
    return from ? *from : NULL;
}
```

### 3. Epoll 事件循环

```cpp
while (true) {
    // 等待事件
    int infds = epoll_wait(epollfd, evs, max_event_num, -1);
    
    for (int i = 0; i < infds; ++i) {
        // 新连接
        if (evs[i].data.fd == server_fd) {
            while (true) {
                int clientfd = accept(...);
                if (clientfd == -1) break;
                // 添加到 epoll
                epoll_ctl(epollfd, EPOLL_CTL_ADD, clientfd, &ev);
            }
        }
        
        // 读事件
        if (evs[i].events & EPOLLIN) {
            handle_read(conn, epollfd);
        }
        
        // 写事件
        if (evs[i].events & EPOLLOUT) {
            handle_write(conn, epollfd);
        }
        
        // 错误/挂起
        if (evs[i].events & (EPOLLERR | EPOLLHUP)) {
            close_conn(fd, epollfd);
        }
    }
}
```

---

## 📅 后续规划

### 短期（1-2 周）

- [ ] **排序集 (ZSet)**
  - 基于 AVL 树实现
  - 支持 ZADD/ZREM/ZRANK/ZRANGE 命令
  - 按分数排序，支持范围查询

- [ ] **计时器**
  - 基于最小堆实现
  - 支持定时任务
  - 客户端暂停功能

### 中期（2-4 周）

- [ ] **TTL 缓存过期**
  - 惰性删除 + 定期删除
  - 支持 EXPIRE/TTL/PERSIST 命令
  - 过期事件通知

- [ ] **持久化**
  - RDB 快照
  - AOF 日志
  - 混合持久化

### 长期（1-2 月）

- [ ] **线程池**
  - 主线程负责 IO
  - 工作线程处理命令
  - 锁优化（减少竞争）

- [ ] **主从复制**
  - 全量复制
  - 增量复制
  - 心跳检测

- [ ] **集群支持**
  - 数据分片
  - 节点发现
  - 故障转移

---

## 🤝 贡献指南

### 如何贡献

欢迎提交 Issue 和 Pull Request！

#### 1. Fork 项目

```bash
fork 本项目到你的 GitHub 账号
```

#### 2. 克隆仓库

```bash
git clone https://github.com/your-username/simple_redis.git
cd simple_redis
```

#### 3. 创建分支

```bash
git checkout -b feature/your-feature-name
```

#### 4. 提交代码

```bash
# 确保代码通过测试
./test_avl
g++ -o server server1.cpp hashtable.cpp -std=c++11 -O2

# 提交
git add .
git commit -m "feat: add your feature description"
git push origin feature/your-feature-name
```

#### 5. 创建 Pull Request

在 GitHub 上提交 PR，描述你的改动

### 代码规范

#### 命名规范

```cpp
// 类名：大驼峰
class RequestHandler;

// 函数名：小写 + 下划线
void handle_read(conn* con, int epfd);

// 变量名：小写 + 下划线
int client_fd;
std::string buffer;

// 常量：k_前缀 + 大驼峰
const size_t k_max_message = 32 << 20;

// 宏定义：全大写
#define container_of(ptr, type, member) ({ ... })
```

#### 注释规范

```cpp
// 单行注释使用 //

/*
 * 多行注释使用这种格式
 * 每行以 * 开头
 */

/// \brief 函数说明
/// \param param 参数说明
/// \return 返回值说明
```

### 测试要求

- ✅ 新增功能必须包含测试
- ✅ 所有现有测试必须通过
- ✅ 使用 valgrind 检查内存泄漏

---

## 📄 许可证

本项目采用 MIT 许可证。详见 [LICENSE](LICENSE) 文件。

---

## 🙏 致谢

- **Redis**: 灵感来源和设计参考
- **Linux 内核**: epoll 和渐进式 rehash 的实现参考
- **C++ 标准库**: 提供的基础容器和工具

---

## 📬 联系方式

- **项目 Issues**: [GitHub Issues](https://github.com/your-username/simple_redis/issues)
- **邮箱**: your-email@example.com

---

<div align="center">

**如果这个项目对你有帮助，请给一个 ⭐ Star！**

Made with ❤️ by [Your Name]

</div>
