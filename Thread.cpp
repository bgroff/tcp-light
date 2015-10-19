/*
 * File: Thread.cpp
 * Class: ICS 451
 * Project #: 3
 * Team Members: Bryce Groff, Brandon Grant, Emiliano Miranda
 * Author: Bryce Groff
 * Created Date: 04-27-09
 * Desc: Represents a Lock.
 */

#include "Thread.h"

Thread::Thread (void *(*func) (void *), void *caller)
	: mCaller(caller), mFunction(func)
{
}

Thread::~Thread()
{
}

int Thread::Start()
{
	return pthread_create(&mThread, NULL, mFunction, mCaller);
}

int Thread::Join()
{
	return pthread_join(mThread, NULL);
}

void Thread::Exit()
{
	pthread_exit(NULL);
}
