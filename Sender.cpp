/*
 *  Sender.cpp
 *
 */
#include "Sender.h"

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>
#include <fstream>

using namespace std;

#define error(s) { cerr<<"Error: "<<(s); exit(1); }
#define kSendDefaultTimeOut 200
#define kSendDefaultMss 4800 // 4 packets
#define kSendMaxMss 16800 // 14 packets
//#define kSendMaxMss 30000 // 25 packets
#define kSendSynSeqNum 0
#define kSendSynAckSeqNum 1
#define kSendDebug 0

void printUsage();

int main (int argc, char *argv[])
{
	char fileName[20];
	char ipAdd[16];
	uint32_t portNumI;
	unsigned short portNumS;
	struct sockaddr_in * recv;

        //confirm that required number of arguments are present
	if(argc != 4){
	  printUsage();
	  exit(1);
	}
	//check validity of filename and store
	if (!isRegExMatch(argv[1], kFileNameRegEx, kFileNameMaxChars)){
	  printUsage();
	  exit(1);
	}
	strcpy(fileName, argv[1]);

	//check validity of IP address and store
	strcpy(ipAdd, argv[2]);
	
	//check validity of port number and store
	portNumI = atoi(argv[3]);
	if (portNumI < kPortNumMin || portNumI > kPortNumMax){
	  printUsage();
	  exit(1);
	}
	portNumS = (unsigned short)portNumI;

	if ((recv = getHostAddress(ipAdd, portNumS)) == NULL){error("Unable to locate host");}

	Sender sender(fileName, recv);
	sender.Start();
}

/* Function: printUsage
 * Desc: This function displays the usage information on how this program should work.
 */
void printUsage()
{
	cout<<"Invalid command line arguments specified.\n\n";
	cout<<"User sust supply three arguments for program\n";
	cout<<"First Argument Must be filename to transfer cannot exceed 20 characters\n";
	cout<<"Sencond Argument must be a valid IP address of the receiver\n";
	cout<<"Third argument must be numeric port number that receiver is listening on\n";
	cout<<"Usage: relsend <filename> <ip> <port>\n";
	cout<<"\t<port> - A number between "<<kPortNumMin<<" and "<<kPortNumMax<<".\n";
}

/* Sets up opening the file and setting the mFileSize correctly. If the file is not found or
 * something goes wrong opening the file then we close the application. 
 * Also the constructor will setup the Send and Timer thread.
 */
Sender::Sender(char fileName[], sockaddr_in* recv)
	: mFileName(fileName), mRecv(recv), mLastAck(0), mEstRTT(0),
	  mSendThread(_StartSend, this), mTimerThread(_StartTimer, this),
	  mCurrentState(SEND_NO_CONN), mConnected(false), mFileOffset(0),
	  mWindowSize(kSendDefaultMss), mCongWin(kSendDefaultMss), mRecvWin(0),
	  mTheTimeout(200), mEstDEV(0), mRetransmit(false)
{
	try {
		mFile.open(mFileName.c_str(), ios::in | ios::binary);
		mFile.seekg(0, ios::end);
  		mFileSize = (uint64_t)mFile.tellg();
		mFile.seekg(0, ios::beg);
 	}
 	catch (ios_base::failure &e) {
	 	cerr<<"Open: "<<e.what();
	 	exit(-1);
 	}
	mTransTimer = new TransmissionTimer(mEstRTT, 0, (void*)this, Sender::_TimeOutCallBack);
}

Sender::~Sender()
{
	mFile.close();
}


// ***********************************************************************************

void Sender::Start()
{
	//initialize class members to values passed in as parameters

	// Set the expected sequence number we should have when are are done.
	mFinSeqNum = mFileSize + 2;
	mSock = _ConfigureSocket();
	mSendThread.Start();
	_StartListen();
}

void *Sender::_StartSend(void *args)
{
	// Args points to this object.
	Sender *fSender = (Sender*)args;

	if (fSender != NULL)
	{
		cout<<"Sending file '"<<fSender->mFileName<<"' to "<<inet_ntoa(fSender->mRecv->sin_addr)
			<<" on port "<<dec<<ntohs(fSender->mRecv->sin_port)<<"."<<endl;

		// Get the connection start time.
		gettimeofday(&(fSender->mConnStartTime), NULL);

		do
		{
			fSender->mSendLock.Lock();
			fSender->_SendCurrent();
			fSender->mTransTimer->Start(true, fSender->mTheTimeout);
			fSender->mSendLock.Wait();
			fSender->mSendLock.Unlock();
		}
		while (fSender->mConnected || (fSender->mCurrentState == SEND_NO_CONN));
	}
}

void Sender::_SendCurrent()
{
        gettimeofday(&mBegTimestamp, NULL);
	switch (mCurrentState)
	{
		case SEND_NO_CONN:
			_SendSyn();
			break;
		case SEND_DATA:
		case SEND_FIN:
			_SendData();
			break;
	}
}

void Sender::_SendSyn()
{
	uint32_t fSize = _BuildSynPacket();
	_SendPacket(fSize, true);
	// Start timeout thread.
}

void Sender::_SendData()
{
	streamsize fSize = 0;
	uint32_t fPacketSize = 0;
	// Send data from mFileOffset + mWindowSize or next nearest whole packet.

	// Check to see if our file offset is at the end of file.

	// Check the file position against where we are supposed to be sending
	// from. The member mFileOffset can be adjusted from ACKs received to
	// indicate the starting position of where data should be sent.
	streampos fFilePos = mFile.tellg();

	// Make sure we don't have an error condition.
	if ((int)fFilePos != -1 || ((int)fFilePos == -1 && mFile.eof() && (uint64_t)mFileOffset != mFileSize))
	{
		if (fFilePos != mFileOffset)
		{
		  if(!mFile.good()){
		    mFile.clear();
		  }
			// Update our stream position to match mFileOffset.
			mFile.seekg(mFileOffset, ios_base::beg);
		}

		if (kSendDebug)
		{
			cout<<"Window Size = "<<dec<<mWindowSize<<endl;
		}

		int packetCount = mWindowSize / kPacketSize;

		// Send each packet.
		for (int i = 0; (i < packetCount) && mFile.good(); i++)
		{
			// While the file is no an EOF or failure copy out chunks of kPacketSize
			// until there is no more data left.
			char fBuffer[kPacketSize - kDataPacketSize];

			// Send one packet worth of data from the file.
			mFile.read(fBuffer, kPacketSize - kDataPacketSize);
			fSize = mFile.gcount(); // See how many bytes we read.
			fPacketSize = _BuildDataPacket(fBuffer, (unsigned short)fSize);
			_SendPacket(fPacketSize, true);
		}
	}
	else if (!mFile.eof())
	{
		cout<<"An error occurred while getting file stream position. No packets sent."<<endl;
	}
	
	// Check if we need to send a FIN.
	if (mFile.eof())
	{
		// When we are done send a Fin.

		if (mCurrentState != SEND_FIN)
		{
			mCurrentState = SEND_FIN;
		}

		fPacketSize = _BuildFinPacket();
  		_SendPacket(fPacketSize, true);
	}
}

void Sender::_ParseAck()
{
	if(mMFBIn[0] == (char)ACK)
	{
		uint64_t fSeqNum = 0;
		uint32_t fOffset = 1;

		//extract info and test to make sure valid

		gettimeofday(&mEndTimestamp, NULL);
		// Get ACK #.
		if(tryGetULongFromMessage(mMFBIn + fOffset, (kMaxPacketSize - fOffset), &fSeqNum))
		{
			fOffset += kSeqNumByteSize;
			uint32_t fWinSize = kSendDefaultMss;

			// Get window size.
			if(tryGetUIntFromMessage(mMFBIn + fOffset, (kMaxPacketSize - fOffset), &fWinSize))
			{
				mRecvWin = fWinSize;

				// Check if we are receiving an ACK in response to data being sent.
				if ((mCurrentState == SEND_DATA && fSeqNum > kSendSynAckSeqNum) || (mCurrentState == SEND_FIN && fSeqNum < mFinSeqNum))
				{
					// Check if ACK # is in valid range.
					if (fSeqNum >= mSeqNumBase && fSeqNum <= (mSeqNumBase + mWindowSize))
					{
						// Update our file offset with the number of bytes ACKed.
						mFileOffset += fSeqNum - mSeqNumBase;

						// Update our base sequence number.
						mSeqNumBase = fSeqNum;
						mLastAck = fSeqNum;

						// Update RTT.
						_UpdateRTT(false);

						// Update window size.
						mCongWin += kPacketSize;
						mWindowSize = MIN(mCongWin, mRecvWin);

						// If for some reason window is less than packet size, reset it to packet size.
						if (mWindowSize < kPacketSize)
						{
							mWindowSize = kPacketSize;
						}
						else if (mWindowSize > kSendMaxMss)
						{
							mWindowSize = kSendMaxMss;
						}

						// Signal send thread.
						mSendLock.Signal();
					}
				}
				else if (mCurrentState == SEND_FIN && fSeqNum == mFinSeqNum)
				{
					// File was successfully transerred and acknowledged.
					mConnected = false;
					cout<<"File sent successfully!"<<endl;

					// Get the connection end time.
					gettimeofday(&mConnEndTime, NULL);
					double transTime = (((mConnEndTime.tv_sec * 1000000.0) + mConnEndTime.tv_usec) - ((mConnStartTime.tv_sec * 1000000.0) + mConnStartTime.tv_usec)) / 1000000.0;

					cout.precision(4);
					cout << "Time to send was " << dec << transTime << " seconds at a rate of " << (transTime / mFileSize) << " seconds per byte." << endl;

					// Just do quick and dirty exit for now.
					exit(EXIT_SUCCESS);
				}
				else if (mCurrentState == SEND_NO_CONN && (fSeqNum == kSendSynAckSeqNum || fSeqNum == kSendSynSeqNum))
				{
					//if the sequence number is 1 need to set connected to true
					if (fSeqNum == kSendSynAckSeqNum)
					{
						mSeqNumBase = fSeqNum;
						mLastAck = fSeqNum;
						mCurrentState = SEND_DATA;
						mConnected = true;
						//mTransTimer->Start(true);

						// Signal send thread.
						mSendLock.Signal();
					}
					//if seq number is zero then we have recieved nack shutdown
					else
					{
						cout<<"Receiver already has file. Shutting down."<<endl;
						exit(1);
					}
				}
				else if (kSendDebug)
				{
					cout<<"An unanticipated ACK was received. Ignoring."<<endl;
				}
			}
		}
	}
}

// ***********************************************************************************


void Sender::_StartListen()
{
	ssize_t fSize;

	//while (mLastAck < mFileSize + 2)
	while (mCurrentState == SEND_NO_CONN || mConnected)
	{
		fSize = _ReceivePacket();

		if (fSize > 0)
		{
			mSendLock.Lock();
		    _ParseAck();
			mSendLock.Unlock();
		}
	}
}

void *Sender::_StartTimer(void *args)
{
	//TODO: Can we use the TransmissionTimer that mills built here as well?
}

int32_t Sender ::_ConfigureSocket()
{
  int sock = -1;
  struct protoent* proEnt = NULL;
  struct sockaddr_in sin;
  struct sockaddr * sap = (sockaddr*)&sin;

  if((proEnt = getprotobyname("udp")) == NULL){ error("Error opening listen protocol");}
  if((sock = socket(PF_INET, SOCK_DGRAM, proEnt->p_proto)) < 0){ error("Error creating socket");}

  return sock;
}


/*Func: BuildSynPacket 
 *Desc: takes a char* as a parameter and begins inserting the required information
 *to fill SYN packet
 *Ret: returns the length in bytes that were inserted into the buffer
 */
uint32_t Sender::_BuildSynPacket()
{
  uint32_t fLength = 0;

  //insert identifier and increase length
  mMFBOut[0] = (char)SYN;
  fLength++;

  //insert the sequence number and increase length
  setULongToMessage(mMFBOut + fLength, (kMaxPacketSize - fLength), mLastAck);
  fLength += 8;

  //insert filesize and increase length
  setULongToMessage(mMFBOut + fLength, (kMaxPacketSize - fLength), mFileSize);
  fLength += 8;
  
  //insert filename into the buffer and increase length
  mFileName.copy(mMFBOut + fLength, mFileName.length());
  fLength += mFileName.length();

  //insert the 00 to terminate filename and increase length
  mMFBOut[fLength] = 0x00;
  fLength++;

  return fLength;
}

/*Func: BuildFinPacket
 *Desc: Fills a passed buffer with the info for a syn packet
 *Ret: returns the length in bytes inserted into the buffer
 */
uint32_t Sender::_BuildDataPacket(char buffer[], unsigned short dataSize)
{
	uint32_t fLength = 0;
  
	//insert selector byte and increase length and advance pointer
	mMFBOut[0] = (char)DATA;
	fLength++;

	//insert sequence number and increase length and advance pointer
	setULongToMessage(mMFBOut + fLength, (kMaxPacketSize - fLength), mLastAck);
	fLength += 8;

	setUShortToMessage(mMFBOut + fLength, (kMaxPacketSize - fLength), dataSize);
	fLength += 2;
  
	// Copy the file data into a packet.
	memcpy(mMFBOut + fLength, buffer, dataSize);
	fLength += dataSize;
  
	return fLength;
}


/*Func: BuildFinPacket
 *Desc: Fills a passed buffer with the info for a fin packet
 *Ret: returns the length in bytes inserted into the buffer
 */
uint32_t Sender::_BuildFinPacket()
{
  uint32_t fLength = 0;

  //insert selector byte and increase length and advance pointer
  mMFBOut[0] = (char)FIN;
  fLength++;
  
  //insert sequence number andd increase length and advance pointer
  //setULongToMessage(mMFBOut + fLength, (kMaxPacketSize - fLength), mLastAck);
  setULongToMessage(mMFBOut + fLength, (kMaxPacketSize - fLength), mFinSeqNum - 1);
  fLength += 8;
  
  return fLength;
}

void Sender::_SendPacket(uint32_t dataSize, bool print)
{
	sendPacket(mSock, mRecv, mMFBOut, dataSize, kSendDebug);
}

ssize_t Sender::_ReceivePacket()
{
	ssize_t fBytesRecv;
	uint32_t fAddrSize = sizeof(mRecv);
	fBytesRecv = recvfrom(mSock, mMFBIn, kMaxPacketSize, 0, (sockaddr*)mRecv, &fAddrSize);
	// TODO: Check if fBytesRecv > 0 or figure out what to do if that is the case.
	
	return fBytesRecv;
}


/*Func:_Retransmit
 *Desc: This function to be called whenever a timeout occurs in the timer class will determine where to retransmit data
 *Ret: n/a
 */
void Sender::_Retransmit()
{
	uint32_t fSize = 0;

	//lock and adjust the window size and update RTT
	if (mSendLock.TryLock() == 0)

	{
		if((mCongWin/2) < kPacketSize){
			mCongWin == kPacketSize;
		} 
		else{
			mCongWin /= 2;
		}

		mWindowSize = MIN(mCongWin, mRecvWin);

		_UpdateRTT(true);

		mSendLock.Signal();
		mSendLock.Unlock();
	}
}


void Sender::_TimeOutCallBack(void* caller)
{

  if(caller != NULL){
    Sender* sendr = (Sender*)caller;
    sendr->_Retransmit();

  }

}

void Sender::_UpdateRTT(bool flag)
{

  //if true called by timeout add 1 std dev to timeout interval
  if(flag){
	  mRetransmit = true;

	  int tempAdj = mEstDEV;

	  if (tempAdj < 0)
	  {
		  tempAdj *= -1;
	  }

    mTheTimeout += tempAdj;
  }
  //otherwise called by the parse ack update with calc
  else{
    if(!mRetransmit){
      mTheTimeout = calculateTimeOutInterval(&mEstRTT, &mEstDEV, &mBegTimestamp, &mEndTimestamp);

      if( mTheTimeout == 0){
	mTheTimeout = 200;
      }
    }

    else{
      mRetransmit = false;
    }
  }



}













