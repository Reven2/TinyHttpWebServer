#ifndef _LOCKER_H_
#define _LOCKER_H_

#include<iostream>
#include<exception>
#include<pthread.h>
using namespace std;

//mutex when append or pop task on queue
class MutexLocker
{
	public:
		//constructor
		MutexLocker()
		{	 //return 0 on success
			if(pthread_mutex_init(&m_mutex,NULL))
				{
					cout<<"mutex init error\n"<<endl;
					throw exception();
				}
		}

		~MutexLocker()
		{
			pthread_mutex_destroy(&m_mutex);
		}

		bool lock()
		{	
			return pthread_mutex_lock(&m_mutex)==0;
		}

		bool unlock()
		{
			return pthread_mutex_unlock(&m_mutex)==0;
		}
	
	private:
		pthread_mutex_t m_mutex;
};

//(thread)  wakeuped or blocked rely on if has tasks in queue
class Cond
{
	public:
		Cond()
		{
			if(pthread_cond_init(&m_cond,NULL))
				throw exception();

			if(pthread_mutex_init(&m_mutex,NULL))
				throw exception();
		}

		~Cond()
		{
			pthread_mutex_destroy(&m_mutex);
			pthread_cond_destroy(&m_cond);
		}

		//阻塞一个线程，直到收到条件变量m_cond的通知
		bool wait()
		{
			return pthread_cond_wait(&m_cond,&m_mutex)==0;
		}

		//唤醒阻塞等待此条件变量的某个线程
		bool signal()
		{
			return pthread_cond_signal(&m_cond)==0;
		}

		//唤醒所有阻塞等待此条件变量的线程
		bool brocast()
		{
			return pthread_cond_broadcast(&m_cond)==0;
		}

	private:
		pthread_cond_t m_cond;
		pthread_mutex_t m_mutex;
};

#endif
