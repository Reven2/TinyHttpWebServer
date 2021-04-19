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
	ThreadPool(int thread_num = 5);
	~ThreadPool();
	bool append(T *task);

private:
	static void *worker(void*); //回调函数
	void run();
	T *getTask(); 		//从任务队列在获取任务
};


//constructor
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
		//pthread_creat 成功返回 0 
		if(pthread_create(&threads[i],NULL,worker,this))
		{
			delete [] threads;
			cout<<"pthread_creat error \n";
			throw exception();
		}
	
		//分离线程 线程执行完后 系统自动回收
		if(pthread_detach(threads[i]))
		{
			delete [] threads;
			cout<<"detach error\n";
			throw exception();
		}
	}
}


template<typename T>
ThreadPool<T>::~ThreadPool()
{
	delete [] threads;
	m_stop = true;
}

template<typename T>
bool ThreadPool<T>::append(T *task)
{	
	//操作工作队列要加锁，因为它被所有线程共享
	queue_mutex_locker.lock();
	cout <<"append task ok\n";
	bool need_signal = task_queue.empty();
	task_queue.push(task);
	queue_mutex_locker.unlock();

	//任务队列已经有任务 唤醒线程处理
	if(need_signal){
		queue_cond_locker.signal();
		}
	return true;
}

template<typename T>
void *ThreadPool<T>::worker(void *arg)
{	
	ThreadPool *pool = (ThreadPool*)arg;
	pool->run();
	return pool;
}

template<typename T>
T * ThreadPool<T>::getTask()
{	
	T *task = NULL;
	
	queue_mutex_locker.lock();
	if(!task_queue.empty())
	{
		cout<<"now tasks in queue_task: "<<task_queue.size();
		task = task_queue.front();
		task_queue.pop();
		cout<<"after tasks in queue_task: "<<task_queue.size();
	}
		queue_mutex_locker.unlock();
		
		cout << "get task end \n";
		return task;
}


template<typename T>
void ThreadPool<T>::run()
{
	//循环从任务队列取出任务进行处理
	while(!m_stop)
	{
		T *task =getTask();
		if(!task)
		{
			queue_cond_locker.wait();
			cout<<"thread get task and do\n";
		}
		else
		{
			task->doit();//处理入口函数，task class 来处理	
			delete task; //task 处理完毕 析构对象
		}
	}
}
#endif
