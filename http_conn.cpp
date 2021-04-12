#include"http_conn.h"
#include<fstream>

//定义http响应的一些状态信息
const char* ok_200_title = "OK";
const char* error_400_title = "Bad Request";
const char* error_200_form = "Your request has bad syntax or is inherently impossible to staisfy.\n";
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
