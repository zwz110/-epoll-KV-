#include <iostream>
#include <string>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstdint>
#include <vector>
#include <cstdlib>
#include <cstdio>

// TAG 标签定义
enum {
    TAG_NIL = 0,    // 空值
    TAG_ERR = 1,    // 错误（错误码 4B + 消息长度 4B + 消息）
    TAG_STR = 2,    // 字符串（长度 4B + 数据）
    TAG_INT = 3,    // 整数（8B）
    TAG_DBL = 4,    // 双精度浮点数
    TAG_ARR = 5,    // 数组（元素个数 4B + 元素）
};

// 协议说明：
// 请求格式：[4 字节总长度][4 字节参数个数][4 字节参数 1 长度][参数 1 数据][4 字节参数 2 长度][参数 2 数据]...
// 响应格式：[4 字节总长度][TAG 标签 (1B)][数据...]

// 可靠读取指定字节数
bool read_exact(int sock, void* buf, size_t n) {
    char* p = static_cast<char*>(buf);
    while (n > 0) {
        ssize_t r = recv(sock, p, n, 0);
        if (r <= 0) {
            if (r == 0) {
                std::cerr << "连接已关闭" << std::endl;
            } else {
                perror("recv");
            }
            return false;
        }
        p += r;
        n -= r;
    }
    return true;
}

// 可靠写入指定字节数
bool write_exact(int sock, const void* buf, size_t n) {
    const char* p = static_cast<const char*>(buf);
    while (n > 0) {
        ssize_t r = send(sock, p, n, 0);
        if (r <= 0) {
            perror("send");
            return false;
        }
        p += r;
        n -= r;
    }
    return true;
}

// 发送命令：[4 字节总长度][4 字节参数个数][4 字节参数 1 长度][参数 1 数据]...
void send_command(int sock, const std::vector<std::string>& args) {
    std::vector<uint8_t> message_body;
    
    uint32_t num_args = static_cast<uint32_t>(args.size());
    message_body.insert(message_body.end(),
                       reinterpret_cast<uint8_t*>(&num_args),
                       reinterpret_cast<uint8_t*>(&num_args) + 4);
    
    for (const auto& arg : args) {
        uint32_t len = static_cast<uint32_t>(arg.size());
        message_body.insert(message_body.end(),
                           reinterpret_cast<uint8_t*>(&len),
                           reinterpret_cast<uint8_t*>(&len) + 4);
        message_body.insert(message_body.end(),
                           arg.begin(), arg.end());
    }
    
    uint32_t total_length = static_cast<uint32_t>(message_body.size());
    send(sock, &total_length, sizeof(total_length), 0);
    send(sock, message_body.data(), message_body.size(), 0);
}

// 解析响应数据
struct Response {
    uint32_t status;
    std::string data;
    bool is_error;
    uint32_t error_code;
};

Response recv_response(int sock) {
    uint32_t total_len;
    if (!read_exact(sock, &total_len, 4)) {
        return {0xFFFFFFFF, "", true, 0};
    }
    
    if (total_len < 1) {
        std::cerr << "无效的响应长度：" << total_len << std::endl;
        return {0xFFFFFFFF, "", true, 0};
    }
    
    // 读取 TAG 标签
    uint8_t tag;
    if (!read_exact(sock, &tag, 1)) {
        return {0xFFFFFFFF, "", true, 0};
    }
    
    // 读取剩余数据
    size_t data_len = total_len - 1;
    std::string data;
    if (data_len > 0) {
        data.resize(data_len);
        if (!read_exact(sock, &data[0], data_len)) {
            return {0xFFFFFFFF, "", true, 0};
        }
    }
    
    // 根据 TAG 解析数据
    if (tag == TAG_NIL) {
        std::cout << "  [接收] 类型=nil" << std::endl;
        return {0, "", false, 0};
    }
    else if (tag == TAG_ERR) {
        if (data_len >= 8) {
            uint32_t err_code = *reinterpret_cast<const uint32_t*>(data.data());
            uint32_t msg_len = *reinterpret_cast<const uint32_t*>(data.data() + 4);
            std::string message = (msg_len > 0) ? data.substr(8, msg_len) : "";
            std::cout << "  [接收] 类型=错误，错误码=" << err_code << ", 消息=" << message << std::endl;
            return {err_code, message, true, err_code};
        }
        return {0xFFFFFFFF, "", true, 0};
    }
    else if (tag == TAG_STR) {
        if (data_len >= 4) {
            uint32_t str_len = *reinterpret_cast<const uint32_t*>(data.data());
            std::string str_data = (str_len > 0) ? data.substr(4, str_len) : "";
            std::cout << "  [接收] 类型=字符串，长度=" << str_len << ", 数据=" << str_data << std::endl;
            return {0, str_data, false, 0};
        }
        return {0xFFFFFFFF, "", true, 0};
    }
    else if (tag == TAG_INT) {
        if (data_len >= 8) {
            int64_t int_val = *reinterpret_cast<const int64_t*>(data.data());
            std::cout << "  [接收] 类型=整数，值=" << int_val << std::endl;
            return {0, std::to_string(int_val), false, 0};
        }
        return {0xFFFFFFFF, "", true, 0};
    }
    else if (tag == TAG_ARR) {
        if (data_len >= 4) {
            uint32_t arr_len = *reinterpret_cast<const uint32_t*>(data.data());
            std::cout << "  [接收] 类型=数组，元素个数=" << arr_len << std::endl;
            return {0, "[数组，" + std::to_string(arr_len) + " 个元素]", false, 0};
        }
        return {0xFFFFFFFF, "", true, 0};
    }
    else {
        std::cout << "  [接收] 未知 TAG=" << (int)tag << std::endl;
        return {0xFFFFFFFF, "", true, 0};
    }
}

// SET 命令
bool cmd_set(int sock, const std::string& key, const std::string& value) {
    send_command(sock, {"set", key, value});
    auto resp = recv_response(sock);
    
    if (!resp.is_error && resp.status == 0) {
        std::cout << "✓ SET " << key << " = " << value << " 成功" << std::endl;
        return true;
    } else {
        std::cout << "✗ SET " << key << " 失败：错误码 " << resp.status << std::endl;
        return false;
    }
}

// GET 命令
std::string cmd_get(int sock, const std::string& key) {
    send_command(sock, {"get", key});
    auto resp = recv_response(sock);
    
    if (!resp.is_error && resp.status == 0) {
        std::cout << "✓ GET " << key << " = " << resp.data << std::endl;
        return resp.data;
    } else {
        std::cout << "✗ GET " << key << " 失败：错误码 " << resp.status << std::endl;
        return "";
    }
}

// DEL 命令
bool cmd_del(int sock, const std::string& key) {
    send_command(sock, {"del", key});
    auto resp = recv_response(sock);
    
    if (!resp.is_error && resp.status == 0) {
        int64_t deleted = std::stoll(resp.data);
        if (deleted > 0) {
            std::cout << "✓ DEL " << key << " 成功，删除了 " << deleted << " 个键" << std::endl;
        } else {
            std::cout << "✗ DEL " << key << " 失败：键不存在" << std::endl;
        }
        return deleted > 0;
    } else {
        std::cout << "✗ DEL " << key << " 失败：错误码 " << resp.status << std::endl;
        return false;
    }
}

// KEYS 命令
bool cmd_keys(int sock) {
    send_command(sock, {"keys"});
    auto resp = recv_response(sock);
    
    if (!resp.is_error && resp.status == 0) {
        std::cout << "✓ KEYS = " << resp.data << std::endl;
        return true;
    } else {
        std::cout << "✗ KEYS 失败：错误码 " << resp.status << std::endl;
        return false;
    }
}

// 测试基本命令
void test_basic_commands(int sock) {
    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << "测试 1：基本命令" << std::endl;
    std::cout << std::string(60, '=') << std::endl;
    
    cmd_set(sock, "name", "Redis Test");
    cmd_set(sock, "age", "100");
    cmd_set(sock, "message", "Hello, World!");
    
    cmd_get(sock, "name");
    cmd_get(sock, "age");
    cmd_get(sock, "message");
    
    cmd_keys(sock);  // 获取所有键
    
    cmd_get(sock, "nonexistent");
    
    cmd_del(sock, "age");
    
    cmd_get(sock, "age");
    
    cmd_keys(sock);  // 再次获取所有键
}

// 测试流水线
void test_pipeline(int sock) {
    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << "测试 2：流水线（批量发送）" << std::endl;
    std::cout << std::string(60, '=') << std::endl;
    
    std::vector<std::vector<std::string>> commands = {
        {"set", "pipe1", "value1"},
        {"set", "pipe2", "value2"},
        {"set", "pipe3", "value3"},
        {"get", "pipe1"},
        {"get", "pipe2"},
        {"get", "pipe3"},
        {"del", "pipe1"},
        {"del", "pipe2"},
        {"del", "pipe3"},
    };
    
    std::cout << "批量发送 " << commands.size() << " 个命令..." << std::endl;
    
    for (const auto& cmd : commands) {
        send_command(sock, cmd);
    }
    
    std::cout << "已发送 " << commands.size() << " 个命令，开始接收响应..." << std::endl;
    
    for (size_t i = 0; i < commands.size(); i++) {
        auto resp = recv_response(sock);
        const auto& cmd = commands[i];
        
        if (!resp.is_error && resp.status == 0) {
            if (cmd[0] == "get" && !resp.data.empty()) {
                std::cout << "  响应 " << (i+1) << ": GET = " << resp.data << std::endl;
            } else {
                std::cout << "  响应 " << (i+1) << ": " << cmd[0] << " 成功" << std::endl;
            }
        } else {
            std::cout << "  响应 " << (i+1) << ": " << cmd[0] << " 错误 " << resp.status << std::endl;
        }
    }
}

// 测试大值
void test_large_value(int sock) {
    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << "测试 3：大值测试" << std::endl;
    std::cout << std::string(60, '=') << std::endl;
    
    std::string large_value(10 * 1024, 'X');
    
    std::cout << "SET large_key = (10KB 数据)..." << std::endl;
    cmd_set(sock, "large_key", large_value);
    
    std::cout << "GET large_key..." << std::endl;
    std::string result = cmd_get(sock, "large_key");
    
    if (result.size() == large_value.size()) {
        std::cout << "✓ 大值测试通过！收到 " << result.size() << " 字节" << std::endl;
    } else {
        std::cout << "✗ 大值测试失败！期望 " << large_value.size() 
                  << " 字节，收到 " << result.size() << " 字节" << std::endl;
    }
}

// 测试空值
void test_empty_value(int sock) {
    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << "测试 4：空值测试" << std::endl;
    std::cout << std::string(60, '=') << std::endl;
    
    std::string empty_value = "";
    
    std::cout << "SET empty_key = (空字符串)..." << std::endl;
    cmd_set(sock, "empty_key", empty_value);
    
    std::cout << "GET empty_key..." << std::endl;
    std::string result = cmd_get(sock, "empty_key");
    
    if (result.size() == 0) {
        std::cout << "✓ 空值测试通过！收到 0 字节" << std::endl;
    } else {
        std::cout << "✗ 空值测试失败！期望 0 字节，收到 " << result.size() << " 字节" << std::endl;
    }
}

// 测试特殊字符
void test_special_chars(int sock) {
    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << "测试 5：特殊字符测试" << std::endl;
    std::cout << std::string(60, '=') << std::endl;
    
    // 包含空格、标点符号
    std::string special_value = "Hello, World! @#$%^&*()_+-=[]{}|;':\",./<>?";
    
    std::cout << "SET special_key = (特殊字符)..." << std::endl;
    cmd_set(sock, "special_key", special_value);
    
    std::cout << "GET special_key..." << std::endl;
    std::string result = cmd_get(sock, "special_key");
    
    if (result == special_value) {
        std::cout << "✓ 特殊字符测试通过！" << std::endl;
    } else {
        std::cout << "✗ 特殊字符测试失败！" << std::endl;
    }
}

// 测试中文
void test_unicode(int sock) {
    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << "测试 6：Unicode 字符测试" << std::endl;
    std::cout << std::string(60, '=') << std::endl;
    
    std::string unicode_value = "你好，世界！こんにちは！안녕하세요!";
    
    std::cout << "SET unicode_key = (Unicode 字符串)..." << std::endl;
    cmd_set(sock, "unicode_key", unicode_value);
    
    std::cout << "GET unicode_key..." << std::endl;
    std::string result = cmd_get(sock, "unicode_key");
    
    if (result == unicode_value) {
        std::cout << "✓ Unicode 测试通过！" << std::endl;
    } else {
        std::cout << "✗ Unicode 测试失败！期望：" << unicode_value 
                  << "，收到：" << result << std::endl;
    }
}

// 测试大量键
void test_many_keys(int sock) {
    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << "测试 7：大量键测试" << std::endl;
    std::cout << std::string(60, '=') << std::endl;
    
    int count = 100;
    
    std::cout << "批量 SET " << count << " 个键..." << std::endl;
    for (int i = 0; i < count; i++) {
        std::string key = "key_" + std::to_string(i);
        std::string value = "value_" + std::to_string(i);
        cmd_set(sock, key, value);
    }
    
    std::cout << "KEYS (应该有 " << count + 3 << " 个键)..." << std::endl;
    cmd_keys(sock);
    
    std::cout << "随机 GET 测试..." << std::endl;
    cmd_get(sock, "key_50");
    cmd_get(sock, "key_99");
    
    std::cout << "批量 DEL " << count << " 个键..." << std::endl;
    for (int i = 0; i < count; i++) {
        std::string key = "key_" + std::to_string(i);
        cmd_del(sock, key);
    }
    
    std::cout << "✓ 大量键测试完成！" << std::endl;
}

// 测试覆盖写
void test_overwrite(int sock) {
    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << "测试 8：覆盖写测试" << std::endl;
    std::cout << std::string(60, '=') << std::endl;
    
    std::cout << "SET overwrite_key = value1..." << std::endl;
    cmd_set(sock, "overwrite_key", "value1");
    
    std::cout << "GET overwrite_key = " << std::endl;
    cmd_get(sock, "overwrite_key");
    
    std::cout << "SET overwrite_key = value2 (覆盖)..." << std::endl;
    cmd_set(sock, "overwrite_key", "value2");
    
    std::cout << "GET overwrite_key = " << std::endl;
    cmd_get(sock, "overwrite_key");
    
    std::cout << "✓ 覆盖写测试完成！" << std::endl;
}

// 测试 KEYS 命令
void test_keys_command(int sock) {
    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << "测试 9：KEYS 命令专项测试" << std::endl;
    std::cout << std::string(60, '=') << std::endl;
    
    // 先清理一些键
    cmd_del(sock, "test_key1");
    cmd_del(sock, "test_key2");
    cmd_del(sock, "test_key3");
    
    std::cout << "KEYS (初始状态)..." << std::endl;
    cmd_keys(sock);
    
    std::cout << "SET test_key1, test_key2, test_key3..." << std::endl;
    cmd_set(sock, "test_key1", "value1");
    cmd_set(sock, "test_key2", "value2");
    cmd_set(sock, "test_key3", "value3");
    
    std::cout << "KEYS (添加 3 个键后)..." << std::endl;
    cmd_keys(sock);
    
    std::cout << "DEL test_key2..." << std::endl;
    cmd_del(sock, "test_key2");
    
    std::cout << "KEYS (删除 1 个键后)..." << std::endl;
    cmd_keys(sock);
    
    std::cout << "✓ KEYS 命令测试完成！" << std::endl;
}

// 测试错误处理
void test_error_handling(int sock) {
    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << "测试 10：错误处理测试" << std::endl;
    std::cout << std::string(60, '=') << std::endl;
    
    std::cout << "GET 不存在的键..." << std::endl;
    cmd_get(sock, "nonexistent_key_1");
    
    std::cout << "DEL 不存在的键..." << std::endl;
    cmd_del(sock, "nonexistent_key_2");
    
    std::cout << "✓ 错误处理测试完成！" << std::endl;
}

int main(int argc, char* argv[]) {
    const char* host = "127.0.0.1";
    int port = (argc > 1) ? atoi(argv[1]) : 5005;
    
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket() error");
        return 1;
    }
    
    struct sockaddr_in server_addr = {};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    inet_pton(AF_INET, host, &server_addr.sin_addr);
    
    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect() error");
        std::cerr << "无法连接到 " << host << ":" << port << std::endl;
        std::cerr << "请确保服务器已启动：./server1 " << port << std::endl;
        close(sock);
        return 1;
    }
    
    std::cout << "已连接到 " << host << ":" << port << std::endl;
    
    try {
        // 基础测试
        test_basic_commands(sock);
        test_pipeline(sock);
        test_large_value(sock);
        
        // 增强测试
        test_empty_value(sock);
        test_special_chars(sock);
        test_unicode(sock);
        test_many_keys(sock);
        test_overwrite(sock);
        test_keys_command(sock);
        test_error_handling(sock);
        
        std::cout << "\n" << std::string(60, '=') << std::endl;
        std::cout << "🎉 所有测试完成！" << std::endl;
        std::cout << std::string(60, '=') << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "测试失败：" << e.what() << std::endl;
    }
    
    close(sock);
    return 0;
}
