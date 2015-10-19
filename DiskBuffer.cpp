/*
 * File: DiskBuffer.h
 * Class: ICS 451
 * Project #: 3
 * Team Members: Bryce Groff, Brandon Grant, Emiliano Miranda
 * Author: Bryce Groff
 * Created Date: 04-23-09
 * Desc: Creates a buffer created on disk that can be written to.
 */
#include <iostream>
#include <fstream>
#include <inttypes.h>
#include <sys/mman.h>

#include "DiskBuffer.h"

/*!
	\param fileName The filename that will be used to store the information.
	\param fileSize The total size of the file to be stored.
	\param nextSeq The starting sequence number.
*/
DiskBuffer::DiskBuffer(string &fileName, uint64_t fileSize, uint64_t nextSeq) 
	: mFileName(fileName), mFileSize(fileSize), mNextSeq(nextSeq)
{
	// Open the file for writing
	try {
		mFile.open(mFileName.c_str(), fstream::out);
	}
	catch (ios_base::failure &e) { 
		cerr<<"DiskBuffer [Constructor]: "<<e.what()<<endl;
	}
}

DiskBuffer::~DiskBuffer()
{
	mFile.close();
}

/*!
	\brief Adds data to the DiskBuffer. If the data is in order, the 
	data will be added and the out of order cache will be iterated over
	to find any data that may come next. If the data is out of order it will be
	put in the OutOfSeqCache
	\sa Data
	\param &packet The packet that you wish to add to the buffer.
    \return The amount of data saved to disk.
*/
uint64_t DiskBuffer::Add(Data &packet)
{
	uint64_t fSeqNum = packet.mSeqNum;
	uint32_t fSize = packet.mPacketSize;

	// If the sequence number is the number that we 
	// are expecting to receive then process the data.
	if (mNextSeq == fSeqNum) {
		// TODO: This can probably be finner grain.
		//mWriteLock.Lock();
	
		try {
			mFile.write(packet.mData, (streamsize)fSize);
		}
		catch (ios_base::failure &e) {
			cerr<<"DiskBuffer [Add]: "<<e.what()<<endl;
		}
	
		// Increment the current sequence number by the size of the packet. 
		// This should be the next sequence number. Then loop over the out 
		// of order cache and add as much data as we have.
		mNextSeq += fSize;
		Data *fNext = mOutOfSeqCache.GetData(mNextSeq);
		while (fNext != NULL) {
			// Write the data.
			mFile.flush();
			mFile.write(fNext->mData, (streamsize)fNext->mPacketSize);
			fSize += fNext->mPacketSize;
			
			// Increment the pointer
			mNextSeq += fNext->mPacketSize;

			// Free the memory.
			delete [] fNext->mData;
			delete fNext;

			fNext = mOutOfSeqCache.GetData(mNextSeq);
		}
	
		//mWriteLock.Unlock();
	}
	
	// If the sequence number is greater than the number we are expecting
	// then add the data to the out of order cache.
	else if (mNextSeq < fSeqNum) {
		// Lock so we are not adding data while trying to remove above.
		//mWriteLock.Lock();
		mOutOfSeqCache.Add(packet);
		//mWriteLock.Unlock();
		//fSize = 0;
	}

	// return how many bytes we saved to disk.
	return fSize;
}

/*!
	\brief Returns the next sequence number that DiskBuffer is expecting.
    \return mNextSeq
*/
uint64_t DiskBuffer::GetNextSeq()
{
	return mNextSeq;
}

/*!
	\brief Returns the window size to throttle back the sender.
    \return the window size
*/
uint32_t DiskBuffer::GetWindowSize()
{
	//TODO: Make this do something useful
	return 0xFFFFFFFF;
}

/*!
	\brief Flushes all the data that is in memory but not yet on the disk.
	\return Nothing
*/
void DiskBuffer::Flush()
{
	try {
		mFile.flush();
	}
	catch (ios_base::failure &e) {
		cerr<<"DiskBuffer [Flush]: "<<e.what()<<endl;
	}
}
