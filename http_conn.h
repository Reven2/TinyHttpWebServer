#ifndef HTTPCONNECTION_H
#define HTTPCONNECTION_H
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include <map>
#include"locker.h"

//http_conn 是 threadpool 的模板参数类
class http_conn
{
public:
	//文件的最大长度
	static const int FILENAME_LEN = 256;
	//读缓冲区的大小
	static const int READ_BUFFER_SIZE = 2048;
	//写缓冲区的大小
	static const int WRITE_BUFFER_SIZE = 1024;

	//HTTP请求方法
	enum METHOD
	{
		GET = 0,
		POST,
		HEAD,
		PUT,
		DELETE,
		TRACE,
		OPTIONS,
		CONNECT,
		PATCH
	};

	//主状态机状态
	enum CHECK_STATE
	{
		//当前正在分析请求行 状态
		CHECK_STATE_REQUESTLINE = 0,
		//当前正在分析头部字段 状态
		CHECK_STATE_HEADER,
		//当前正在分析 ？
		CHECK_STATE_CONTENT
	};

	//服务器处理HTTP请求的可能结果
	enum HTTP_CODE
	{
		//请求不完整，需要继续读取客户数据
		NO_REQUEST,
		//表示获得了一个完整客户请求
		GET_REQUEST,
		//表示客户请求语法有错误
		BAD_REQUEST,

		NO_RESOURCE,

		FORBIDDEN_REQUEST,

		FILE_REQUEST,

		INTERNAL_ERROR,

		CLOSED_CONNECTION
	};

	//行的读取状态
	enum LINE_STATUS
	{
		//表示读到了完整行
		LINE_OK = 0,
		//行出错
		LINE_BAD,
		//行数据还不完整
		LINE_OPEN
	};

public:
	http_conn(){}
	~http_conn(){}


public:
	//初始化新接受的链接
	void init(int sockfd,const sockaddr_in &addr);

	//关闭链接
	void close_conn(bool real_close = true);

	//处理客户请求,注意这个函数 就是和线程接轨的地方？
	void process();

	//非阻塞读操作
	bool read();

	//非阻塞写操作
	bool write();


private:
	//初始化链接
	void init();

	//解析HTTP请求报文
	HTTP_CODE process_read();
	//填充HTTP应答报文
	bool process_write(HTTP_CODE ret);

	//下面一组函数被 process_read() 调用拿来分析HTTP请求
	HTTP_CODE parse_request_line(char *text);
	HTTP_CODE parse_headers(char *text);
	HTTP_CODE parse_content(char *text);
	HTTP_CODE do_request();
	char *get_line() {return m_read_buf + m_start_line;}
	LINE_STATUS parse_line();


	//下面一组函数被process_write() 调用拿来填充HTTP应答
	void unmap();
	bool add_response(const char* fromat,...);//变参函数
	bool add_content(const char* content);
	bool add_status_line(int status,const char *title);
	bool add_headers(int content_length);
	bool add_linger();
	bool add_blank_line();


public:
	//epoll内核事件表设为static，所有socket事件都注册到上面
	static int _m_epollfd;
	//统计用户的数量
	static int m_user_count;


private:
	//HTTP链接的 socket 和 对方的 socket 地址
	int m_sockfd;
	sockaddr_in m_address;

	//读缓冲区
	char m_read_buf[READ_BUFFER_SIZE];
	//表示缓冲中已经读入客户数据的最后一个字节的下一个位置
	int m_read_idx;
	//当前正在分析的字符在读缓冲区中的位置
	int m_checked_idx;
	//当前正在解析的 行的起始位置
	int m_start_line;

	//写缓冲区
	char m_write_buf[WRITE_BUFFER_SIZE];
	//写缓冲区中待发送的字节数
	int m_write_idx;

	//主状态机的当期所处状态
	CHECK_STATE m_check_state;
	//请求方法
	METHOD m_method;


	
	//客户请求目标文件的完整路径
	char m_real_file[FILENAME_LEN];
	//客户请求目标文件名
	char *m_url;
	//HTTP版本号，此程序仅支持HTTP/1.1
	char *m_version;
	//主机名
	char *m_host;
	//HTTP请求的消息体的长度？
	int m_content_length;
	//HTTP 请求是否要求保持连接
	bool m_linger;
	

	//客户请求的目标文件被 mmap 到内存中的起始位置
	char *m_file_address;
	//客户请求的目标文件的状态，拿来判断是否存在，是文件还是目录，是否可读 等等文件属性都可以获得
	struct stat m_file_stat;

	//因为采用writev来执行写操作，需要以下成员
	//struct iovc{
	//	void* iov_base;	 内存块起始地址
	//	size_t iov_len;	 内存块的长度
	//	}
	struct iovec m_iv[2];
	int m_iv_count;		//背写内存块的数量

};




















#endif
