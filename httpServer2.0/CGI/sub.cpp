#include <iostream>
#include <string>
#include <cstring>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

using namespace std;

int main(int argc,char **argv)
{
	int a,b,accp_fd;

	//把输入字符串参数 分别存放到 a , b ,accp_fd
	sscanf(argv[0],"%d-%d,%d",&a,&b,&accp_fd);
	cout<< " CGI si runing"<<endl;

	int result = a-b;
	//http 应答报文状态行
	string response = "HTTP/1.1 200 OK\r\nContent-Type: text/html;charset=utf-8\r\n\r\n";
	
	//实体体
	string body = "<html><head><title>Reven's CGI</title></head>";

	body += "<body><p>The result is " + to_string(a) + "-" + 				to_string(b) + " = " + to_string(result);
	body += "</p></body></html>";
	response += body;

	//输出重定向到客户端socket
	dup2(accp_fd,STDOUT_FILENO);
	cout<<response.c_str();

	return 0;
}
