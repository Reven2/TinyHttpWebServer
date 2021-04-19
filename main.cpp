#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <cassert>
#include <sys/epoll.h>

#include "locker.h"
#include "threadpool.h"
#include "http_conn.h"

const int MAX_FD = 65536;           //最大文件描述符
const int MAX_EVENT_NUMBER = 10000; //最大事件数

//告诉编译器到别的文件去找该函数定义
extern int addfd(int epollfd,int fd,bool one_shot);
extern int removefd(int epollfd,int fd);


void addsig(int sig,void(handler)(int),bool restart = true)
{
	struct sigaction sa;
	memset(&sa,'\0',sizeof(sa));
	sa.sa_handler = handler;	//回调函数
	if(restart)
		sa.sa_flags |= SA_RESTART;

//sigfillset() initializes set to full,
//including all signals.
	sigfillset(&sa.sa_mask);

//assert(exp) prints an error message to standard  error  and
//terminates  the  program by calling abort(3),if exp false
	assert(sigaction(sig,&sa,NULL) != -1);
}


void show_error(int connfd,const char* info)
{
	printf("%s",info);
	//把info信息发送到connfd
	send(connfd,info,strlen(info),0);
	close(connfd);
}

int main(int argc,char**argv)
{
	if(argc <= 2)
	{
		printf("usage:%s ip_address port_number\n",
				basename(argv[0]));
		//basename()截取path中最后的文件名或路径名
		return 1;
	}

	const char* ip = argv[1];
	int port = atoi(argv[2]);

	//忽略SIGPIPE信号
	addsig(SIGPIPE,SIG_IGN);

	//创建线程池
	threadpool<http_conn>* pool = NULL;
	try
	{
		pool = new threadpool<http_conn>;
	}
	catch(...)
	{
		return 1;
	}

	//预先为每个可能的客户链接分配一个http_conn对象
	http_conn* users = new http_conn[MAX_FD];
	assert(users);			//判断有没有出错
	int user_count = 0;		//用户数量初始化为 0 

	int listenfd = socket(PF_INET,SOCK_STREAM,0);
	assert(listenfd >= 0);
	
	//设置SO_LINGER所需的结构体参数
	struct linger temp = {1,0}; 

	int ret = 0;
	struct sockaddr_in address;
	bzero(&address,sizeof(address));
	address.sin_family = AF_INET;
	inet_pton(AF_INET,ip,&address.sin_addr);
	address.sin_port = htons(port);

	ret = bind(listenfd,(struct sockaddr*)&address,sizeof(address));
	assert(ret >= 0);

	ret = listen(listenfd,5);
	assert(ret >= 0);

	//就绪事件结构体数组 epoll_wait 传出参数
	epoll_event events[MAX_EVENT_NUMBER];
	
	int epollfd = epoll_create(5);
	assert(epollfd != -1);
	//挂上树 封装函数
	addfd(epollfd,listenfd,false);

	http_conn::m_epollfd = epollfd;//static m_epollfd

	while(1)
	{
		int number = epoll_wait(epollfd,events,MAX_EVENT_NUMBER,-1);
		if((number < 0) && (errno != EINTR))
		{
			printf("epoll failure\n");
			break;
		}
	
		for(int i=0;i<number;i++)
		{	
			int sockfd = events[i].data.fd;

			if(sockfd == listenfd)
			{
				struct sockaddr_in client_address;
				socklen_t client_addrlength = sizeof(client_address);
				int connfd = accept(listenfd,(struct sockaddr*)&client_address,&client_addrlength);
				if(connfd < 0)
				{
					printf("error is %d\n",errno);
					continue;
				}
				if(http_conn::m_user_count >= MAX_FD)
				{
					show_error(connfd,"Internal server busy");
					continue;
				}
			//初始化客户端链接
			users[connfd].init(connfd,client_address);
			}

			else if(events[i].events & (EPOLLIN|EPOLLHUP|EPOLLERR))
			{	//若有异常则直接关闭客户链接
				users[sockfd].close_conn();
			}

			else if(events[i].events & EPOLLIN)
			{//根据读客户端数据结构决定添加任务还是关闭链接
				if(users[sockfd].read())
					pool->append(users + sockfd);	
				else
					users[sockfd].close_conn();
			}

			else if(events[i].events & EPOLLOUT)
			{//根据写的结果决定是否关闭链接
				if(!users[sockfd].write())
					users[sockfd].close_conn();
			}
			
			else {}
		}
	}
	
	close(epollfd);
	close(listenfd);
	delete [] users;
	delete pool;	

	return 0;
}
