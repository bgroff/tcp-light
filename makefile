CFLAGS=-pg 
CC=g++
LIBS=-lpthread -lrt
all: relsend relrevc

relsend: Sender.cpp Mutex.cpp Transmission.cpp Thread.cpp TransmissionTimer.cpp
	$(CC) $(CFLAGS) Sender.cpp Mutex.cpp Transmission.cpp Thread.cpp TransmissionTimer.cpp $(LIBS) -o relsend

relrevc: Receiver.cpp Transmission.cpp TransmissionTimer.cpp Mutex.cpp Thread.cpp DiskBuffer.cpp OutOfSeqCache.cpp
	$(CC) $(CFLAGS) Receiver.cpp Transmission.cpp TransmissionTimer.cpp Mutex.cpp Thread.cpp DiskBuffer.cpp OutOfSeqCache.cpp $(LIBS) -o relrecv
clean:
	rm *.o relsend relrecv
docs: Doxyfile
	doxygen Doxyfile
