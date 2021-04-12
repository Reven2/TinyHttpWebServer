#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <list>
#include <cstdio>
#include <exception>
#include <pthread.h>
#include "locker.h"

//线程池类，定义为模板类是为了代码复用
//模板参数 T 是 任务类

template<typename T>
class threadpool
{
	public:
		//constructor
		threadpool(int m_thread_number = 8,int m_max_requests = 10000);
		//destructor
		~threadpool();
	
		//往请求队列中 添加任务函数
		bool append(T* request);

	private:
		//工作线程运行的函数，它不断从任务队列取出任务执行
		static void* worker(void* arg);
		void run();


	private:
		int m_thread_number; //线程池中的线程数
		int m_max_requests; //队列中允许的最大请求(任务)数
		pthread_t* m_threads;//描述线程池的数组，大小为m_thread_number
		std::list<T*> m_workqueue;//请求（任务）队列(list类)
		locker m_queuelocker;//保护请求队列的锁(locker类型)
		sem m_queuestat;	//是否有任务需要处理(sem类型)
		bool m_stop;		//是否结束线程

};


//constructor
template<typename T>
threadpool<T>::threadpool(int thread_number,int max_requests):m_thread_number(thread_number),m_max_requests(max_requests),m_stop(false),m_threads(NULL)
{
	//池内数组小于0 或者 请求队列请求数目小于0 是异常情况
	if(thread_number<=0 || max_requests<=0)
		throw std::exception();

	//分配空间 创建线程池 用数组描述线程池
	m_threads = new pthread_t[m_thread_number];
	if(!m_threads)
		throw std::exception();

	//创建 thread_number 个线程，并将他们设置为分离线程
	//分离线程 该类线程终止时系统会自动清理并将其移除
	for(int i=0;i<thread_number;i++){

		//m_threads+i指示每个线程的地址
		//worker() 是线程执行回调函数
		//this传入的 threadpool对象 是 worker() 需要的参数
		if(pthread_create(m_threads+i,NULL,worker,this)!=0)
		{
			delete[] m_threads;
			throw std::exception();
		}
		if(pthread_detach(m_threads[i]))
		{
			delete [] m_threads;
			throw std::exception();
		}
	}
}


template<typename T>
threadpool<T>::~threadpool(){
	//删除释放线程池数组
	delete [] m_threads;
	//设置结束线程
	m_stop = true;
}


//往队列中添加任务
template<typename T>
bool threadpool<T>::append(T* request)
{
	//操作工作队列要加锁，因为它被所有线程共享
	m_queuelocker.lock();

	//队列中的请求(任务)大于 允许的最大请求数目 拒绝添加进入
	if(m_workqueue.size()>m_max_requests){
		m_queuelocker.unlock();
		return false;
	}
	//任务添加进入请求队列
	m_workqueue.push_back(request);
	m_queuelocker.unlock();

	//任务 +1，表示有任务需要处理
	m_queuestat.post();
	return true;
}


template<typename T>
void *threadpool<T>::worker(void *arg)
{
	//注意worker 参数传进来的是一个this(threadpool对象)
	threadpool* pool = (threadpool*)arg;
	pool->run();
	return pool;
}

template<typename T>
void threadpool<T>::run()
{
	while(1)
	{
		m_queuestat.wait(); //任务 -1 , 处理了一个任务

		m_queuelocker.lock(); //加锁处理任务队列变化
		if(m_workqueue.empty()) //如果任务队列早已经是空
		{
			m_queuelocker.unlock();
			continue;
		}

		T* request = m_workqueue.front(); //从队列取出任务
		m_workqueue.pop_front();//将已取出任务从队列中删除
		if(!request) continue;	//取出异常为空，循环继续取
		
		//执行任务逻辑
		//注意request->process , request对象的类型应该是http_conn,process是具有的方法
		request->process();
	}
}


#endif
