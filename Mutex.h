/*
 * File: Mutex.h
 * Class: ICS 451
 * Project #: 3
 * Team Members: Bryce Groff, Brandon Grant, Emiliano Miranda
 * Author: Bryce Groff
 * Created Date: 04-19-09
 * Desc: Represents a Lock.
 */
#ifndef _MUTEX_H_
#define _MUTEX_H_

#include <pthread.h>
#include <inttypes.h>
#include <time.h>

/*! \class Mutex
    \brief Wrapper around pthread locks.

   Mutex gives a wrapper around the pthread mutual exclusion mechanism.
*/
class Mutex {
	public:
		Mutex();
		Mutex(int type);
		~Mutex();
		
		int				Lock(); //!< Locks this lock.
		int				TryLock(); //!< Tries to lock this lock. If 0 is returned, then the lock is required; otherwise, an error number is returned.
		int				Unlock(); //!< Unlocks this lock.
		int		 		TimedWait(timespec &); //!< Waits for timespec amount of time then timesout
		int				Wait(); //!< Waits until signaled
		int				Signal(); //!< Signals the condition variable to continue.
	
	private:
		pthread_mutex_t mLock;
		pthread_cond_t	mCond;
		pthread_mutexattr_t mAttr;
};
#endif
