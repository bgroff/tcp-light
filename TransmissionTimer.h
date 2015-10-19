/*
 * File: TransmissionTimer.h
 * Class: ICS 451
 * Project #: 3
 * Team Members: Bryce Groff, Brandon Grant, Emiliano Miranda
 * Author: Emiliano Miranda
 * Created Date: 04-22-09
 * Desc: This file contains the declaration for the TransmissionTimer class
 * which can act as a timed notification mechanism for transmissions.
 */

#ifndef _TRANSMISSION_TIMER_H_
#define _TRANSMISSION_TIMER_H_

#include <errno.h>
#include <pthread.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include "Mutex.h"
#include "Thread.h"

#define kTransmissionTimerInfiniteInterval -1

class TransmissionTimer
{
	public:
		TransmissionTimer(unsigned int milliSeconds, void* caller, void (*callBackFunc)(void*));
		TransmissionTimer(unsigned int milliSeconds, int intervalCt, void* caller, void (*callBackFunc)(void*));
		~TransmissionTimer();
		int GetIntervalCount();
		void SetIntervalCount(int intervalCt);
		unsigned int GetTimerDelay();
		void SetTimerDelay(unsigned int milliSeconds);
		void Start(bool abortIfStarted);
		void Start(bool abortIfStarted, unsigned int newDelayMilliSeconds);
		void Stop();

	private:
		static void* 	_DoTimer(void *arg);

		unsigned int	mDelayMilliSecs;
		int				mIntervalCt;
		//unsigned int	mSecs;
		//useconds_t		mUsecs; // Unsigned int
		void*			mTimerCaller;
		void (*mTimerCallBack)(void*);
		volatile bool	mIsRunning; // Protection against possiblity of multiple threads on different cores seeing different value in L1 cache.
		volatile bool	mIsPaused;
		volatile bool	mDoAbort;
		time_t			mSecs;
		long			mNanoSecs;
		Mutex			mMutex;
		Thread			mTimerThread;
		//Thread*			mOldTimerThread; // Used to stop an existing thread.
};

#endif
