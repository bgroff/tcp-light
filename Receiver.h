/*
 *  Receiver.h
 *
 */
#ifndef _RECEIVER_H_
#define _RECEIVER_H_

#include <iostream>
#include <time.h>

#include "Transmission.h"
#include "TransmissionTimer.h"
#include "DiskBuffer.h"
#include "Mutex.h"

using namespace std;

enum ReceiverState
{
	RECV_NO_CONN,
	RECV_DATA,
	RECV_FIN
};

class Receiver {
	public:
		Receiver(unsigned short port);
		virtual ~Receiver();
		
		void Start();
	
	private:
		int _ConfigureSocket(unsigned short port);
		void _StartRecvCheck();
		void _ParseMessage(struct sockaddr_in* senderAddr, char* buff, uint32_t size);
		void _ParseSyn(struct sockaddr_in* senderAddr, char* buff, uint32_t size, uint64_t seqNum);
		void _ParseData(struct sockaddr_in* senderAddr, char* buff, uint32_t size, uint64_t seqNum);
		void _ParseFin(struct sockaddr_in* senderAddr, char* buff, uint32_t size, uint64_t seqNum);
		void _BuildAckPacket(char packet[kAckPacketSize]);
		void _SendAck(bool isRetransmit);
		void _SetSenderAddr(struct sockaddr_in* senderAddr, bool copy);
		void _UpdateRtt();
		void _AckTimeOut();

		static void _TimeOutCallBack(void* caller);
				
		TransmissionTimer*	mTransTimer;
		DiskBuffer			*mDiskBuffer;
		Mutex				mPacketLock;
		
		ReceiverState		mCurrentState;
		bool				mIsStarted;
		struct timeval		mConnStartTime;
		struct timeval		mConnEndTime;
		struct timeval		mRttStartTime;
		struct timeval		mRttEndTime;
		unsigned short		mPort;
		struct sockaddr_in*	mSenderAddr;
		int					mSocket;
		uint64_t			mFileSize;
		uint64_t			mTotalReceived;
		uint64_t			mLastAck;
		string				mFileName;
		uint32_t			mEstRtt; // Estimated round trip time.
		int					mDevRtt; // Deviation of round trip time.
		uint32_t			mTimeOutInterval;
		bool				mLastAckRetransmit;
};
#endif
