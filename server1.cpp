#include<sys/epoll.h>
#include<sys/socket.h>
#include<unistd.h>
#include<fcntl.h>
#include<cassert>
#include"parse.h"
#include"hashtable.h"
//#include<netinet/in.h>
//#include<vector>
//#include<string.h>
//#include<iostream>

// container_of macro for getting struct pointer from member pointer
#define container_of(ptr, type, member)({\
const typeof(((type*)0)->member)*_mptr=(ptr);\
(type*)((char*)_mptr-offsetof(type,member));})
    


//static 修饰函数 = 仅在当前编译单元（.c/.cpp 文件）可见
static int set_no_block(int fd){
    int flag=fcntl(fd,F_GETFL,0);
    if(flag==-1){
        perror("fcntl F_GETFL failed!");
        return  -1;
    }
    if(fcntl(fd,F_SETFL,flag | O_NONBLOCK)==-1){
        perror("fcntl F_SETFL failed!");
        return  -1;
    }
    return 0;
}
static int create_listen(int port){
    int server_fd=socket(AF_INET6,SOCK_STREAM,0);
    if(server_fd==-1){
        std::cerr<<"socket(server)  error!"<<std::endl;
        return -1;
    }
    int off=0;
    // 设置 IPv6 双栈（允许 IPv4 映射）
    if(setsockopt(server_fd,IPPROTO_IPV6,IPV6_V6ONLY,&off,sizeof(off))==-1){
        perror("setsockopt(IPV6_V6ONLY) error!");
        close(server_fd);
        return -1;
    }

    int opt = 1;
    //允许绑定处于 TIME_WAIT 状态的端口；
    if(setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))==-1){
        perror("setsockopt(SO_REUSEADDR) error!");
        close(server_fd);
        return -1;
    }
    struct sockaddr_in6 addr={};
    addr.sin6_family=AF_INET6;
    addr.sin6_port=htons(port);
    addr.sin6_addr=in6addr_any;
    if(bind(server_fd,(struct sockaddr*)&addr,sizeof(addr))==-1){
        perror("bind() error!");
        close(server_fd);
        return -1;
    }
    if(listen(server_fd,65535)!=0){
        perror("listen() error!");
        close(server_fd);
        return -1;
    }
    if(set_no_block(server_fd)==-1){
        perror("set_no_block(server_fd) error!");
        close(server_fd);
        return -1;
    }
    return server_fd; 
}
struct conn{
    int fd=-1;
    // 缓冲区

    std::vector<uint8_t>recv_buf;
    std::vector<uint8_t>send_buf;
    // 状态
    bool want_write=false;
    bool want_read=true;
    bool is_closed=false;
    
};
//const size_t k_max_message=32<<20;

static bool handel_accept(int epfd,conn*con){
    struct epoll_event ev;
    ev.data.fd=con->fd;
    if(con->want_read&&con->want_write){
        ev.events=EPOLLIN | EPOLLOUT |EPOLLET;
    }
    else if(con->want_read)     ev.events=EPOLLIN | EPOLLET;
    else if(con->want_write)    ev.events=EPOLLOUT | EPOLLET;
    else{
        epoll_ctl(epfd,EPOLL_CTL_DEL,con->fd,NULL);
        return false;
    }
    epoll_ctl(epfd,EPOLL_CTL_ADD,con->fd,&ev);
    return true;
}


const int max_event_num=10;
std::vector<conn*>fd2conn;  // a map of all client connections, keyed by fd

// 函数声明
void handle_read(conn*con,int epfd);
void handel_write(conn*con,int epfd);
void close_conn(int fd,int epfd);
bool try_one_request(conn*con,int epfd);

int main(int argc,char*argv[]){
    if(argc!=2){
        std::cout << "Using:./4_2test6 通讯端口 \n";
        std::cout << "Example:./4_2test6 5005 \n\n";
        std::cout << "注意：运行服务端程序的Linux系统的防火墙必须要开通5005端口。\n";
        std::cout << "      如果是云服务器，还要开通云平台的访问策略。\n\n";
        return -1;
    }
    int server_fd=create_listen(atoi(argv[1]));
    if(server_fd==-1){
        perror("create_listen() error!");
        return -1;
    }
    std::cout<<"监听套接字已创建，server_listen="<<server_fd<<std::endl;
    // 创建 epoll 句柄
    int epollfd=epoll_create(1);
    if(epollfd==-1){
        perror("epoll_create() failed");
        close(server_fd);
        return -1;
    }
    struct epoll_event ev;
    ev.events=EPOLLIN | EPOLLET;
    ev.data.fd=server_fd;
    if(epoll_ctl(epollfd,EPOLL_CTL_ADD,server_fd,&ev)==-1){
        perror("epoll_ctl() failed");
        close(server_fd);
        close(epollfd);
        return -1;
    }
    epoll_event evs[max_event_num];
    //事件循环
    while(true){
        // 等待监视的socket有事件发生。
        int infds=epoll_wait(epollfd,evs,max_event_num,-1);
        if(infds==-1){
            perror("epoll_wait() error!");
            if(errno==EINTR)    continue;
            close(epollfd);
            close(server_fd);
            return -1;
        }
        else if(infds==0){
            std::cout<<"超时，正在重试...\n";
            continue;
        }
        else{
            for(int i=0;i<infds;++i){
                //服务端事件
                if(evs[i].data.fd==server_fd){
                    while(true){
                        struct sockaddr_in6 client_addr;
                        socklen_t addrlen=sizeof(client_addr);
                        int clientfd=accept(evs[i].data.fd,(struct sockaddr*)&client_addr,&addrlen);
                        if(clientfd==-1){
                            if(errno==EINTR)    continue;
                            if(errno==EAGAIN || errno==EWOULDBLOCK) break;
                            perror("accept() error!");
                            break;
                        }
                        else{
                            std::cout<<"客户端已连接,clientfd="<<clientfd<<std::endl;
                            set_no_block(clientfd);
                            conn*new_con=new conn();
                            new_con->fd=clientfd;
                            if(handel_accept(epollfd,new_con)){
                                if(fd2conn.size()<=clientfd){
                                    fd2conn.resize(clientfd+1);
                                }
                                fd2conn[clientfd]=new_con;
                            }
                        }
                    }
                }
                if(evs[i].events&EPOLLIN){
                    std::cout<<"主备处理文件描述符为"<<evs[i].data.fd<<"的读事件!"<<std::endl;
                    conn*c=fd2conn[evs[i].data.fd];
                    if(c) handle_read(c,epollfd);
                }
                if(evs[i].events&EPOLLOUT){
                    std::cout<<"主备处理文件描述符为"<<evs[i].data.fd<<"的写事件!"<<std::endl;
                    conn*c=fd2conn[evs[i].data.fd];
                    if(c) handel_write(c,epollfd);
                }
                if(evs[i].events&(EPOLLERR | EPOLLHUP)){
                    int fd=evs[i].data.fd;
                    conn*c=fd2conn[fd];
                    if(c){
                        fd2conn[fd]=nullptr;     // 1. 从映射表删除
                        epoll_ctl(epollfd,EPOLL_CTL_DEL,c->fd,NULL);    // 2. 从 epoll 删除
                        close(fd);   // 3. 关闭 fd
                        delete c;       // 4. 释放连接对象
                    }
                }
            }
        }
    }
    for(int i=0;i<fd2conn.size();++i){
        if(!fd2conn[i])
            close_conn(i,epollfd);
    }
    close(epollfd);
    close(server_fd);
    return 0;
}

struct {
    Hmap db;
}g_data;
struct entry_data{
    struct HNode node;
    std::string key;
    std::string value;
};
static bool entry_eq(HNode*lhs,HNode*rhs){
    struct entry_data*le=container_of(lhs,struct entry_data,node);
    struct entry_data*re=container_of(rhs,entry_data,node);
    return le->key==re->key;
}
static uint64_t fnv1a_64(const uint8_t*data,size_t len){
    uint64_t hash=0xCBF29CE484222325ULL;    // 64位FNV偏移基础值,FNV标准固定初始值，不可自定义，ULL：明确 64 位无符号常量
    for(size_t i=0;i<len;++i){
        hash^=data[i];                      //异或，不同为1，相同为0
        hash*=0x00000100000001B3ULL;        // 64位FNV质数
    }
    return hash;
}
static void do_get(std::vector<std::string>&cmd,RequestHandler&spon,Buffer&send_buf){
    entry_data tmp;
    tmp.key.swap(cmd[1]);   //键值
    tmp.node.hcode=fnv1a_64((uint8_t*)tmp.key.data(),tmp.key.size());
    HNode*node=hm_lookup(&g_data.db,&tmp.node,entry_eq);
    if(!node){
        spon.out_err(send_buf,ERR_NOT_FIND,std::string("未找到目标键!")); //键不存在
        return ;
    }
    const std::string &val=container_of(node,entry_data,node)->value;
    assert(val.size()<=k_max_message);
    return spon.out_str(send_buf,val.data(),val.size());
}
static void do_set(std::vector<std::string>&cmd,RequestHandler&spon,Buffer&send_buf){
    entry_data tmp;
    tmp.key.swap(cmd[1]);
    tmp.node.hcode=fnv1a_64((uint8_t*)tmp.key.data(),tmp.key.size());
    HNode*node=hm_lookup(&g_data.db,&tmp.node,entry_eq);
    if(node){
        container_of(node,entry_data,node)->value.swap(cmd[2]);
    }
    else{
        entry_data*new_node=new entry_data();
        new_node->key.swap(tmp.key);
        new_node->node.hcode=tmp.node.hcode;
        new_node->value.swap(cmd[2]);
        hm_insert(&g_data.db,&new_node->node);
    }
    return spon.out_nil(send_buf);
}
static void do_del(std::vector<std::string>&cmd,RequestHandler&spon,Buffer&send_buf){
    entry_data tmp;
    tmp.key.swap(cmd[1]);
    tmp.node.hcode=fnv1a_64((uint8_t*)tmp.key.data(),tmp.key.size());
    HNode*from=hm_delete(&g_data.db,&tmp.node,entry_eq);
    if(from){
        delete(container_of(from,entry_data,node));
    }
    return spon.out_int(send_buf,from?1:0);
}
static bool cb_keys(HNode*node,void*arg){
    Buffer&send_buf=*(Buffer*)arg;
    const std::string&key=container_of(node,entry_data,node)->key;
    RequestHandler::out_str(send_buf,key.data(),key.size());
    return true;
}
static void do_keys(std::vector<std::string>&cmd,Buffer&send_buf){
    RequestHandler::out_arr(send_buf,(uint32_t)hm_size(&g_data.db));
    hm_foreach(&g_data.db,&cb_keys,(void*)&send_buf);
}
void RequestHandler::do_request(Buffer&send_buf){
            if(cmd.size()==3&&cmd[0]=="set"){
                return do_set(cmd,*this,send_buf);//避免多余的内存分配和拷贝,如果 value 很大（比如几 MB），swap 比赋值快得多。
            }
            else if(cmd.size()==2&&cmd[0]=="get"){
                return do_get(cmd,*this,send_buf);
                }
            else if(cmd.size()==2&&cmd[0]=="del"){
                return do_del(cmd,*this,send_buf);
            }
            else if(cmd.size()==1&&cmd[0]=="keys"){
                return do_keys(cmd,send_buf);
            }
            else{
                out_err(send_buf,ERR_UNKNOW,std::string("未识别的命令!"));
            }
        }
bool RequestHandler::handel_request(uint8_t*ptr,size_t size,Buffer&send_buf){
        cmd.clear();
        data.clear();
        size_t header=send_buf.size();
        buf_append_int32(send_buf,0) ;//长度占位
        if(!parser_req(ptr,size)){
            out_err(send_buf,ERR_PARSER,std::string("命令解析失败!"));
        }
        else{
        do_request(send_buf);
        }
        size_t data_len=send_buf.size()-header-4;
        memcpy(&send_buf[header],&data_len,4);
        return true;
    }

bool try_one_request(conn*con,int epfd){
    if(con->recv_buf.size()<4){
        return false;//继续读
    }
    uint32_t len=0;
    memcpy(&len,con->recv_buf.data(),4);
    if(len>k_max_message){
        perror("发送的消息太长!");
        close_conn(con->fd,epfd);
        return false;//简单处理，关闭连接(后续可以完善)
    }
    if(len+4>con->recv_buf.size()){
        return false;//继续读
    }
    /*
    const uint8_t *request=&con->recv_buf[4];
    printf("client says: len:%d data:%.*s\n",len, len < 100 ? len : 100, request);
    con->send_buf.insert(con->send_buf.end(),(uint8_t*)&len,(uint8_t*)&len+4);
    con->send_buf.insert(con->send_buf.end(),request,request+len);
    con->recv_buf.erase(con->recv_buf.begin(),con->recv_buf.begin()+4+len);
    */
   RequestHandler rh;
   rh.handel_request(con->recv_buf.data()+4,len,con->send_buf);
    con->recv_buf.erase(con->recv_buf.begin(),con->recv_buf.begin()+4+len);
    return true;
}
void close_conn(int fd,int epfd){
    conn*c=fd2conn[fd];
    if(!c)  return;
    fd2conn[fd]=nullptr;
    epoll_ctl(epfd,EPOLL_CTL_DEL,fd,NULL);
    close(fd);
    delete c;
}
void handle_read(conn*con,int epfd){
    if(con){
        std::cout<<"开始读"<<std::endl;
        int fd=con->fd;
        ssize_t recv_num;
        //size_t total_read=0;
        uint8_t buf[4096];;//4kB
        while(true){
            recv_num=recv(fd,buf,sizeof(buf),0);
            if(recv_num<0){
                if(errno==EINTR)    continue;
                if(errno==EAGAIN || errno==EWOULDBLOCK) break;
                close_conn(fd,epfd);
                perror("recv() error");
                break;
            }
            else if(recv_num==0){//对端关闭连接
                close_conn(fd,epfd);
                std::cout<<"连接正常关闭,to_recv\n";
                break;
            }
            else{
                con->recv_buf.insert(con->recv_buf.end(),buf,buf+recv_num);
            }
        }
        std::cout<<"读结束!"<<std::endl;
        //根据协议处理内容，加入到发送缓冲区。`
        while(try_one_request(con,epfd)){
            if (fd2conn[fd] == nullptr) return; // try_one_request 可能关闭连接
        }
        //加入到发送缓冲区,判断是否注册写事件
        if(con->send_buf.size()>0&&!con->want_write){
            con->want_write=true;
            epoll_event ev;
            //ev.events=EPOLLET | EPOLLIN | EPOLLOUT;
            ev.events=EPOLLIN | EPOLLOUT;
            ev.data.fd=fd;
            epoll_ctl(epfd,EPOLL_CTL_MOD,fd,&ev);
        }
    }
    else{
        perror("传入handle_read的连接结构体指针是空的!");
        return ;
    } 
}

void handel_write(conn*con,int epfd){
    if(con){
        std::cout<<"开始发送"<<std::endl;
        int fd=con->fd;
        uint8_t*ptr=con->send_buf.data();
        size_t need_send=con->send_buf.size();
        size_t have_sent=0;
        while(need_send>0){
            ssize_t n=send(fd,ptr+have_sent,need_send,0);
            if(n<0){
                if(errno==EINTR)    continue;
                if(errno==EAGAIN || errno==EWOULDBLOCK) break;
                //handle_error(con);
                close_conn(fd,epfd);
                con->is_closed = true;
                perror("send() error");
                break;
            }
            else{
                need_send-=n;
                have_sent+=n;
            }
        }
        std::cout<<"发送了"<<have_sent<<"个字节\n";
        con->send_buf.erase(con->send_buf.begin(),con->send_buf.begin()+have_sent);
        if(con->send_buf.size()==0&&con->want_write){
            con->want_write=false;
            epoll_event ev;
            ev.events=EPOLLET | EPOLLIN;
            ev.data.fd=fd;
            epoll_ctl(epfd,EPOLL_CTL_MOD,fd,&ev);
        }
    }
    else{
        perror("传入handle_read的连接结构体指针是空的!");
        return;
    }
}

