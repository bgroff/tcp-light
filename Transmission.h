/*
 * File: Transmission.h
 * Class: ICS 451
 * Project #: 3
 * Team Members: Bryce Groff, Brandon Grant, Emiliano Miranda
 * Author: Bryce Groff, Emiliano Miranda
 * Created Date: 04-19-09
 * Desc: Program wide defines and utilities.
 */
#ifndef _TRANSMISSION_H_
#define _TRANSMISSION_H_

#define kPacketSize 1200
#define kAckPacketSize 13
#define kDataPacketSize 11
#define kPortNumMin 1
#define kPortNumMax 65535
#define kFileNameMaxChars 20
#define kMicroSecond 1000000
#define kRttEstDelta 3
#define kRttDevDelta 2
#define kSeqNumByteSize 8

// Valid file name characters are a-z and 0-9, and the file name may also contain a single ".".
#define kFileNameRegEx "^[0-9a-z]*\\.?[0-9a-z]*$"

#define MIN(x, y) ((x) < (y) ? (x) : (y))

#include <iostream>
#include <arpa/inet.h>
#include <fcntl.h>
#include <inttypes.h>
#include <math.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>

using namespace std;

/*! \class Data
    \brief Container class for holding packet data.  
*/
class Data {
	public:
		Data(){}
		Data(uint32_t seq, unsigned short size, char *data) 
			: mSeqNum(seq), mPacketSize(size), mData(data) {} //!< Adds the information to the object.
		~Data() {  }
		
		uint64_t		mSeqNum;
		unsigned short	mPacketSize;
		char			*mData;
};

enum MessageType {
	SYN = 0x55,
	ACK = 0x5A,
	DATA = 0xDD,
	FIN = 0x5F
};

struct SynPacket {
	uint8_t		code;
	uint64_t	seq;
	uint64_t	len;
	char		*name;
};

struct AckPacket {
	uint8_t		code;
	uint64_t	ack;
	uint32_t	window;
};

struct DataPacket {
	uint8_t		code;
	uint64_t	seq;
	uint16_t	len;
	char		*data;
};

struct FinPacket {
	uint8_t		code;
	uint64_t	seq;
};

bool tryGetUShortFromMessage(char* buff, int size, unsigned short* result);
bool tryGetUIntFromMessage(char* buff, int size, uint32_t* result);
bool tryGetULongFromMessage(char* buff, int size, uint64_t* result);
bool tryGetStringFromMessage(char* buff, int size, char* result, int resultSize);
bool setUShortToMessage(char* buff, int buffSize, uint16_t input);
bool setUIntToMessage(char* buff, int buffSize, uint32_t input);
bool setULongToMessage(char* buff, int buffSize, uint64_t input);
bool isEqualHost(struct sockaddr_in* host1, struct sockaddr_in* host2);
bool isRegExMatch(const char* str, const char* pattern, int maxChars);
void sendPacket(int sock, struct sockaddr_in* receiver, char* data, int size);
void sendPacket(int sock, struct sockaddr_in* receiver, char* data, size_t size, bool printPackets);
void spawnThread(pthread_t *thread, void *(*threadFunc)(void*), void *args);
struct sockaddr_in* getHostAddress(char* hostName, unsigned short port);
bool doesFileExist(char* fileName);
uint32_t calculateTimeOutInterval(uint32_t* estRtt, int* devRtt, struct timeval* sampleStartTime, struct timeval* sampleEndTime);

#endif
