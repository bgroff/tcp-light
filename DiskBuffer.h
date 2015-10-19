/*
 * File: DiskBuffer.h
 * Class: ICS 451
 * Project #: 3
 * Team Members: Bryce Groff, Brandon Grant, Emiliano Miranda
 * Author: Bryce Groff
 * Created Date: 04-23-09
 * Desc: Creates a buffer created on disk that can be written to.
 */
#ifndef _DISKBUFFER_H_
#define _DISKBUFFER_H_
#include <iostream>
#include <fstream>
#include <inttypes.h>

#include "Transmission.h"
#include "OutOfSeqCache.h"
#include "Mutex.h"

using namespace std;

/*! \class DiskBuffer
    \brief Represents a place to store large amounts of information.

   DiskBuffer opens a file on the disk and performs writes to the file.
   Allows one to Add data, Get the sequence number of the last processed packet,
   and has the ability to flush what is in memory to disk.
*/
class DiskBuffer {
	public:
		DiskBuffer(string &fileName, uint64_t fileSize, uint64_t nextSeq);
		virtual ~DiskBuffer();
		
		uint64_t		Add(Data &);
		uint64_t		GetNextSeq();
		uint32_t		GetWindowSize();
		void	 		Flush();
		
	private:
		//Mutex			mWriteLock;
		OutOfSeqCache	mOutOfSeqCache;
		
		fstream			mFile;
		std::string 	mFileName;
		uint64_t		mFileSize;
		uint64_t		mNextSeq;
		uint32_t		mWindowSize;
};
#endif
