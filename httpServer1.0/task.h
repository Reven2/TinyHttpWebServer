#ifndef _TASK_H_
#define _TASK_H_

#include <sstream>
#include <fstream>
#include <string>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/sendfile.h>
#include <sys/wait.h>

#include "threadPool.h"

using namespace std;

class Task
{
public:
	Task(){};
	Task(int fd):accp_fd(fd){}
	~Task(){  close(accp_fd);	};

	//处理分析请求
	void doit();

	//构建应答报文
const string construct_header(const int num ,
		const string &info,const string &type);

	//发送应答报文和文件
int send_file(const string &filename,const string &type,
		const int num = 200,const string &info="OK");

	//处理get请求
	int deal_get(const string &uri);
	//处理post请求
	int deal_post(const string &uri,char* buf);
	//获取文件属性
	int get_size(const string &filename);

private:
	int accp_fd;
};



//从accp_fd读取数据
void Task::doit()
{
	char buf[BUFSIZ] = {0};

	while(int nread = recv(accp_fd,buf,BUFSIZ,0))
	{
		//cout<< "nread = " << nread <<end;
		if(nread < 0)
		{
			cout<<"nread error\n";
			exit(1);
		}
		else if(nread == 0)
		{
			cout<<"client is closed\n";
		}

		string method,uri,version;
		stringstream ss;

		//把buf内容读入 ss ，注意 stringstream 忽略空格
		//所以我们可以成功提取出 方法 路径 版本
		ss << buf; 
		ss >> method >> uri >> version;

		cout<<"method = " << method << endl;
		cout<<"uri = " << uri << endl;
		cout<<"version" << version << endl <<endl;

		cout<<"request = \n"<<buf<<"\n" << endl;
		
		if(method == "GET") deal_get(uri);
	//else if(method == "POST") deal_post();
		
		//方法不正确 发送错误信息
		else
		{
			string header = construct_header(
			501,"Not Implement","text/plain");
			send(accp_fd,header.c_str(),header.size(),0);
		}
		//处理完毕则退出
		break;
	}

}

const string Task::construct_header(const int num ,
		const string &info,const string &type)
{	//注意空格间隔和行尾\r\n
	string response = "HTTP/1.1 "+to_string(num)+
		" " + info + "\r\n";
	//部分首部信息
	response += "Server: Reven\r\nContent-Type "+
		type + "; charset=utf-8\r\n\r\n";
	
	return response;
}

int Task::deal_get(const string &uri)
{
	//从uri第一个字节开始复制字串到filename
	 string filename = uri.substr(1);

	 if( uri == "/" || uri == "/index.html" ) {
		send_file( "index.html", "text/html" );
	 } 

	 //string::npos uri表示没有find的字符串
	 else if( uri.find( ".jpg" ) != string::npos ||
			  uri.find( ".png" ) != string::npos ) 
	{
		send_file( filename, "image/jpg" );
	} 
	 else if( uri.find( ".html" ) != string::npos ) {
	 	send_file( filename, "text/html" );
	 } else if( uri.find( ".ico" ) != string::npos ) {
		send_file( filename, "image/x-icon" );
	 } else if( uri.find( ".js" ) != string::npos ) {
		send_file( filename, "yexy/javascript" );
	 } else if( uri.find( ".css" ) != string::npos ) {
		send_file( filename, "text/css" );
	 } else if( uri.find( ".mp3" ) != string::npos ) {
		send_file( filename, "audio/mp3" );
	 } else {
		send_file( "404.html", "text/html", 
			404, "Not Found" );
	}
}



int Task::send_file(const string &filename,
		const string &type,
		const int num,const string &info)
{
	string header = construct_header(num,info,type);

	//send 第二个参数只能是c类型字符串，不能使用string
	send(accp_fd,header.c_str(),header.size(),0);

	struct stat filestat;
	int ret = stat(filename.c_str(),&filestat);
	if(ret < 0||!S_ISREG(filestat.st_mode))
	{
		cout<<"file not found :"<<filename<<endl;
		return -1;
	}

	if(!filename.empty())
	{
		int fd = open(filename.c_str(),O_RDONLY);
		sendfile(accp_fd,fd,NULL,get_size(filename));
		close(fd);
	}

	cout<<filename<<"end finish to "<<accp_fd <<endl;
	return 0;
}

int Task::get_size(const string &filename)
{
	struct stat filestat;
	int ret = stat(filename.c_str(),&filestat);

	if(ret < 0)
		cout<<"get file size fail"<<endl;

	return filestat.st_size;
}





#endif
