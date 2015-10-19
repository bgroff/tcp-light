/*
 * File: TransmissionTimer.cpp
 * Class: ICS 451
 * Project #: 3
 * Team Members: Bryce Groff, Brandon Grant, Emiliano Miranda
 * Author: Emiliano Miranda
 * Created Date: 04-22-09
 * Desc: This file contains the definition for the TransmissionTimer class
 * which can act as a timed notification mechanism for transmissions.
 */

#include "TransmissionTimer.h"
#include "Transmission.h"
#include <iostream>

TransmissionTimer::TransmissionTimer(unsigned int milliSeconds, void* caller, void (*callBackFunc)(void*))
	: mTimerThread(_DoTimer, this), mMutex(PTHREAD_MUTEX_RECURSIVE), mTimerCaller(caller), mTimerCallBack(callBackFunc),
	 mIsPaused(true), mDoAbort(false)
{
	this->mIntervalCt = 0;
	SetTimerDelay(milliSeconds);

	this->mIsRunning = true;
	mTimerThread.Start();
}

TransmissionTimer::TransmissionTimer(unsigned int milliSeconds, int intervalCt, void* caller, void (*callBackFunc)(void*))
	: mTimerThread(_DoTimer, this), mMutex(PTHREAD_MUTEX_RECURSIVE), mTimerCaller(caller), mTimerCallBack(callBackFunc),
	mIsPaused(true), mDoAbort(false)
{
	SetIntervalCount(intervalCt);
	SetTimerDelay(milliSeconds);

	this->mIsRunning = true;
	mTimerThread.Start();
}

TransmissionTimer::~TransmissionTimer()
{
}

int TransmissionTimer::GetIntervalCount()
{
	return this->mIntervalCt;
}

void TransmissionTimer::SetIntervalCount(int intervalCt)
{
	if (intervalCt < kTransmissionTimerInfiniteInterval)
	{
		intervalCt = kTransmissionTimerInfiniteInterval;
	}

	this->mIntervalCt = intervalCt;
}

unsigned int TransmissionTimer::GetTimerDelay()
{
	return this->mDelayMilliSecs;
}

void TransmissionTimer::SetTimerDelay(unsigned int milliSeconds)
{
	this->mDelayMilliSecs = milliSeconds;
	this->mSecs = milliSeconds / 1000;
	this->mNanoSecs = (long)((milliSeconds % 1000) * 1000000L);
}

void TransmissionTimer::Start(bool abortIfStarted, unsigned int newDelayMilliSeconds)
{
	SetTimerDelay(newDelayMilliSeconds);
	Start(abortIfStarted);
}

/* Function: Start
 * Desc: This function starts the timer. If there are no iterations set, then
 * the timer will only be run once. The abortIfStarted parameter determines if 
 * the timer stop a current timer operation and restart. If true, then if the
 * timer is currently running, it will be stopped (without a callback 
 * notification) and the timer will be reset and started again. If false, then
 * the timer will only start if the it is currently not already running.
 */
void TransmissionTimer::Start(bool abortIfStarted)
{
	mMutex.Lock();

	// Check if the timer is running and if we should start a new timer.
	if (abortIfStarted && this->mIsRunning)
	{
		mDoAbort = true;
	}

	// Check if we have to keep track of an interval count.
	if (this->mIntervalCt > 0)
	{
		this->mIntervalCt--;
	}

	if (mIsPaused)
	{
		mIsPaused = false;
	}

	mMutex.Signal();
	mMutex.Unlock();
}

/* Function: Stop
 * Desc: This function stops the timer and any future iterations from running. The 
 * callback function will not be called from calling this function.
 */
void TransmissionTimer::Stop()
{
	mMutex.Lock();

	if (this->mIsRunning && !this->mIsPaused)
	{
		mMutex.Signal();

		if (this->mIntervalCt != 0)
		{
			this->mIntervalCt = 0;
		}

		//this->mIsRunning = false;
		this->mIsPaused = true;
	}

	mMutex.Unlock();
}

void* TransmissionTimer::_DoTimer(void* arg)
{
	TransmissionTimer* timer = (TransmissionTimer*)arg;

	if (arg != NULL)
	{
		bool isTimeOut = false;

		timer->mMutex.Lock();
		
		// Check if we can run the timer.
		while (timer->mIsRunning)
		{
			while (timer->mIsPaused)
			{
				timer->mMutex.Wait();
			}

			if (timer->mIsRunning)
			{
				isTimeOut = false;
				struct timespec absTime; // Store the sleep time.
				clock_gettime(CLOCK_REALTIME, &absTime); // First get the current time.
				absTime.tv_sec += timer->mSecs; // Add seconds offset.
				absTime.tv_nsec += timer->mNanoSecs; // Add nanoseconds offset.

				//cout << "Waiting for time out... " << dec << timer->mSecs << endl;


				int retVal = timer->mMutex.TimedWait(absTime);
				isTimeOut = retVal == ETIMEDOUT;

				/*if (isTimeOut)
				{
					cout << "Not aborted or stopped." << endl;
				}
				else
				{
					cout << "Aborted or stopped!" << endl;
				}*/

				if (!timer->mDoAbort)
				{
					// Check if we need to notify a callback function.
					if (timer->mIsRunning && !timer->mIsPaused)
					{
						if (isTimeOut)
						{
							if (timer->mTimerCallBack != NULL)
							{
								// Call the callback function to notify of timer time out.
								timer->mTimerCallBack(timer->mTimerCaller);
							}

							// We are only going to restart the timer if we have remaining iterations and
							// we timed out (i.e. we weren't stopped). Alternatively, if for
							// some reason we aren't running the timer, we will just stop, even if there
							// are iterations left.
							if (timer->mIntervalCt > 0)
							{
								timer->mIntervalCt--;
							}
							else if (timer->mIntervalCt == 0)
							{
								timer->mIsRunning = false;
							}
						}
					}
				}
				else
				{
					timer->mDoAbort = false;
				}
			}
		}

		timer->mMutex.Unlock();
	}

	timer->mTimerThread.Exit();
}
