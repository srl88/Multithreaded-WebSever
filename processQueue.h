#include <stdio.h>
typedef struct{
  int sequenceNumber; //The sequence number of the request (starting at 1)
  int fileDescriptor; //The file descriptor of the client (returned by network_wait())
  FILE * handle; //The FILE * handle (or the file descriptor) of the file being requested
  int remainingBytesToSend; //The number of bytes of the file that remain to be sent
  int quantum; //The quantum, which is the maximum number of bytes to be sent when the request is serviced
} RCB;

//individual RCB node item 
struct RCBBlock{
	//rbc item and a next block item
	RCB * r;
	struct RCBBlock * next;
};
//this is the rbc linked list of rbc blocks 
typedef struct{
	struct RCBBlock * RCBList;
} RCBQueue;

//prototypes
void newQueue(RCBQueue * q);
void addToQueue(RCB * r, RCBQueue * q);
RCB * nextProcess(RCBQueue * q);
void addToMiddle(RCB * r, RCBQueue * q);

