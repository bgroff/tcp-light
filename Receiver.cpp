/*
 * File: Receiver.cpp
 * Class: ICS 451
 * Project #: 3
 * Team Members: Bryce Groff, Brandon Grant, Emiliano Miranda
 * Author: Emiliano Miranda
 * Created Date: 04-20-09
 * Desc: This file contains the definition and entry point for the Receiver class
 * and relrecv program.
 */

#include <errno.h>
#include <pthread.h>
#include "Receiver.h"

#define kRecvDebug 0
#define kSynTimeOut 1 // Timeout SYN after 1 second.
#define kRecvMinPacketSize 9
#define kSeqNumSize 8
#define kRecvDefaultTimeOut 100
#define kRecvFinTimeOut 1000
#define error(s) { perror(s); exit(1); }

void printUsage();

/* Function: main
 * Desc: This function is the main entry point for the relrecv program.
 */
int main (int argc, char *argv[])
{
	// Check to see if we have correct # of arguments.
	if (argc == 2)
	{
		int port = atoi(argv[1]);

		if (port >= kPortNumMin && port <= kPortNumMax)
		{
			Receiver receiver((unsigned short)port);
			receiver.Start();
		}
		else
		{
			printUsage();
		}
	}
	else
	{
		printUsage();
	}

	exit(EXIT_SUCCESS);
}

/* Function: printUsage
 * Desc: This function displays the usage information on how this program should work.
 */
void printUsage()
{
	cout<<"Invalid command line arguments specified.\n\n";
	cout<<"You must supply one numeric argument, which denotes the port number.\n";
	cout<<"Usage: relrecv <port>\n";
	cout<<"\t<port> - A number between "<<kPortNumMin<<" and "<<kPortNumMax<<"."<<endl;
}

/* Function (ctor): Receiver 
 * Desc: This constructor initializes a new instance of the Receiver class. The port
 * number must be specified for the receiver to listen on to accept a file.
 */
Receiver::Receiver(unsigned short port) 
	: mPort(port), mCurrentState(RECV_NO_CONN), mSocket(-1), mFileSize(1), mTotalReceived(0),
	  mLastAck(0), mIsStarted(false), mSenderAddr(NULL), mDiskBuffer(NULL), 
	  mTimeOutInterval(kRecvDefaultTimeOut), mLastAckRetransmit(false),
	  mEstRtt(0), mDevRtt(0)
{
	// Temporarily use hard coded timeout of 100ms.
	mTransTimer = new TransmissionTimer(mTimeOutInterval, kTransmissionTimerInfiniteInterval, (void*)this, Receiver::_TimeOutCallBack);
}

/* Function (dtor): Receiver 
 * Desc: This destructor frees memory allocated by the Receiver class.
 */
Receiver::~Receiver()
{
	delete mSenderAddr;
	delete mTransTimer;
	delete mDiskBuffer;
}

/* Function: Start
 * Desc: This function starts the receiver to begin listening for a file transfer.
 */
void Receiver::Start()
{
	if (!mIsStarted)
	{
		int sock = _ConfigureSocket(mPort);

		if (sock > -1)
		{
			mIsStarted = true;
			mSocket = sock;
			_StartRecvCheck();
		}
		else
		{
			cerr<<"Could not bind the socket"<<endl;
		}

		if (mDiskBuffer != NULL)
		{
			// Make sure that nothing is left in the buffer.
			mDiskBuffer->Flush();
		}
	}
}

/* Function: _ConfigureSocket
 * Desc: This function creates and initializes a socket that can be used
 * by the receiver for accepting a file.
 */
int Receiver::_ConfigureSocket(unsigned short port)
{
	int sock = -1;
	struct protoent* protEnt = NULL;
	struct sockaddr_in sin;
	struct sockaddr* sap = (struct sockaddr*)&sin;

	if ((protEnt = getprotobyname("udp")) == NULL) 
	{
		error("Error opening listen protocol");
	}

	if ((sock = socket(PF_INET, SOCK_DGRAM, protEnt->p_proto)) < 0)
	{
		error("Error creating listen socket");
	}

	memset(&sin, 0, sizeof(sin)); // Zero all bits in sin.
	sin.sin_family = AF_INET;
	sin.sin_port = htons(port);
	sin.sin_addr.s_addr = INADDR_ANY;

	if (bind(sock, sap, sizeof(sin)) != 0)
	{
		error("Error binding listen socket");
	}

	return sock;
}

/* Function: _StartRecvCheck
 * Desc: This function starts the receiver listen loop to receive a file from a
 * sender.
 */
void Receiver::_StartRecvCheck()
{
	int bytesRead = 0;
	char buff[kPacketSize];
	struct sockaddr_in* senderAddrIn = new struct sockaddr_in;
	struct sockaddr* senderAddr = (struct sockaddr*)senderAddrIn;
	socklen_t sockAddrInSize = sizeof(*senderAddrIn);

	cout<<"Waiting to receive file..."<<endl;

	while (this->mIsStarted)
	//while (this->mIsStarted && this->mLastAck < this->mFileSize) // This is not entirely correct.
	{
		mPacketLock.Lock();

		if (kRecvDebug)
		{
			cout<<"Starting to listen on port "<<dec<<mPort<<" with socket descriptor "<<mSocket<<endl;
			cout<<mTotalReceived<<" bytes out of "<<mFileSize<<" bytes received. Sequence # = "<<mLastAck<<endl;
		}

		mPacketLock.Unlock();

		bytesRead = recvfrom(mSocket, buff, kPacketSize, 0, senderAddr, &sockAddrInSize);

		mPacketLock.Lock();

		if (bytesRead > 0)
		{
			// Check if we are in debug mode to print the packet.
			if (kRecvDebug)
			{
				cout<<"Received data from "<<inet_ntoa(senderAddrIn->sin_addr)
					<<" on port "<<dec<<ntohs(senderAddrIn->sin_port)<<".\n"
					<<"Packet: ";

				for (int i = 0; i < bytesRead; i++)
				{
					// Print individual byte (i.e. ff or 5a).
					cout<<hex<<int((unsigned char)buff[i])<<" ";
				}

				cout<<endl<<endl;
			}

			// Everything looks okay initially, so let's parse this packet.
			_ParseMessage(senderAddrIn, buff, bytesRead);	
		}
		else if (bytesRead == -1)
		{
			cerr<<"Error receiving data on listen socket: "<<mSocket<<endl;
		}

		mPacketLock.Unlock();
	}

	if (senderAddrIn != NULL)
	{
		delete senderAddrIn;
	}
}

/* Function: _ParseMessage
 * Desc: This function parses a UDP packet received and takes an appropriate action
 * based on its validity.
 */
void Receiver::_ParseMessage(struct sockaddr_in* senderAddr, char* buff, uint32_t size)
{
	// All packets must have at least 9-bytes to be valid.
	// 1 byte for message type.
	// 8 bytes for sequence number.
	if (size >= kRecvMinPacketSize)
	{
		char msg = buff[0];
		uint64_t seqNum = 0;
		int offset = 1;

		if (tryGetULongFromMessage(buff + offset, size - offset, &seqNum))
		{
			offset += kSeqNumSize; // Init offset with num of bytes seq # is.

			if (msg == (char)SYN)
			{
				_ParseSyn(senderAddr, buff + offset, size - offset, seqNum);
			}
			else if (msg == (char)DATA)
			{
				_ParseData(senderAddr, buff + offset, size - offset, seqNum);
			}
			else if (msg == (char)FIN)
			{
				_ParseFin(senderAddr, buff + offset, size - offset, seqNum);
			}
		}
	}
}

/* Function: _ParseSyn
 * Data: This function parses a SYN packet from the sender and takes an appropriate action
 * based on its validity.
 */
void Receiver::_ParseSyn(struct sockaddr_in* senderAddr, char* buff, uint32_t size, uint64_t seqNum)
{
	// Check if valid SYN and if we are in correct state.

	// Check if we haven't received a SYN yet.
	if (mCurrentState == RECV_NO_CONN)
	{
		// Verify rest of packet is correct format.
		uint64_t fileSize = 0;

		// Get total size of file.
		if (tryGetULongFromMessage(buff, size, &fileSize))
		{
			int offset = sizeof(uint64_t); // Init offset with num of bytes file size is.

			// Allocate 2 extra bytes. One for \0 (since I'm assuming max chars is just the name alone) and one for appending 'r' to front of file name.
			char fileName[kFileNameMaxChars + 2];

			// Get file name and verify it is in a valid format.
			// We will copy the name of the file into the fileName char array starting at index 1. This will leave
			// room at index 0 for 'r' to be inserted. After copying the name into our char array, we will verify
			// it is in the correct format with the isRegExMatch call.
			if (tryGetStringFromMessage(buff + offset, size - offset, fileName + 1, kFileNameMaxChars + 1) // kFileNameMaxChars + 1 = name plus \0
				&& isRegExMatch(fileName + 1, kFileNameRegEx, kFileNameMaxChars)) 
			{
				fileName[0] = 'r';

				/*if (kRecvDebug)
				{
					cout<<"Received SYN for file '"<<fileName<<"'."<<endl;
				}*/

				cout<<"Receiving file '"<<fileName<<"' from "<<inet_ntoa(senderAddr->sin_addr)
					<<" on port "<<dec<<ntohs(senderAddr->sin_port)<<"."<<endl;

				if (!doesFileExist(fileName))
				{
					// We have everything we need from the SYN.

					if (kRecvDebug)
					{
						cout<<"Sending ACK..."<<endl;
					}

					// Get the connection start time.
					gettimeofday(&mConnStartTime, NULL);

					// Get the initial RTT start time.
					gettimeofday(&mRttStartTime, NULL);

					// Save sender data.
					_SetSenderAddr(senderAddr, true);

					// Save file name.
					mFileName.assign(fileName);

					// Save total file size.
					mFileSize = fileSize;
					mTotalReceived = 0;

					// Save sequence # + 1.
					mLastAck = seqNum + 1;

					// Set our state to data mode.
					mCurrentState = RECV_DATA;

					// Setup the DiskBuffer
					mDiskBuffer = new DiskBuffer(mFileName, mFileSize, mLastAck);

					// Start SYN timeout thread.
					mTransTimer->Start(true);
				}
				else
				{
					if (kRecvDebug)
					{
						cout<<"File already exists. Sending NACK..."<<endl;
					}

					// Get the connection start time.
					gettimeofday(&mConnStartTime, NULL);

					// Get the initial RTT start time.
					gettimeofday(&mRttStartTime, NULL);

					mLastAck = seqNum;
					this->mIsStarted = false;
				}

				// Send ACK.
				_SendAck(false);
			}
		}
	}
	// Check if we received a SYN previously (indicated by being in RECV_DATA state and 0 bytes received).
	else if (mCurrentState == RECV_DATA && mTotalReceived == 0 && isEqualHost(mSenderAddr, senderAddr))
	{
		// Reset SYN timeout thread.
		this->mTransTimer->Start(true);

		// Resend ACK.
		_SendAck(true);
	}
}

/* Function: _ParseData
 * Data: This function parses a DATA packet from the sender and takes an appropriate action
 * based on its validity.
 */
void Receiver::_ParseData(struct sockaddr_in* senderAddr, char* buff, uint32_t size, uint64_t seqNum)
{
	// Check if we are talking to the right host.
	if (isEqualHost(mSenderAddr, senderAddr))
	{
		// Check if we are in the right state and if the ack # matches up
		// with what we expect.
		//if (mCurrentState == RECV_DATA && mLastAck == seqNum)
		if (mCurrentState == RECV_DATA)
		{
			unsigned short fLength = 0;

			// Get length of data.
			if (tryGetUShortFromMessage(buff, size, &fLength))
			{
				int offset = 2;

				// Create object to hold data we received.
				Data data(seqNum, fLength, buff + offset);

				// Add the data.
				if (mDiskBuffer->Add(data) > 0)
				{
					//mTotalReceived += fLength;
					uint64_t buffAck = mDiskBuffer->GetNextSeq();

					// Check if the data was added in order. If it wasn't
					// then the ack number in the disk buffer will be the
					// same ack we have in this receiver object.
					if (mLastAck < buffAck)
					{
						mTotalReceived += (buffAck - mLastAck);

						// Update our ack number since we received data in order.
						mLastAck = buffAck;

						// Update RTT.
						// Only update RTT for packets that have been transmitted once.
						_UpdateRtt();

						// Start timer.
						mTransTimer->Start(true, mTimeOutInterval);

						_SendAck(false);
					}
				}
			}
		}
		/*else
		{
			cout << "Data was received out of order." << endl;
		}*/
	}
}

/* Function: _ParseFin
 * Desc: This function parses a FIN packet from the sender and takes an appropriate action
 * based on its validity.
 */
void Receiver::_ParseFin(struct sockaddr_in* senderAddr, char* buff, uint32_t size, uint64_t seqNum)
{
	// Check if we are talking to the right host.
	if (isEqualHost(mSenderAddr, senderAddr))
	{
		// Check if we are in the right state and if the ack # matches up
		// with what we expect.
		if (mCurrentState == RECV_DATA && mLastAck == seqNum)
		{
			// Validate file.
			if (mTotalReceived == mFileSize)
			{
				// Save sequence # + 1.
				mLastAck = seqNum + 1;

				// Change our state to FIN.
				mCurrentState = RECV_FIN;
				
				// Set timeout for 1 second, then shut down.
				mTransTimer->Start(true, kRecvFinTimeOut);

				// Send ACK.
				_SendAck(false);

				// Flush any bufferred data to disk.
				if (mDiskBuffer != NULL)
				{
					mDiskBuffer->Flush();
				}

				// Get the connection end time.
				gettimeofday(&mConnEndTime, NULL);
				double transTime = (((mConnEndTime.tv_sec * 1000000.0) + mConnEndTime.tv_usec) - ((mConnStartTime.tv_sec * 1000000.0) + mConnStartTime.tv_usec)) / 1000000.0;

				cout.precision(4);
				cout << "File received successfully!" << endl;
				cout << "Time to receive was " << dec << transTime << " seconds at a rate of " << (transTime / mTotalReceived) << " seconds per byte." << endl;
				cout << "Terminating in " << dec << (kRecvFinTimeOut / 1000) << " second..." << endl;
			}
			else
			{
				cout << "An error occurred while validiating the received file. The expected number of bytes received does not "
					<< "match the actual number of bytes received." << endl;
				cout << "Expected: " << dec << mFileSize << " bytes." << endl;
				cout << "Received: " << dec << mTotalReceived << " bytes." << endl;
			}
		}
		else if (mCurrentState == RECV_FIN)
		{
			mTransTimer->Start(true, kRecvFinTimeOut);

			// Resend ACK.
			_SendAck(true);
		}
		else if (kRecvDebug)
		{
			cout << "Received FIN packet before all data was received." << endl;

			// Reset timeout thread.
			this->mTransTimer->Start(true);

			// Resend ACK.
			_SendAck(true);
		}
	}
}

/* Function: _SetSenderAddr
 * Desc: This function will set the sender variable for this instance to
 * the specified sender parameter. If the copy parameter is true, then
 * the sender data will be copied; otherwise, the same pointer will be 
 * used.
 */
void Receiver::_SetSenderAddr(struct sockaddr_in* senderAddr, bool copy)
{
	delete mSenderAddr;

	if (copy)
	{
		mSenderAddr = new struct sockaddr_in;
		mSenderAddr->sin_family = senderAddr->sin_family;
		mSenderAddr->sin_port = senderAddr->sin_port;
		mSenderAddr->sin_addr.s_addr = senderAddr->sin_addr.s_addr;
	}
	else
	{
		mSenderAddr = senderAddr;
	}
}

/* Function: _BuildAckPacket
 * Desc: This function builds an ACK packet in the specified packet parameter.
 */
void Receiver::_BuildAckPacket(char packet[kAckPacketSize])
{
	//char* packet = new char[kAckPacketSize];
	int offset = 1;
	packet[0] = (char)ACK;
	
	// Copy the ack # to packet.
	setULongToMessage(packet + offset, kAckPacketSize - offset, mLastAck);

	// Increase offset by 8-bytes for ack #.
	offset += kSeqNumSize;

	uint32_t windowSize = 0;

	if (mDiskBuffer != NULL)
	{
		windowSize = mDiskBuffer->GetWindowSize();
	}
	else
	{
	}

	// Copy the window size to packet.
	setUIntToMessage(packet + offset, kAckPacketSize - offset, windowSize);
}

/* Function: _SendAck
 * Desc: This function will send an ACK packet to the sender of the current file.
 */
void Receiver::_SendAck(bool isRetransmit)
{
	//if (mIsStarted && (mCurrentState == RECV_DATA || mCurrentState == RECV_FIN))
	if (this->mIsStarted)
	{
		char packet[kAckPacketSize];
		_BuildAckPacket(packet);
		sendPacket(mSocket, mSenderAddr, packet, kAckPacketSize, kRecvDebug);
	}

	if (isRetransmit && !mLastAckRetransmit)
	{
		// Set value to signal we retransmitted.
		mLastAckRetransmit = true;
	}
}

/* Function: _UpdateRtt
 * Desc: This function updates the running estimated round trip time and deviation.
 * These values are used to calculate a new time out interval, which is also
 * updated. An update will only occur if there was no previous retransmission.
 */
void Receiver::_UpdateRtt()
{
	// Check if we didn't retransmit.
	if (!mLastAckRetransmit)
	{
		// Get the RTT end time.
		gettimeofday(&mRttEndTime, NULL);
		mTimeOutInterval = calculateTimeOutInterval(&mEstRtt, &mDevRtt, &mRttStartTime, &mRttEndTime);

		// Check if we had a error condition for some reason.
		if (mTimeOutInterval == 0)
		{
			mTimeOutInterval = kRecvDefaultTimeOut;
		}
	}
	// We retransmitted, so don't calculate RTT this time.
	else
	{
		mLastAckRetransmit = false;
	}

	// Restart the RTT time.
	gettimeofday(&mRttStartTime, NULL);
}

/* Function: _AckTimeOut
 * Desc: This function handles a time out event for ACKs. If the program is in a
 * state that can resend ACKs, then one will be re-sent.
 */
void Receiver::_AckTimeOut()
{
	// See if we can get a packet lock. If we can't then that means the main thread
	// is parsing a packet and we won't bother retransmitting an ACK.
	if (mPacketLock.TryLock() == 0)
	{
		if (kRecvDebug)
		{
			cout << "ACK timeout!" << endl;
		}

		// Make sure we are in right state and we haven't changed out ACK number.
		if (mCurrentState == RECV_DATA && mLastAck == mDiskBuffer->GetNextSeq())
		{
			_SendAck(true);
		}
		else if (mCurrentState == RECV_FIN)
		{
			//cout << "Terminating after FIN timeout." << endl;
			this->mIsStarted = false;
			exit(EXIT_SUCCESS);
		}

		mPacketLock.Unlock();
	}
	else if (kRecvDebug)
	{
		cout << "An ACK timeout occurred, but the packet lock could not be obtained." << endl;
	}
}

/* Function: _TimeOutCallBack
 * Desc: Static function that handles call backs from the TransmissionTimer
 * class. It will call the correct function to handle the time out on the
 * original object that instantiated the timer.
 */
void Receiver::_TimeOutCallBack(void* caller)
{
	if (caller != NULL)
	{
		Receiver* recv = (Receiver*)caller;
		recv->_AckTimeOut();
	}
}
