#ifndef _HTTPSERVER_H_
#define _HTTPSERVER_H_

#include"task.h"
#define MAX_EVENT 20

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
	signal(SIGPIPE,SIG_IGN); //忽略SIGPIPE信号

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
	ThreadPool<Task> threadpool(20);
	

	epoll_fd = epoll_create(1024);
	if(epoll_fd<0) 
	{
		cout<<"epoll_create error\n";
		exit(-1);
	}

	epoll_event events[MAX_EVENT]; //epoll_wait 参数
	epoll_event event;		//结构体

	//设置结构体，挂上树，listenfd LT模式
	event.data.fd = sock_fd;
	event.events = EPOLLIN|EPOLLHUP;
	epoll_ctl(epoll_fd,EPOLL_CTL_ADD,sock_fd,&event);



	while(1)
	{
		//epoll_wait 返回就绪事件个数，events传出参数
		int ret=epoll_wait(epoll_fd,events,MAX_EVENT,-1);
		if(ret<0)
		{
			cout<<"epoll_wait error\n";
			exit(-1);
		}

		for(int i=0;i<ret;i++)
		{
			int fd = events[i].data.fd;

			if(fd == sock_fd) //监听fd 说明是新链接
			{
				struct sockaddr_in client_addr;
				socklen_t client_addr_len = sizeof(client_addr);
		
			//注意accept是阻塞调用的，
			//你要用客户端链接来测试
			//不然程序不会往下执行的...
				int conn_fd = accept(sock_fd,
					(struct sockaddr*)&client_addr,
									&client_addr_len);

					if(conn_fd < 0)
					{
						cout<<"accpet error ,line "<<__LINE__<<endl;
						exit(-1);
					}	
	
				addfd(epoll_fd,true,conn_fd);
			}
			//异常事件
			else if(events[i].events & 
					(EPOLLERR|EPOLLHUP|EPOLLRDHUP))
				removefd(epoll_fd,fd);
			//读事件
			else if(events[i].events & EPOLLIN)
			{
				Task *task = new Task(fd);
				threadpool.append(task);
			}
			else cout<<"other events"<<endl;
		}
	}	
	return 0;
}

#endif
