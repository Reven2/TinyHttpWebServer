#ifndef _HTTPSERVER_H_
#define _HTTPSERVER_H_

#include"task.h"
using namespace std;

class HttpServer
{
public:
	HttpServer(int p):port(p),sock_fd(0)
	{
		memset(&server_addr,0,sizeof(server_addr));
	}
	~HttpServer(){ close(sock_fd); };
	int run();

private:
	int port;
	int sock_fd;	//using in listening
	int epoll_fd;
	struct sockaddr_in server_addr;

};


int setnonblocking(int fd)
{
	int old_option = fcntl(fd,F_GETFL);
	int new_option = old_option |O_NONBLOCK;
	fcntl(fd,F_SETFL,new_option);
	return old_option;
}


void addfd(int epoll_fd,bool oneshot,int fd)
{
	epoll_event event;  //传出就绪事件数组
	event.data.fd = fd;
	event.events = EPOLLIN | EPOLLET; //ET设置
	if(oneshot) event.events |= EPOLLONESHOT;
	epoll_ctl(epoll_fd,EPOLL_CTL_ADD,fd,&event);
	setnonblocking(fd);
}


void removefd(int epoll_fd,int fd)
{
	epoll_ctl(epoll_fd,EPOLL_CTL_DEL,fd,0);
	close(fd);
}




int HttpServer::run()
{
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	sock_fd = socket(AF_INET,SOCK_STREAM,0);
	if(sock_fd < 0)
	{
		//__LINE__ 这个宏是这行代码的位置
		cout<<"socket error ,line "<<__LINE__<<endl;
		return -1;
	}

	int ret = bind(sock_fd,
					(struct sockaddr*)&server_addr,		
						sizeof(server_addr));
	if(ret < 0)
	{
		cout<<"bind error ,line "<<__LINE__<<endl;
		return -1;
	}

	ret = listen(sock_fd,5);
	if(ret < 0)
	{
		cout<<"listen error ,line "<<__LINE__<<endl;
		return -1;
	}
	cout << "listen ok \n";

	//创建线程池
	ThreadPool<Task> threadpool(5);
	
	while(1)
	{
		struct sockaddr_in client_addr;
		socklen_t client_addr_len = sizeof(client_addr);
		cout << "enter loop ok\n";

		//注意accept是阻塞调用的，你要用客户端链接来测试
		//不然程序不会往下执行的...
		int conn_fd = accept(sock_fd,(struct sockaddr*)
				&client_addr,&client_addr_len);

		cout<<"accept ok\n";
		sleep(2);	
		if(conn_fd < 0)
		{
			cout<<"accpet error ,line "<<__LINE__<<endl;
			exit(-1);
		}	
	
		//需要new，不然线程函数没运行完，Task就被析构
		Task *task = new Task(conn_fd);

		threadpool.append(task);
	}	
	return 0;
}

#endif
