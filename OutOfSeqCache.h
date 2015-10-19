/*
 * File: OutOfSeqCache.h
 * Class: ICS 451
 * Project #: 3
 * Team Members: Bryce Groff, Brandon Grant, Emiliano Miranda
 * Author: Bryce Groff
 * Created Date: 04-23-09
 * Desc: Creates cache to hold out of order data.
 */
#ifndef _OUTOFSEQCACHE_H_
#define _OUTOFSEQCACHE_H_

#include <map>
#include <utility>

#include "Transmission.h"

using namespace std;

typedef map<uint64_t, Data*> DataMap;
 

/*! \class OutOfSeqCache
    \brief Represents a place to store packets that come in out of order.
    Allows one to Add data, Get the data that corrisponds to a sequence number.  
*/
 class OutOfSeqCache {
	public:
		OutOfSeqCache();
		virtual ~OutOfSeqCache();
		
		void		Add(Data);
		Data		*GetData(uint64_t);
	
	private:
		DataMap		mMap;
};
#endif
