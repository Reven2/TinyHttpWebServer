#ifndef LOCKER_H
#define LOCKER_H

#include <exception>
#include <pthread.h>
#include <semaphore.h>


//封装一个信号量类
class sem{

	public:
		//constructor
		sem(){
			if(sem_init(&m_sem,0,0)!=0)
				//初始化成功返回0，失败抛出异常
				throw std::exception();
		}
		sem(int num){
			if(sem_init(&m_sem,0,num)!=0)
				throw std::exception();	
		}

		//destructor
		~sem(){
			sem_destroy(&m_sem);
		}

		//封装+1 和-1 操作
		bool wait(){
			//wait -1 操作 成功返回true 
			return sem_wait(&m_sem)==0;
		}

		bool post(){
			//post +1 操作 成功返回true
			return sem_post(&m_sem)==0;
		}

	private:
		sem_t m_sem;
};

//当进入关键代码段,获得互斥锁将其加锁;离开关键代码段,唤醒等待该互斥锁的线程.
class locker{
	public:
		locker(){
			if(pthread_mutex_init(&m_mutex,NULL)!=0)
				throw std::exception();
		}

		//注意千万不要在析构函数里面抛出异常
		~locker(){
			pthread_mutex_destroy(&m_mutex);
		}

		//加锁,解锁
		bool lock(){
			return pthread_mutex_lock(&m_mutex)==0;
		}
		bool unlock(){
			return pthread_mutex_unlock(&m_mutex)==0;
		}

		//获取锁
		pthread_mutex_t *get(){
			return &m_mutex;
		}

	private:
		pthread_mutex_t m_mutex;

};

//封装有关条件变量类
class cond{
	public:
		cond(){
			if(pthread_cond_init(&m_cond,NULL)!=0)
				throw std::exception();
		}

		~cond(){
			pthread_cond_destroy(&m_cond);
		}

		//注意wait需要参数 互斥量
		bool wait(pthread_mutex_t *m_mutex){
			return pthread_cond_wait(&m_cond,m_mutex)==0;
		}
	
		bool signal(){
			return pthread_cond_signal(&m_cond)==0;
		}

		bool broadcast(){
			return pthread_cond_broadcast(&m_cond)==0;
		}

	private:
		pthread_cond_t m_cond ;

};
























#endif
