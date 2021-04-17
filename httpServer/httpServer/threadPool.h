#ifndef _THREADPOOL_H_
#define _THREADPOOL_H_

#include"locker.h"
#include<queue>
using namespace std;

template<typename T>
class ThreadPool
{
private:
	int thread_number; //线程池的线程数量
	pthread_t *threads;//线程数组（注意类型）
	queue<T*> task_queue; //任务队列
	MutexLocker queue_mutex_locker; //操纵队列互斥量
	Cond queue_cond_locker; //是否有队列任务 条件变量
	bool m_stop; //是否结束线程池运行


public:
	//成员函数声明
	ThreadPool(int thread_num = 20);
	~ThreadPool();
	bool append(T *task);

private:
	static void *worker(void*); //回调函数
	void run();
	T *getTask(); 		//从任务队列在获取任务
};


template<typename T>
ThreadPool<T>::ThreadPool(int thread_num):thread_number(thread_num),threads(NULL),m_stop(false)
{
	if(thread_number < 0) 
	{
		cout<<"thread_number < 0\n";
		throw exception();
	}

	//创建数组存放线程号(线程结构体)
	threads = new pthread_t[thread_number];
	if(!threads)
	{
		cout<<"new threads exception!\n";
		throw exception();
	}

	//循环创建线程，注意worker是static的
	//要使用类成员函数和变量，所以将类对象作为参数传给worker
	for(int i =0;i<thread_number;i++)
	{
		if(pthread_create(threads[i],NULL,worker,this))
		{
			delete [] threads;
			cout<<"pthread_creat error \n";
			throw exception();
		}
	
		//分离线程
		if(pthread_detach(threads[i]))
		{
			delete [] threads;
			cout<<"detach error\n";
			throw exception();
		}
	}
}








#endif
