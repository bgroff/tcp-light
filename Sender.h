/*
 *  Sender.h
 *
 */
#ifndef _SENDER_H_
#define _SENDER_H_

#define kMaxPacketSize 1200

#include <iostream>
#include <fstream>

#include "Mutex.h"
#include "Thread.h"
#include "Transmission.h"
#include "TransmissionTimer.h"

using namespace std;

enum SenderState
{
	SEND_NO_CONN,
	SEND_DATA,
	SEND_FIN
};

class Mutex;

/*! \class Sender
    \brief The main class of the sending application.

   Parses command line and opens a connection to the receiver. Starts
   worker threads for sending and receiving data, as well as handling timeouts
*/
class Sender {
	public:
		Sender(char fileName[], struct sockaddr_in* recv);
		virtual ~Sender();
		
		void Start();
	
	private:
		static void*	_StartSend(void *);
		static void*	_StartTimer(void *);
		void			_StartListen();
		void			_SendPacket(uint32_t, bool);
		ssize_t			_ReceivePacket();
		int32_t 		_ConfigureSocket();
		uint32_t 		_BuildSynPacket();
		uint32_t 		_BuildDataPacket(char buffer[], unsigned short dataSize);
		uint32_t 		_BuildFinPacket();
		void 			_ParseAck();
		void                    _Retransmit();
		void                    _UpdateRTT(bool flag);
		void			_SendCurrent();
		void			_SendSyn();
		void			_SendData();
		//void			_SendFin();

		static void             _TimeOutCallBack(void* caller);

		sockaddr_in		*mRecv;
		fstream			mFile;
		uint64_t        mFileSize;
		//uint64_t		mSeqNum; // Stores the current file offset.
		uint64_t		mLastAck;
		uint32_t		mWindowSize; // Stores currenet window size, which is min(mCongWin, mRecvWin)
		string     		mFileName;
		int32_t         mSock;
		bool            mConnected;
		char            mMFBOut[1200];
		char            mMFBIn[14];
		Mutex			mSendLock;
		Thread 			mSendThread;
  		Thread 			mTimerThread;
		TransmissionTimer*      mTransTimer;
		uint32_t                mEstRTT;
		int		                mEstDEV;
		bool                    mRetransmit;
		struct timeval          mBegTimestamp;
		struct timeval          mEndTimestamp;
		struct timeval		mConnStartTime;
		struct timeval		mConnEndTime;

		SenderState		mCurrentState;
		uint64_t		mSeqNumBase;
		uint64_t		mFinSeqNum;
		uint32_t		mCongWin;
		uint32_t		mRecvWin;
		uint32_t        mTheTimeout;
		streampos		mFileOffset;
};
#endif














































