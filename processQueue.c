#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include "processQueue.h"

//this file is for the RCBQueue which is a queue for the files that are being processed
//add to queue adds an item to the front of the queue
void addToQueue(RCB * r, RCBQueue * q){
  struct RCBBlock * curr=NULL;
  //if the rcb queue is null, 
  if (q->RCBList == NULL) {   
    //create a new queue
    newQueue(q);
    //set the first item as the new rcb block
    curr = q->RCBList;    
    curr->r=r;
  } 
  else { 
    //if the queue is not empty
    struct RCBBlock * next=NULL;
    curr = q->RCBList;
    //go to the end of the queue
    while (curr->next!=NULL) curr = curr->next;
    //add the item to the end of the queue
    next = (struct RCBBlock *) malloc( sizeof(struct RCBBlock));
    next->next=NULL;
    next->r = r;
    curr->next = next;
  }
}
//this is a function needed for the shortest job first algorithm because items must be added to the queue according to their size
void addToMiddle(RCB * r,RCBQueue * q){
    struct RCBBlock * curr=NULL;
    //if the queue is empty
    if (q->RCBList==NULL){
        //make new queue and add item, same as above
        newQueue(q);
        curr=q->RCBList;
        curr->next = NULL;
        curr->r=r;
    }
    //if queue not empty
    else {
	struct RCBBlock* next = NULL;
        curr=q->RCBList;
        //processes added based on size to send because of SJF
        int remain=r->remainingBytesToSend;
        //if this is the correct place to insert this process
        if(curr->r->remainingBytesToSend>remain){
          //allocate space
            next=(struct RCBBlock *) malloc (sizeof (struct RCBBlock));
            next->next=NULL;
            //insert process
            next->r=curr->r;
            next->next=curr->next;
            q->RCBList->r=r;
            q->RCBList->next=next;
        }
        else{
            //if not, traverse linked list until correct index is found to insert process
            while(curr->next!=NULL&&curr->next->r->remainingBytesToSend<remain) curr=curr->next;
            //allocate space
            next=(struct RCBBlock *) malloc( sizeof( struct RCBBlock));
            //insert process 
            next->next=NULL;
            next->r=r;
            next->next=curr->next;
            curr->next=next;
        }
    }
}
//get the next process to be processed from the rcb queue
RCB* nextProcess(RCBQueue * q){
  RCB * next = q->RCBList->r;
  free(q->RCBList); 
  q->RCBList = q->RCBList->next;
  return next;
}
//new queue is created if there are no items in the queue 
void newQueue(RCBQueue * q) {
  //allocate a size of the required rcb block 
  q->RCBList = (struct RCBBlock *) malloc (sizeof (struct RCBBlock));
  q->RCBList->next=NULL;
}
