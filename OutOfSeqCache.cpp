/*
 * File: OutOfSeqCache.h
 * Class: ICS 451
 * Project #: 3
 * Team Members: Bryce Groff, Brandon Grant, Emiliano Miranda
 * Author: Bryce Groff
 * Created Date: 04-23-09
 * Desc: Creates cache to hold out of order data.
 */
#include <map>
#include <utility>

#include "OutOfSeqCache.h"
#include "Transmission.h"

/*!
	\brief Constructor does nothing.
*/
OutOfSeqCache::OutOfSeqCache()
{
}

OutOfSeqCache::~OutOfSeqCache()
{
}

/*!
	\brief Adds data to the OutOfSeqCache. Remeber that you must free the data that 
	gets passed in to the OutOfSeqCache.
	\sa Data
	\param &packet The packet that you wish to add to the buffer. The data of the packet 
	must be on the heap.
    \return Nothing
*/
void OutOfSeqCache::Add(Data packet)
{	
	DataMap::iterator fIter = mMap.begin();
	fIter = mMap.find(packet.mSeqNum);

	// Make sure we didn't already add this packet.
	if (fIter == mMap.end())
	{
		// Make sure that we copy the data so that when the execution continues
		// running we do not loose the data.
		Data *fData = new Data();
		fData->mSeqNum = packet.mSeqNum;
		fData->mPacketSize = packet.mPacketSize;
		fData->mData = new char[packet.mPacketSize];
		memcpy(fData->mData, packet.mData, packet.mPacketSize);

		// Add the packet to the map, use the seqNum as a lookup.
		mMap.insert(pair<uint64_t, Data*>(packet.mSeqNum, fData));
	}
}

/*!
	\brief Iterates through the list of packet and either returns the 
	packet if we can find it, or return NULL if we cannot.
	\param &seqNum The sequence number to search for.
    \return The packet or NULL if the data was not found.
*/
Data *OutOfSeqCache::GetData(uint64_t seqNum)
{
	Data *fVal = NULL;
	
	// find will return an iterator to the matching element if it is found
	// or to the end of the map if the key is not found
	DataMap::iterator fIter = mMap.begin();
	fIter = mMap.find(seqNum);
	if( fIter != mMap.end() ) {
		fVal = fIter->second;
		mMap.erase(fIter);
	}

	// If we fall through return NULL.
	return fVal;
}
