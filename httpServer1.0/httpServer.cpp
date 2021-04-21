#include "httpServer.h"
#include "task.h"

int main(int argc,char** argv)
{
	if(argc != 2)
	{
		cout<<"usage:./HttpServer port\n";
		return -1;
	}

	int port = atoi(argv[1]);

	//初始化
	HttpServer httpServer(port);
	httpServer.run();


return 0;	
}
