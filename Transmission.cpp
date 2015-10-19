/*
 * File: Transmission.cpp
 * Class: ICS 451
 * Project #: 3
 * Team Members: Bryce Groff, Brandon Grant, Emiliano Miranda
 * Author: Emiliano Miranda
 * Created Date: 04-20-09
 * Desc: This file contains the definition for helper functions used
 * in the reliable transfer protocol for project 3.
 */

#include "Transmission.h"
#include <pthread.h>

// Private function prototypes.
uint32_t calculateEstimatedRtt(uint32_t estRtt, uint32_t sampleRtt);
int calculateDeviationRtt(int devRtt, uint32_t estRtt, uint32_t sampleRtt);
uint32_t getTimeDiffMilliSeconds(struct timeval* startTime, struct timeval* endTime);

ssize_t error_send(int s, const void *buf, size_t len, int flags, const struct sockaddr *to, socklen_t tolen);

/* Function: tryGetUShortFromMessage
 * Desc: This function attempts to convert the first two bytes from
 * the specified buffer into an unsigned 16-bit integer. The buffer is 
 * assumed to be arranged in network byte order. The result is copied
 * into the location of memory where the result pointer parameter is
 * addressed to. If true is returned, then the result was successfully
 * copied. Otherwise, faise is returned if not.
 */
bool tryGetUShortFromMessage(char* buff, int size, unsigned short* result)
{
	bool success = false;

	if (buff != NULL && result != NULL && size >= 2)
	{
		*result = (unsigned char)buff[0] << 8 | (unsigned char)buff[1];
		success = true;
	}

	return success;
}

/* Function: tryGetUIntFromMessage
 * Desc: This function attempts to convert the first four bytes from
 * the specified buffer into an unsigned 32-bit integer. The buffer is 
 * assumed to be arranged in network byte order. The result is copied
 * into the location of memory where the result pointer parameter is
 * addressed to. If true is returned, then the result was successfully
 * copied. Otherwise, faise is returned if not.
 */
bool tryGetUIntFromMessage(char* buff, int size, uint32_t* result)
{
	bool success = false;

	if (buff != NULL && result != NULL && size >= 4)
	{
		*result = (unsigned char)buff[0] << 24 | (unsigned char)buff[1] << 16 | (unsigned char)buff[2] << 8 | (unsigned char)buff[3];
		success = true;
	}

	return success;
}

/* Function: tryGetULongFromMessage
 * Desc: This function attempts to convert the first eight bytes from
 * the specified buffer into an unsigned 64-bit integer. The buffer is 
 * assumed to be arranged in network byte order. The result is copied
 * into the location of memory where the result pointer parameter is
 * addressed to. If true is returned, then the result was successfully
 * copied. Otherwise, faise is returned if not.
 */
bool tryGetULongFromMessage(char* buff, int size, uint64_t* result)
{
	bool success = false;

	if (buff != NULL && result != NULL && size >= 8)
	{
		uint32_t t1 = (unsigned char)buff[0] << 24 | (unsigned char)buff[1] << 16 | (unsigned char)buff[2] << 8 | (unsigned char)buff[3];
		uint32_t t2 = (unsigned char)buff[4] << 24 | (unsigned char)buff[5] << 16 | (unsigned char)buff[6] << 8 | (unsigned char)buff[7];
		*result = ((uint64_t)t1 << 32) | (uint64_t)t2;
		success = true;
	}

	return success;
}

/* Function: tryGetStringFromMessage
 * Desc: This function will copy a null terminated string out of the specified 
 * buffer into the specified result string. The return value indicates if all 
 * of the string from the buffer was copied into the result string. Only true
 * is returned if the entire string was copied. If false is returned, it should
 * be assumed either no string or an incomplete string was copied into the 
 * result.
 */
bool tryGetStringFromMessage(char* buff, int size, char* result, int resultSize)
{
	bool success = false;

	if (buff != NULL && result != NULL && size > 0 && resultSize > 0)
	{
		char val;
		int i;

		for (i = 0; i < resultSize; i++)
		{
			val = buff[i];
			result[i] = val;

			// Check if we reached end of string.
			if (val == '\0')
			{
				break;
			}
		}

		// Check if we reached end of string before running out of space to store in result.
		if (val == '\0')
		{
			success = true;
		}
		// Otherwise, we will properly terminate the string, but ensure success is false since
		// we didn't copy everything.
		else
		{
			result[i] = '\0';
		}
	}

	return success;
}

/* Function: setUIntToMessage
 * Desc: Copies the specified uint32_t input value to the buff parameter in
 * network byte order. A return value of true means the input was copied. A 
 * return value of false means it was not copied.
 */
bool setUShortToMessage(char* buff, int buffSize, uint16_t input)
{
bool success = false;

	if (buff != NULL && buffSize >= 4)
	{
		buff[0] = (input >> 8) & 0xFF;
		buff[1] = input & 0xFF;
		success = true;
	}
	
	return success;
}

/* Function: setUIntToMessage
 * Desc: Copies the specified uint32_t input value to the buff parameter in
 * network byte order. A return value of true means the input was copied. A 
 * return value of false means it was not copied.
 */
bool setUIntToMessage(char* buff, int buffSize, uint32_t input)
{
	bool success = false;

	if (buff != NULL && buffSize >= 4)
	{
		buff[0] = (input >> 24) & 0xFF;
		buff[1] = (input >> 16) & 0xFF;
		buff[2] = (input >> 8) & 0xFF;
		buff[3] = input & 0xFF;
		success = true;
	}

	return success;
}

/* Function: setULongToMessage
 * Desc: Copies the specified uint64_t input value to the buff parameter in
 * network byte order. A return value of true means the input was copied. A 
 * return value of false means it was not copied.
 */
bool setULongToMessage(char* buff, int buffSize, uint64_t input)
{
	bool success = false;

	if (buff != NULL && buffSize >= 8)
	{
		buff[0] = (input >> 56) & 0xFF;
		buff[1] = (input >> 48) & 0xFF;
		buff[2] = (input >> 40) & 0xFF;
		buff[3] = (input >> 32) & 0xFF;
		buff[4] = (input >> 24) & 0xFF;
		buff[5] = (input >> 16) & 0xFF;
		buff[6] = (input >> 8) & 0xFF;
		buff[7] = input & 0xFF;
		success = true;
	}

	return success;
}

/* Function: isEqualHost
 * Desc: Determines if two specified IPv4 socket addresses are equal and returns
 * true if they are or false if not.
 */
bool isEqualHost(struct sockaddr_in* host1, struct sockaddr_in* host2)
{
	bool isEqual = false;

	if (host1 != NULL && host2 != NULL)
	{
		if (host1->sin_family == host2->sin_family && host1->sin_port == host2->sin_port && host1->sin_addr.s_addr == host2->sin_addr.s_addr)
		{
			isEqual = true;
		}
	}

	return isEqual;
}

/* Function: isRegExMatch
 * Desc: This function uses regular expressions to check a specified string
 * against a specified pattern. A value of true is returned if a match was 
 * found; otherwise, a value of false is returned if no match was found.
 * The maxChars parameter is used to specify the maximum number of
 * characters (without null terminator) allowed in the str parameter. If 
 * the value is set to -1, then it is assumed there is no limit.
 *
 * EXAMPLE USAGE:
 *	if (isRegExMatch("foo.txt", kFileNameRegEx, kFileNameMaxChars))
 *	{
 *		fprintf(stdout, "match\n");
 *	}
 *	else
 *	{
 *		fprintf(stdout, "no match\n");
 *	}
 */
bool isRegExMatch(const char* str, const char* pattern, int maxChars)
{
    bool isMatch = false;
    regex_t re;

	if (str != NULL && pattern != NULL && (maxChars == -1 || strlen(str) <= maxChars))
	{
		/* Setup pattern and other data need for comparison. */
		if (regcomp(&re, pattern, REG_EXTENDED | REG_NOSUB) == 0)
		{
			/* Do the actual comparison. */
			if (regexec(&re, str, 0, NULL, 0) == 0)
			{
				isMatch = true;
			}

			regfree(&re);
		}
	}

    return isMatch;
}

/* Function: sendData
 * Desc: This function sends the specified data buffer to the specified socket. The
 * receiver paramater should be a valid IPv4 address.
 */
void sendPacket(int sock, struct sockaddr_in* receiver, char* data, int size)
{
	sendPacket(sock, receiver, data, size, false);
}

/* Function: sendData
 * Desc: This function sends the specified data buffer to the specified socket. The
 * receiver paramater should be a valid IPv4 address.
 */
void sendPacket(int sock, struct sockaddr_in* receiver, char* data, size_t size, bool printPackets)
{
	if (sock >= 0 && receiver != NULL && data != NULL && size > 0)
	{
		// Check if we should print the packet about to be sent.
		if (printPackets)
		{
			cout<<"Sending data to "<<inet_ntoa(receiver->sin_addr)<<" on port "<<dec<<ntohs(receiver->sin_port)<<endl;
			cout<<"Packet: ";

			for (int i = 0; i < size; i++)
			{
				// Print individual byte (i.e. ff or 5a).
				cout<<hex<<int((unsigned char)(data[i]))<<" ";
			}
			
			cout<<endl<<endl;
		}

		// Send the packet.
		sendto(sock, data, size, 0, (struct sockaddr*)receiver, sizeof(*receiver));
		//error_send(sock, data, size, 0, (struct sockaddr*)receiver, sizeof(*receiver));
	}
}

/* Function: getHostAddress
 * Desc: This function returns a pointer to a sockaddr_in stucture containing
 * the data required to send to a host based on the specified host name / IP
 * address and port. If the structure cannot be created, then NULL is returned.
 * The caller of this function is responsible for freeing the memory allocated
 * in the returned pointer.
 */
struct sockaddr_in* getHostAddress(char* hostName, unsigned short port)
{
	struct sockaddr_in* sin = NULL;
	struct hostent* hostEnt = gethostbyname(hostName);

	if (hostEnt != NULL && hostEnt->h_addr_list != NULL)
	{
		sin = new struct sockaddr_in;
		memset(sin, 0, sizeof(*sin)); /* Zero all bits in sin. */
		sin->sin_family = AF_INET;
		memcpy(&(sin->sin_addr), hostEnt->h_addr_list[0], hostEnt->h_length);
		sin->sin_port = htons(port);
	}
	else
	{
		cerr<<"Error getting host entry";
	}

	return sin;
}

/* Function: doesFileExist
 * Desc: This function determines if the specified file exists. The
 * fileName parameter must be a valid string. If true is returned, 
 * the file exists; otherwise, if false is returned it does not.
 */
bool doesFileExist(char* fileName)
{
	bool exists = false;

	if (fileName != NULL)
	{
		struct stat buff;

		if (stat(fileName, &buff) == 0)
		{
			exists = true;
		}
	}

	return exists;
}

/* Function: calculateTimeOutInterval
 * Desc: This function calculates the time out interval in milliseconds based off of
 * the current estimated round trip time and deviation. The time out interval 
 * calculation is based on the TCP method. The values in the memory location where
 * the estRTT and devRTT pointers point to will be updated in this function. If any
 * invalid parameters are passed in, the value 0 is returned without changes to
 * estRtt or devRtt pointer memory locations.
 */
uint32_t calculateTimeOutInterval(uint32_t* estRtt, int* devRtt, struct timeval* sampleStartTime, struct timeval* sampleEndTime)
{
	uint32_t timeOut = 0;

	if (estRtt != NULL && devRtt != NULL && sampleStartTime != NULL && sampleEndTime != NULL)
	{
		if (*estRtt > 0)
		{
			//cout<<"Est RTT: "<<dec<<*estRtt<<endl;
			uint32_t sampleRtt = getTimeDiffMilliSeconds(sampleStartTime, sampleEndTime);
			uint32_t newEstRtt = calculateEstimatedRtt(*estRtt, sampleRtt);
			int newDevRtt = calculateDeviationRtt(*devRtt, *estRtt, sampleRtt);
			int estAdj = newDevRtt * 4;

			if (estAdj < 0 && (newEstRtt + estAdj) > newEstRtt)
			{
				timeOut = newEstRtt / 2;
			}
			else
			{
				timeOut = newEstRtt + estAdj;
			}

			*estRtt = newEstRtt;
			*devRtt = newDevRtt;
			/*cout<<"Sample RTT: "<<dec<<sampleRtt<<endl;
			cout<<"New Estimated RTT: "<<dec<<newEstRtt<<endl;
			cout<<"New Deviation RTT: "<<dec<<newDevRtt<<endl;*/
		}
		else
		{
			*estRtt = getTimeDiffMilliSeconds(sampleStartTime, sampleEndTime);
			*devRtt = 0;
			timeOut = *estRtt * 2;
		}
	}

	//cout<<"Timeout: "<<dec<<timeOut<<endl;

	return timeOut;
}

/* Function: calculateEstimatedRtt
 * Desc: This function calculates the estimated round trip time. It requires the
 * current estimated RTT value and current sample RTT. The new estimated round
 * trip time is returned.
 */
uint32_t calculateEstimatedRtt(uint32_t estRtt, uint32_t sampleRtt)
{
	uint32_t estAdj = sampleRtt >> kRttEstDelta; // May lose accuracy.

	return (estRtt - (estRtt >> kRttEstDelta)) + estAdj;
}

/* Function: calculateDeviationRtt
 * Desc: This function calculates the new deviation between an estimated round trip
 * time and the current sample.
 */
int calculateDeviationRtt(int devRtt, uint32_t estRtt, uint32_t sampleRtt)
{
	int devAdj = 0;

	if (sampleRtt > estRtt)
	{
		devAdj = (sampleRtt - estRtt) >> kRttDevDelta;
	}
	else
	{
		devAdj = (estRtt - sampleRtt) >> kRttDevDelta;
	}

	//uint32_t devAdj = ((uint32_t)fabs(((double)sampleRtt - estRtt))) >> kRttDevDelta;
	//cout<<"sampleRtt - estRtt: "<<dec<<(sampleRtt - estRtt)<<endl;
	//cout<<"sampleRtt - estRtt double: "<<dec<<(double)(sampleRtt - estRtt)<<endl;
	/*cout<<"devAdj: "<<dec<<devAdj<<endl;
	cout<<"devRtt: "<<dec<<devRtt<<endl;*/

	return (devRtt - (devRtt >> kRttDevDelta)) - devAdj;

	//double devAdj = fabs((double)(sampleRtt - estRtt)) * 0.25;

	//return (devRtt - (devRtt >> kRttDevDelta)) - devAdj;
}

/* Function: getTimeDiffMilliSeconds
 * Desc: This function returns the difference between two timeval structures in
 * milliseconds.
 */
uint32_t getTimeDiffMilliSeconds(struct timeval* startTime, struct timeval* endTime)
{
	uint32_t rttMilliSecs = 0;

	if (startTime != NULL && endTime != NULL)
	{
		rttMilliSecs = (uint32_t)((((endTime->tv_sec * kMicroSecond) + endTime->tv_usec) - ((startTime->tv_sec * kMicroSecond) + startTime->tv_usec)) / 1000);
		//cout<<dec<<(uint32_t)(((endTime->tv_sec * kMicroSecond) + endTime->tv_usec) - ((startTime->tv_sec * kMicroSecond) + startTime->tv_usec))<<endl;
	}

	return rttMilliSecs;
}




/* call sendto or drop the packet.  Packets are only dropped if
 * DROP_COUNT > 0, in which case (approximately or exactly) one out
 * of every DROP_COUNT packets is dropped.
 *
 * if PREDICTABLE_DROP == 1, one packet is dropped after every
 *   DROP_COUNT - 1 packets are sent, so the drop fraction is 1/DROP_COUNT
 * if PREDICTABLE_DROP == 0, a random number generator is used to drop
 *   approximately 1/DROP_COUNT packets.  In this case, the random
 *   number sequence will be the same every time the program is run,
 *   unless IRREPRODUCIBLE_SEQUENCE is defined.
 *
 * if #DEBUG is defined, packet drops are printed.
 */

/* Copyright (C) 2007, esb@hawaii.edu.  This software is released under
 * the GNU General Public Licence, version 3 or later, normally available
 * from http://www.gnu.org/licenses/    This program is distributed 
 * WITHOUT ANY WARRANTY whatsoever -- see the license for more details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>

#define	DROP_COUNT 5		/* change this to drop packets */
#define	PREDICTABLE_DROP 0	/* this specifies predictable or random drop */
/*
#define	IRREPRODUCIBLE_SEQUENCE // when defined, every run has different drops
*/

//#define DEBUG                   // when defined, prints when dropping a packet

#ifdef IRREPRODUCIBLE_SEQUENCE
#include <sys/time.h>
#include <time.h>
#endif /* IRREPRODUCIBLE_SEQUENCE */

ssize_t error_send (int s, const void *buf, size_t len, int flags,
		    const struct sockaddr *to, socklen_t tolen) {
  static int count_since_last_drop = 0;
  int drop_packet;
#ifdef IRREPRODUCIBLE_SEQUENCE
  static int initialized = 0;

  if (! initialized) {
    unsigned short seedv [3];
    struct timeval now;

    gettimeofday (&now, NULL);
    seedv [0] = now.tv_usec / 0x10000;
    seedv [1] = now.tv_sec % 0x10000;
    seedv [2] = now.tv_sec / 0x10000;
    seed48 (seedv);		/* initialize random number generator */
    initialized = 1;
  }
#endif /* IRREPRODUCIBLE_SEQUENCE */

  drop_packet = 0;
  if (DROP_COUNT != 0) {
    ++count_since_last_drop;
    if (PREDICTABLE_DROP) {
      drop_packet = (count_since_last_drop >= DROP_COUNT);
    } else {			/* random drop */
      drop_packet = (drand48 () < (1.0 / ((double) DROP_COUNT)));
    }
    if (drop_packet) {
#ifdef DEBUG
      printf ("dropping packet, count reached %d\n", count_since_last_drop);
#endif /* DEBUG */
      count_since_last_drop = 0;
      errno = EAGAIN;		/* resource temporarily unavailable */
      return -1;
    } /* else send packet */
  }
  return sendto(s, buf, len, flags, to, tolen);
}
