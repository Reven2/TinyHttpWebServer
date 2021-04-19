#include"http_conn.h"
#include<fstream>

//定义http响应的一些状态信息
const char* ok_200_title = "OK";
const char* error_400_title = "Bad Request";
const char* error_400_form = "Your request has bad syntax or is inherently impossible to staisfy.\n";
const char* error_403_title = "Forbodden";
const char* error_403_form = "You dont have permission to get file from this server.\n";
const char* error_404_title = "Not Found";
const char*	error_404_form = "The request file was not found on thi server.\n";
const char* error_500_title = "Internal Error";
const char* error_500_form = "There was an unusual problem serving the request file.\n";

//网站的根目录
const char *doc_root = "/home/reven/Pictures";



//设置文件描述符为非阻塞
int setnonblocking(int fd)
{
	int old_option = fcntl(fd,F_GETFL);
	int new_option = old_option|O_NONBLOCK;
	fcntl(fd,F_SETFL,new_option);
	return old_option;
}


void addfd(int epollfd,int fd,bool one_shot)
{
	//定义结构体然后初始化 然后挂上红黑树
	epoll_event event; 
	event.data.fd = fd; 
	event.events = EPOLLIN|EPOLLOUT|EPOLLRDHUP;

	//设置操作系统最多触发 此 fd 上注册的一个IN or OUT ..事件
	//且只触发一次,防止例如在读完socket数据，处理事件数据时，
	//又有新数据可读 而再次触发EPOLLIN
	if(one_shot) event.events |= EPOLLONESHOT;

	epoll_ctl(epollfd,EPOLL_CTL_ADD,fd,&event);
	
	setnonblocking(fd);
}


void removefd(int epollfd, int fd)
{
	epoll_ctl(epollfd,EPOLL_CTL_DEL,fd,0);
	close(fd);
}


void modfd(int epollfd,int fd,int ev)
{
	epoll_event event;
	event.data.fd = fd;
	event.events = ev|EPOLLET|EPOLLONESHOT|EPOLLRDHUP;

	epoll_ctl(epollfd,EPOLL_CTL_MOD,fd,&event);
}


//初始化定义在.h中的两个类static变量，用户数量 和 红黑树根
int http_conn::m_user_count = 0;
int http_conn::m_epollfd = -1;


//关闭客户链接
void http_conn::close_conn(bool real_close)
{
	if(real_close && (m_sockfd!=-1))
	{
		removefd(m_epollfd,m_sockfd);//从树上摘下
		m_sockfd = -1;
		m_user_count--;		//关闭连接用户数量-1
	}
}



void http_conn::init(int sockfd,const sockaddr_in& addr)
{
	m_sockfd = sockfd;
	m_address = addr;

	m_user_count++;

	init();
}



//初始化类成员变量
void http_conn::init()
{
	//初始化为 当前正在分析 请求行 状态
	m_check_state = CHECK_STATE_REQUESTLINE;
	//初始化为 不保持连接（非持续链接？）
	m_linger = false;

	m_method = GET; //请求方法
	m_url = 0;		//请求文件名
	m_version = 0; //HTTP version
	m_content_length =0; //请求消息长度
	m_host = 0;   //主机名
	m_start_line = 0; //行开始位置
	m_checked_idx = 0; //当前分析字符在读缓冲区中的位置
	
	//缓冲中已读入客户数据的最后一个字节的下一个字节
	m_read_idx = 0;
	m_write_idx = 0; //写缓冲待发送的字节数

	memset(m_read_buf,'\0',READ_BUFFER_SIZE);
	memset(m_write_buf,'\0',WRITE_BUFFER_SIZE);
	memset(m_real_file,'\0',FILENAME_LEN);
}



/*从状态机*/

//解析出一行数据内容
http_conn::LINE_STATUS http_conn::parse_line()
{
	char temp;
	//循环遍历读缓冲中的字符 解析出每一行
	for(;m_checked_idx < m_read_idx;++m_checked_idx)
	{
		//m_checked_idx :当前字符在缓冲区的位置
		temp = m_read_buf[m_checked_idx];

		//http 报文每行以 \r\n 结尾，若当前分析字节是 '\r'
		if(temp == '\r')
		{	//若是缓冲区中最后一个字节，返回继续读LINE_OPEN
			if((m_checked_idx +1) == m_read_idx)
				return LINE_OPEN;

			//当前分析的下一个字符是\n，说明读到完整行
			else if(m_read_buf[m_checked_idx+1]=='\n')
			{
				//把\r\n 都赋值成 '\0'
				m_read_buf[m_checked_idx++] = '\0';
				m_read_buf[m_checked_idx++] = '\0';
				return LINE_OK;	 
			}
			//不符合以上两种情况，说明行数据出错了
			return LINE_BAD;
		}
		//若当前分析字节是 '\n'
		else if(temp=='\n')
		{
			if((m_checked_idx > 1)&&
				(m_read_buf[m_checked_idx-1]=='\r'))
			{
				m_read_buf[m_checked_idx-1] = '\0';
				m_read_buf[m_checked_idx++] = '\0';
				return LINE_OK;	 				
			}
		
			return LINE_BAD;
		}
	}
	//以上条件并与满足，说明没读到行尾，需要继续读数据
	return LINE_OPEN;
}


//循环从m_sockfd读取客户数据，直到无数据可读或者对方关闭连接
//若是非阻塞ET，则需要一次将数据读完
bool http_conn::read()
{	//如果已读入数据最大下标大于等于缓冲区大小
	if(m_read_idx >= READ_BUFFER_SIZE) return false;
	
	int bytes_read = 0;	//已读入数据字节数
	while(1)
	{
		//  ssize_t recv(int sockfd, void *buf,
		//  				size_t len, int flags)
		bytes_read = recv(m_sockfd,m_read_buf+m_read_idx,
						READ_BUFFER_SIZE-m_read_idx,0);
		if(bytes_read == -1)
		{
			if(errno==EAGAIN||errno==EWOULDBLOCK)
				break;

			return false;
		}
		else if(bytes_read == 0) return false;

		m_read_idx += bytes_read;//刷新已读入字节数
	}
	return true;
}



//解析HTTP请求行，获取 method url http_version
//HTTP_CODE 是服务器处理 HTTP请求可能得到的结果
http_conn::HTTP_CODE http_conn::parse_request_line(char*text)
{
//The  strpbrk(s,accpet)  locates the first occurrence in
// the string s of any of the bytes in the string accept.
	m_url = strpbrk(text," \t");//返回text中的 \t的下一个位置
	if(!m_url) return BAD_REQUEST;//没有空格或者/t，报文错误

	*m_url++ = '\0';

	char* method = text;

	if(strcasecmp(method,"GET")==0) m_method = GET;
	else return BAD_REQUEST;
//The  strspn(s,accept)  calculates the length (in bytes)
// of the initial segment of s which consists  entirely  of
//  bytes in accept.
	m_url += strspn(m_url,"\t");
	m_version = strpbrk(m_url," \t");
	if(!m_version) return BAD_REQUEST;

	*m_version++ = '\0';
	m_version += strspn(m_version," \t");
	if(strcasecmp(m_version,"HTTP/1.1")!=0)
		return BAD_REQUEST;
	if(strncasecmp(m_url,"http://",7)==0)
	{
		m_url += 7;
		m_url = strchr(m_url,'/');
	}
	if(!m_url || m_url[0]!='/')
		return BAD_REQUEST;

	m_check_state = CHECK_STATE_HEADER;
	return NO_REQUEST;

}


//解析HTTP请求的一个头部信息
http_conn::HTTP_CODE http_conn::parse_headers(char* text)
{
	//遇到空行表示头部字段解析完毕
	if(text[0]=='\0')
	{
		//若HTTP请求有消息体，还需要读取m_content_length字节的消息体，状态机转移到CHECK_STATE_CONTENT
		if(m_content_length != 0)
		{
			m_check_state = CHECK_STATE_CONTENT;
			return NO_REQUEST;
		}
		//否则说明我们得到了一个完整的HTTP请求
		return GET_REQUEST;
	}

	//处理Connection 头部字段
	else if(strncasecmp(text,"Connection:",11)==0)
	{
		text += 11;
		text += strspn(text," \t");
		if(strcasecmp(text,"keep-alive")==0)
		m_linger = true ;  //保持连接
	
	}
	//处理Content-Length头部字段
	else if(strncasecmp(text,"Content-Length:",15)==0)
	{
		text += 15;
		text +=strspn(text," \t");
	}
	//处理 Host头部字段
	else if(strncasecmp(text,"Host",5)==0)
	{
		text +=5;
		text +=strspn(text," \t");
		m_host = text;
	}
	else
		printf("oops!unknow header %s\n",text);

	return NO_REQUEST;

}



//处理HTTP请求的消息体，这里只是判断它是否被完整读入即可
http_conn::HTTP_CODE http_conn::parse_content(char* text)
{
	//已读入客户数据大小是否大于 当前解析字节位置+消息体长度
	if(m_read_idx >= (m_content_length + m_checked_idx))
	{
		text[m_content_length]='\0';
		return GET_REQUEST;
	}
	return NO_REQUEST;
}


/*主状态*/

http_conn::HTTP_CODE http_conn::process_read()
{
	LINE_STATUS line_status = LINE_OK;//行状态 完整
	HTTP_CODE ret = NO_REQUEST;	//服务器处理状态 请求不完整
	char *text = 0;

	//条件为 读取了完整行状态，主状态是在分析内容状态
	while(((m_check_state == CHECK_STATE_CONTENT)&&
				(line_status == LINE_OK)) ||
				((line_status = parse_line()) == LINE_OK))
	{
		text = get_line();//text = m_read_buf + m_start_line
		m_start_line = m_checked_idx; //行开始位置
		printf("got a http line:%s\n",text);

		//分析HTTP报文状态机
		switch(m_check_state)
		{
			//解析请求行 的状态
			case CHECK_STATE_REQUESTLINE:
			{
				ret = parse_request_line(text);
				if(ret == BAD_REQUEST)
					break;
			}

			//解析头部的 状态
			case CHECK_STATE_HEADER:
			{
				ret = parse_headers(text);
				if(ret == BAD_REQUEST)
					return BAD_REQUEST;
				//获得完整头部
				else if(ret == GET_REQUEST)
					return do_request();
				break;
			}

			//解析内容的 状态
			case CHECK_STATE_CONTENT:
			{
				ret = parse_content(text);
				//获得完整内容
				if(ret == GET_REQUEST)
					return do_request();
				line_status = LINE_OPEN;
				break;
			}
			default:
				return INTERNAL_ERROR;
		}
	}
	return NO_REQUEST;
}


//当得到了完整的，正确的HTTP请求报文,分析文件属性，若是文件
//则使用mmap将其映射到内存地址m_file_address,通知成功获取文件
http_conn::HTTP_CODE http_conn::do_request()
{
	//复制根目录到 m_real_file
	strcpy(m_real_file,doc_root);
	int len = strlen(doc_root);
	strncpy(m_real_file+len,m_url,FILENAME_LEN-len-1);

	//获取文件属性放到 m_file_stat
	if(stat(m_real_file,&m_file_stat)<0)
		return NO_RESOURCE;

	//判断文件属性是否合法
	if(!(m_file_stat.st_mode & S_IROTH))
		return FORBIDDEN_REQUEST;
	if(S_ISDIR(m_file_stat.st_mode))
		return BAD_REQUEST;

	//读文件内容 然后映射到m_file_address
	int fd = open(m_real_file,O_RDONLY);
	m_file_address = (char*)mmap(0,m_file_stat.st_size,
					PROT_READ,MAP_PRIVATE,fd,0);
	
	close(fd);
	return FILE_REQUEST ;
}

//释放映射内存
void http_conn::unmap()
{
	if(m_file_address)
	{
		munmap(m_file_address,m_file_stat.st_size);
		m_file_address = 0;
	}
}


//处理 HTTP 响应
bool http_conn::write()
{
	int temp = 0;
	int bytes_have_send = 0;		//已经写了多少字节
	int bytes_to_send = m_write_idx; //还有多少字节要写
	if(bytes_to_send == 0)
	{
		modfd(m_epollfd,m_sockfd,EPOLLIN);
		init();
		return true;
	}

	while(1)
	{

		//writev 从m_iv(这是个结构体)把数据写到sockfd
		temp = writev(m_sockfd,m_iv,m_iv_count);
		if(temp <= -1)
		{
		//如果TCP写缓冲没有空间，则等待下一轮的EPOLLOUT事件
		//此期间服务器没办法接受同一个客户的下一个请求
		//但是能保证数据链接的完整性
		 	if(errno == EAGAIN)
			{
				modfd(m_epollfd,m_sockfd,EPOLLOUT);
				return true;
			}
			unmap();
			return false;
		}

		bytes_to_send -= temp ;
		bytes_have_send += temp;
		if(bytes_have_send <= bytes_have_send)
		{
		//发送响应成功，根据conncetion字段决定是否关闭连接
			unmap();
			//如果要保持连接
			if(m_linger){
				//为下一个客户进行初始化准备，挂上树
				init();
				modfd(m_epollfd,m_sockfd,EPOLLIN);
				return true;
			}
			else 
			{
				//断开链接，不再监听该客户端
				modfd(m_epollfd,m_sockfd,EPOLLIN);
				return false;
			}
		}
	}
}


//往写缓冲中写入待发送的数据
bool http_conn::add_response(const char * format,...)
{
	//如果已经写入数据大于写缓冲区大小
	if(m_write_idx >= WRITE_BUFFER_SIZE)
		return false;

	//解决变参问题的一组宏 用于获取不确定个数的参数
	va_list arg_list ;
	//void va_start(va_list ap, last_arg); 
	//last_arg 是最后一个传递给函数的已知的固定参数
	va_start(arg_list,format);

	int len = vsnprintf(m_write_buf + m_write_idx, 
			WRITE_BUFFER_SIZE - 1 - m_write_idx, 
			format, arg_list);

	if (len >= (WRITE_BUFFER_SIZE - 1 - m_write_idx))
	    {
			va_end(arg_list);
			return false;
		}
	m_write_idx += len;  //写缓冲中待发送数据
	va_end(arg_list);
	return true;
}


//应答报文状态行信息
bool http_conn::add_status_line(int status,const char* title)
{
	return add_response("%s %d %s\r\n",
						"HTTP/1.1",status,title);
}

//应答报文头部信息
bool http_conn::add_headers(int content_len)
{
	add_content_length(content_len);
	add_linger();
	add_blank_line();
}

//应答报文Content-Length： 行
bool http_conn::add_content_length(int content_len)
{
	return add_response("Content-Length: %d\r\n",content_len);
}

//应答报文Connection： 行
bool http_conn::add_linger()
{
	return add_response("Connection: %s\r\n",
			(m_linger == true ? "keep-alive":"close"));
}

bool http_conn::add_blank_line()
{
	return add_response("%s","\r\n");
}

//应答报文响应状态信息
bool http_conn::add_content(const char* content)
{
	return add_response("%s",content);
}


//根据服务器处理HTTP请求结果，决定返回客户端的内容
bool http_conn::process_write(HTTP_CODE ret)
{
	switch(ret)
	{
		case INTERNAL_ERROR:
		{
			add_status_line(500,error_500_title);
			add_headers(strlen(error_500_form));
			if(!add_content(error_500_form))
				return false;
			break;
		}
		case BAD_REQUEST:
		{
			add_status_line(400,error_400_title);
			add_headers(strlen(error_400_form));
			if(!add_content(error_400_form));
				return false;
			break;
		
		}
		case NO_REQUEST:
		{
			add_status_line(404,error_404_title);
			add_headers(strlen(error_404_form));
			if(!add_content(error_404_form));
				return false;
			break;
		}
		case FORBIDDEN_REQUEST:
		{
			add_status_line(403,error_403_title);
			add_headers(strlen(error_403_form));
			if(!add_content(error_403_form));
				return false;
			break;
		}
		case FILE_REQUEST:
		{
			add_status_line(200,ok_200_title);
			if(m_file_stat.st_size != 0)
			{
				add_headers(m_file_stat.st_size);

				//写缓冲区起始位置,写缓冲区待发送数据数
				m_iv[0].iov_base = m_write_buf;
				m_iv[0].iov_len = m_write_idx;

				//文件被mmap起始位置，文件大小
				m_iv[1].iov_base = m_file_address;
				m_iv[1].iov_len = m_file_stat.st_size;
				m_iv_count = 2;
				return true;	
			}
			else
			{
				const char* ok_string =
					"<html><body></body></html>";
				add_headers(strlen(ok_string));
				if(!add_content(ok_string))
					return false;
			}
		}
		default:
			return false;
	}
 
	m_iv[0].iov_base = m_write_buf;
	m_iv[0].iov_len = m_write_idx;
	m_iv_count = 1;
	return true;
}

//处理HTTP请求的入口函数，由线程池工作线程调用
void http_conn::process()
{
	//服务器处理分析客户请求可能得到的结果
	HTTP_CODE read_ret = process_read();
	
	//线程分析请求 发现请求出错
	if(read_ret == NO_REQUEST)
	{
		modfd(m_epollfd,m_sockfd,EPOLLIN);
		return;
	}

	//文件没有回写，有其他的异常情况
	bool write_ret = process_write(read_ret);
	if(!write_ret)
	{
		close_conn();
	}

	modfd(m_epollfd,m_sockfd,EPOLLIN);
}


