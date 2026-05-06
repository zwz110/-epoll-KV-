#include<iostream>
#include<cstdint>
#include<vector>
#include<string.h>
#include<unordered_map>
#include<netinet/in.h>

std::unordered_map<std::string,std::string> mp_store; 
const size_t k_max_message=32<<20;
typedef std::vector<uint8_t> Buffer;
enum {
    RES_OK = 0,
    RES_ERR = 1,    // error
    RES_NX = 2,     // key not found
};
//序列化数据的标签
enum {
    TAG_NIL = 0,    // 空值 nil
    TAG_ERR = 1,    // 错误：错误码 + 错误消息
    TAG_STR = 2,    // 字符串 string
    TAG_INT = 3,    // 64位整数 int64
    TAG_DBL = 4,    // 双精度浮点数 double
    TAG_ARR = 5,    // 数组 array
};

// 标签的错误代码
enum{
    ERR_UNKNOW=1,   //未知命令
    ERR_TOO_BIG=2,  //回应内容太长
    ERR_PARSER=3,   //解析失败
    ERR_NOT_FIND=4,//未找到键
};
class RequestHandler{
    private:
        uint32_t status;                         //状态码
        std::vector<uint8_t>data;                //响应数据
        std::vector<std::string> cmd;            //命令

        //redis请求格式,
        //nstr:参数个数,大小为4个字节
        //len:参数的长度,大小为4个字节
        // +------+-----+------+-----+------+-----+-----+------+
        // | nstr | len | str1 | len | str2 | ... | len | strn |
        // +------+-----+------+-----+------+-----+-----+------+
        bool read_uint32(uint8_t*&ptr,uint8_t*end,uint32_t &len){
            if(ptr+4>end){
                return false;
            }
            memcpy(&len,ptr,4);
            ptr+=4;
            return true;
        }
        bool read_str(uint8_t*&ptr,uint8_t*end,size_t len,std::string&out){
            if(ptr+len>end){
                return false;
            }
            out.assign(ptr,ptr+len);
            ptr+=len;
            return true;
        }
        bool parser_req(uint8_t*ptr,size_t size){//size:接收缓冲区的数据长度
            uint8_t*end=ptr+size;
            uint32_t nstr=0;
            if(!read_uint32(ptr,end,nstr)){
                perror("解析失败!");
                return false;
            }
            std::cout << "[解析] 参数个数=" << nstr << std::endl;
            if(nstr>k_max_message){
                perror("消息数太多!");
                return false;
            }
            while(cmd.size()<nstr){
                uint32_t len=0;
                if(!read_uint32(ptr,end,len)){
                    std::cout << "[解析] 读取参数 " << cmd.size() << " 的长度失败" << std::endl;
                    return false;
                }
                std::cout << "[解析] 参数 " << cmd.size() << " 长度=" << len << std::endl;
                cmd.push_back(std::string());
                if(!read_str(ptr,end,len,cmd.back())){
                    std::cout << "[解析] 读取参数 " << cmd.size() << " 的数据失败" << std::endl;
                    return false;
                }
                std::cout << "[解析] 参数 " << cmd.size() << " = " << cmd.back() << std::endl;
            }
            std::cout << "[解析] ptr 偏移=" << (ptr - (end-size)) << ", 总大小=" << size << std::endl;
            if(ptr!=end){
                std::cout << "[解析] 检查失败：ptr=" << (void*)ptr << ", end=" << (void*)end 
                          << ", ptr!=end=" << (ptr!=end) << ", nstr=" << nstr 
                          << ", cmd.size()=" << cmd.size() << std::endl;
                return false;
            }
            return true;    
        }
        //各种数据类型二进制序列化
        /*

        标签（Tag）：固定 1 字节
        长度（Length）：固定 4 字节，无符号整数，小端序
        字符串：任意字节序列，长度由 Length 字段指定
        数组：Length 字段表示数组中元素的个数
        整数和浮点数：固定长度，小端序编码

        空值:   | 标签(1B) |
        整数:   | 标签(1B) | 值(8B) |
        浮点数: | 标签(1B) | 值(8B) |
        字符串: | 标签(1B) | 长度(4B) | 数据(...) |
        数组:   | 标签(1B) | 长度(4B) | 元素1 | 元素2 | ... |
        错误:   | 标签(1B) | 错误码(4B) | 消息长度(4B) | 消息(...) |
        */
        //序列化辅助函数
        // 追加1字节无符号整数
        static void buf_append_u8(Buffer&buf,uint8_t data){
            buf.push_back(data);
        }
        // 追加4字节无符号整数（小端序）
        static void buf_append_int32(Buffer&buf,uint32_t data){
            buf.insert(buf.end(),(uint8_t*)&data,(uint8_t*)&data+4);
        }
        // 追加8字节无符号整数（小端序）
        static void buf_append_int64(Buffer&buf,uint64_t data){
            buf.insert(buf.end(),(uint8_t*)&data,(uint8_t*)&data+8);
        }
        // 追加任意字节序列
        static void buf_append(Buffer&buf,uint8_t*data,size_t size){
            buf.insert(buf.end(),data,data+size);
        }
        public:
        //输出空值
        static void out_nil(Buffer&send_buf){
            buf_append_u8(send_buf,TAG_NIL);
        }
        // 输出字符串
        static void out_str(Buffer&send_buf,const char*s,size_t size){
            buf_append_u8(send_buf,TAG_STR);
            buf_append_int32(send_buf,(uint32_t)size);
            buf_append(send_buf,(uint8_t*)s,size);
        }
        // 输出64位整数
        static void out_int(Buffer&send_buf,int64_t val){
            buf_append_u8(send_buf,TAG_INT);
            buf_append_int64(send_buf,val);
        }
        // 输出数组开头（指定元素个数）
        static void out_arr(Buffer&send_buf,uint32_t n){
            buf_append_u8(send_buf,TAG_ARR);
            buf_append_int32(send_buf,n);
        }
        // 输出错误
        static void out_err(Buffer&send_buf,int32_t code,std::string message){
            buf_append_u8(send_buf,TAG_ERR);
            buf_append_int32(send_buf,code);
            buf_append_int32(send_buf,(uint32_t)message.size());
            buf_append(send_buf,(uint8_t*)message.data(),message.size());

        }
        //[4 字节总长度][TAG 标签][数据...]
    public:
    bool handel_request(uint8_t*ptr,size_t size,Buffer&send_buf);
    void do_request(Buffer&send_buf);
        
    void set_statue(uint32_t s){
        status=s;
    }
    void set_data(std::string result){
        data.assign(result.begin(),result.end());
    }
};