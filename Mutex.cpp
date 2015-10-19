/*
 * File: Mutex.cpp
 * Class: ICS 451
 * Project #: 3
 * Team Members: Bryce Groff, Brandon Grant, Emiliano Miranda
 * Author: Bryce Groff
 * Created Date: 04-19-09
 * Desc: Represents a Lock.
 */

#include "Mutex.h"

Mutex::Mutex()
{
	pthread_mutex_init(&mLock, NULL);
	pthread_cond_init(&mCond, NULL);
}

Mutex::Mutex(int type)
{
	pthread_mutexattr_init(&mAttr);
	pthread_mutexattr_settype(&mAttr, type);
	pthread_mutex_init(&mLock, &mAttr);
	pthread_cond_init(&mCond, NULL);
}

Mutex::~Mutex()
{
	pthread_mutex_destroy(&mLock);
	pthread_mutexattr_destroy(&mAttr);
	pthread_cond_destroy(&mCond);
}

int Mutex::Lock()
{
	return pthread_mutex_lock(&mLock);
}

int Mutex::TryLock()
{
	return pthread_mutex_trylock(&mLock);
}

int Mutex::Unlock()
{
	return pthread_mutex_unlock(&mLock);
}

int Mutex::TimedWait(timespec &waitTime)
{
	return pthread_cond_timedwait(&mCond, &mLock, &waitTime);
}

int Mutex::Wait()
{
	return pthread_cond_wait(&mCond, &mLock);
}

int Mutex::Signal()
{
	return pthread_cond_signal(&mCond);
}
