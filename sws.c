/*
* File: sws.c
* Author: Alex Brodsky
* Purpose: This file contains the implementation of a simple web server.
*          It consists of two functions: main() which contains the main
*          loop accept client connections, and serve_client(), which
*          processes each client request.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdarg.h>
#include <pthread.h>
#include <math.h>
#include "processQueue.h"
#include "network.h"

#define MAX_HTTP_SIZE 8192                 /* size of buffer to allocate */

typedef enum {RR, MLFB, SJF,} scheduler; /* type of scheduler */

int globalSequenceCounter=1;
int requestInQueue=0;
scheduler type; // type of scheduler 
RCBQueue q; // queue for Taking clients
           // queues for the MLQ
RCBQueue RCB_64;
RCBQueue RCB_RR;
/*Thread control*/
static pthread_mutex_t request_mutex;
static pthread_cond_t cond;
static int thread_count; // Number of threads to be created

static void serve_client( int fd, RCBQueue * q) {
  static char *buffer;                              /* request buffer */
  char *req = NULL;                                 /* ptr to req file */
  char *brk;                                        /* state used by strtok */
  char *tmp;                                        /* error checking ptr */
  FILE *fin;                                        /* input file handle */
  int len;                                          /* length of data read */
  
  if( !buffer ) {                                   /* 1st time, alloc buffer */
    buffer = malloc( MAX_HTTP_SIZE );
    if( !buffer ) {                                 /* error check */
      perror( "Error while allocating memory" );
      abort();
    }
  }
  
  memset( buffer, 0, MAX_HTTP_SIZE );
  if( read( fd, buffer, MAX_HTTP_SIZE ) <= 0 ) {    /* read req from client */
    perror( "Error while reading request" );
    abort();
  }
  
  /* standard requests are of the form
  *   GET /foo/bar/qux.html HTTP/1.1
  * We want the second token (the file path).
  */
  tmp = strtok_r( buffer, " ", &brk );              /* parse request */
  if( tmp && !strcmp( "GET", tmp ) ) {
    req = strtok_r( NULL, " ", &brk );
  }
  
  if( !req ) {                                      /* is req valid? */
    len = sprintf( buffer, "HTTP/1.1 400 Bad request\n\n" );
    write( fd, buffer, len );                       /* if not, send err */
    close(fd);
  } else {                                          /* if so, open file */
    req++;                                          /* skip leading / */
    fin = fopen( req, "r" );                        /* open file */
    if( !fin ) {                                    /* check if successful */
      len = sprintf( buffer, "HTTP/1.1 404 File not found\n\n" );
      write( fd, buffer, len );                     /* if not, send err */
      close(fd);
    }
    else { 
      /*NOTE THAT ONLY MAIN THREAD ACCESS HERE*/
      /*MUTEX FOR INSERTION OF THE NEW RECEIVED REQUEST*/
      pthread_mutex_lock(&request_mutex);
      requestInQueue++;
      /* if so, send file */
      /*NOTE THAT THE FIRST QUEUE FOR RR AND MLQ BEHAVE THE SAME WAY*/
      if(type==RR||type==MLFB){
	struct stat st;
	fstat(fileno(fin), &st);
	RCB* r = (RCB *) malloc( sizeof(RCB));
	r->sequenceNumber = globalSequenceCounter;
	globalSequenceCounter++;
	r->fileDescriptor = fd;
	r->handle = fin;
	r->remainingBytesToSend = (int)st.st_size;
	r->quantum = MAX_HTTP_SIZE;
	addToQueue(r,q);
	len = sprintf( buffer, "HTTP/1.1 200 OK\n\n" );
	write( fd, buffer, len );
	/*signals that process has been completed in case there are worker threads asleep*/
	pthread_cond_signal(&cond);
	/*UNLOCKS MUTEX AFTER INSERTION OF THE NEW REQUEST*/
	pthread_mutex_unlock(&request_mutex);
      }
      else if(type==SJF){

	struct stat st;
	fstat(fileno(fin), &st);
	RCB* r = (RCB *) malloc( sizeof(RCB));
	r->sequenceNumber = globalSequenceCounter;
	globalSequenceCounter++;
	r->fileDescriptor = fd;
	r->handle = fin;
	r->remainingBytesToSend = (int)st.st_size;
	r->quantum = (int)st.st_size;
	addToMiddle(r,q);
	len = sprintf( buffer, "HTTP/1.1 200 OK\n\n" );
	write( fd, buffer, len );
	/*signal that process has been completed in case there are worker treads sleep*/
	pthread_cond_signal(&cond);
	/*UNLOCKS MUTEX AFTER INSERTION OF THE NEW REQUEST*/
	pthread_mutex_unlock(&request_mutex);
      }
    }
  }
}

/*RoundRobin scheduler*/
void RoundRobin(RCB* r, RCBQueue* q) {
  static char *buffer;
  long len;
  //set buffer to max size from above
  buffer=malloc(MAX_HTTP_SIZE);     
  //read file from the txt    
  len = fread(buffer, 1, MAX_HTTP_SIZE, r->handle);    
  //write the description 
  len = write(r->fileDescriptor, buffer, len);
  //subtract the remaining bytes of the process from what has been processed
  int remain=r->remainingBytesToSend-len;
  //update the remaining bytes to send
  r->remainingBytesToSend=remain;
  //if terminated...
  if (r->remainingBytesToSend <= 0){
    //close file
    fclose(r->handle);
    close(r->fileDescriptor);
    //free rcb item
    free(r);
    requestInQueue--;
  } else {
    //if not completely processed, return to queue
    addToQueue(r, q);
  }
}
/*MLQ scheduler, sizeToSend = 1 (8KB), 8(64KB), 0 (remaining bytes)*/
void MultiLevelQueue(RCB* r, RCBQueue* q, int sizeToSend){
  static char *buffer;
  int size;
  long len;
  //calculates the # of bytes to send
  if (sizeToSend!=0){ size = sizeToSend*MAX_HTTP_SIZE;}
  else{size = r->remainingBytesToSend;} 
  buffer=malloc(size);
  //reads txt file
  len = fread( buffer, 1, size, r->handle);
  //writes to output file
  len = write(r->fileDescriptor, buffer, len);
  //updates remaining bytes to process
  int remain=r->remainingBytesToSend-len;
  r->remainingBytesToSend=remain;
  //if terminated
  if(r->remainingBytesToSend <=0){
    //close
    fclose(r->handle);
    close(r->fileDescriptor);
    //free rcb item
    free(r);
    requestInQueue--;
  }else{
    //if not terminated, add to queue for additional processing
    addToQueue(r, q);
  }
}
/*ShortesJobFirst scheduler*/
void ShortestJobFirst(RCB* r,RCBQueue * q) {
    static char * buffer;
    buffer=malloc(r->quantum );
    long len;
    //read from txt
    len=fread(buffer,1,r->quantum,r->handle);
    //write to file
    len = write(r->fileDescriptor,buffer,len);
    //update remaining bytes to process
    int remain=r->remainingBytesToSend-len;
    r->remainingBytesToSend=remain;
    //then close, no need to add to queue because SJF processes files to completion 
    fclose(r->handle);
    close(r->fileDescriptor);
    free(r);
    requestInQueue--;
}
/*RUN SCHEDULER METHOD. WORKER THREADS WILL ACCESS THIS*/
void *runScheduler(){
  while(1){
    pthread_mutex_lock(&request_mutex); // locks processesing
    /*Unlocks Mutex while there are no requests for new insertion. Threads are asleep here. Once cond is signal, it releases one at the time, hence, one per new addition*/
    while(requestInQueue<1){pthread_cond_wait(&cond, &request_mutex);}
   if (type==RR){
      while(q.RCBList!=NULL) {
        RCB * process = nextProcess(&q);
	if(process!=NULL) {
	   RoundRobin(process, &q);
	}
      }
    }
    else if(type==MLFB){
      while(q.RCBList!=NULL){
	RCB * process = nextProcess(&q);
	if(process!=NULL){
	  MultiLevelQueue(process, &RCB_64, 1);
	}
      }
      while(RCB_64.RCBList!=NULL&&q.RCBList==NULL){
        RCB * process = nextProcess(&RCB_64);
        if(process!=NULL){
          MultiLevelQueue(process, &RCB_RR, 8);
        }
      }
      while(RCB_RR.RCBList!=NULL&&(RCB_64.RCBList==NULL&&q.RCBList==NULL)){
        RCB * process = nextProcess(&RCB_RR);
        if(process!=NULL){
          MultiLevelQueue(process, &RCB_RR, 0);
        }
      }
    }
    else if(type==SJF){
      while(q.RCBList!=NULL) {
        RCB * process = nextProcess(&q);
	if(process!=NULL) {
	   ShortestJobFirst(process, &q);
	}
      }
    }
   pthread_mutex_unlock(&request_mutex); //Unlocks 
  }
  return (void*)1;
}

/* This function is where the program starts running.
*    The function first parses its command line parameters to determine port #
*    Then, it initializes, the network and enters the main loop.
*    The main loop waits for a client (1 or more to connect, and then processes
*    all clients by calling the seve_client() function for each one.
* Parameters:
*             argc : number of command line parameters (including program name
*             argv : array of pointers to command line parameters
* Returns: an integer status code, 0 for success, something else for error.
*/
int main( int argc, char **argv ) {
  //RCBQueue q; this was moved to global
	   //q.RCBList=NULL;
  int port = -1;                                    /* server port # */
  int fd;                                           /* client file descriptor */
  
  /* check for and process parameters. Main thread initializes the scheduler
  */

  if(argc<3){ printf("Incorrect Number of parameters.\n sws <port> <scheduler> <thread_count>\n"); return 0;
  }
  else if(( sscanf( argv[1], "%d", &port ) < 1 ) ) {
    printf( "usage: sms <port>\n" );
    return 0;
  }
  else{
    if(strcmp(argv[2], "RR")==0){ type = RR; q.RCBList=NULL;} 
    else if(strcmp(argv[2], "MLFB")==0){ type = MLFB; q.RCBList=NULL;
      RCB_64.RCBList=NULL; RCB_RR.RCBList=NULL;
    }
    else if(strcmp(argv[2], "SJF")==0){ type = SJF; q.RCBList=NULL;}
    else {perror("invalid scheduler"); return 0;}
  }
  //set thread count
  thread_count=atoi(argv[3]);
  
  network_init( port ); /* init network module */

  /*thread control*/
  pthread_mutex_init(&request_mutex, NULL);
  pthread_cond_init(&cond, NULL);
  /*array containing the number og working threads*/
  pthread_t working_threads[thread_count];
  int i;
  /*starts all worker threads by MAIN thread and sends them to runScheduler where they run in an infinite loop*/
  for (i=0; i<thread_count; i++){
    if(pthread_create(&working_threads[i], NULL,runScheduler, NULL)){
	perror("Thread could not be created...\n"); 
	abort();
    }
  }
  for( ;; ) {                                       /* main loop */
    network_wait();                                 /* wait for clients */
    /*NOTE: ONLY MAIN THREAD HAS ACCESS HERE AS SUGGESTED BY THE ASSIGMENT*/
    for( fd = network_open(); fd >= 0; fd = network_open() ) serve_client(fd , &q);
    
  }
}