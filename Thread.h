/*
 * File: Thread.h
 * Class: ICS 451
 * Project #: 3
 * Team Members: Bryce Groff, Brandon Grant, Emiliano Miranda
 * Author: Bryce Groff
 * Created Date: 04-27-09
 * Desc: Represents a Lock.
 */
#ifndef _THREAD_H_
#define _THREAD_H_

#include <pthread.h>

/*! \class Thread
    \brief Wrapper around pthread threads.

   Thread gives a wrapper around pthread threads.
*/
class Thread {
	public:
		Thread (void *(void *), void *);
		 ~Thread();
		int 		Start();
		int 		Join();
		void		Exit();

	private:
		pthread_t	mThread;
		void 		*mCaller;
		void 		*(*mFunction)(void *);
};
#endif
