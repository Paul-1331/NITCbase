#include "StaticBuffer.h"

// the declarations for this class can be found at "StaticBuffer.h"

unsigned char StaticBuffer::blocks[BUFFER_CAPACITY][BLOCK_SIZE];
struct BufferMetaInfo StaticBuffer::metainfo[BUFFER_CAPACITY];

StaticBuffer::StaticBuffer() {
    
  // initialise all blocks as free
  for (int bufferIndex = 0;bufferIndex<BUFFER_CAPACITY;bufferIndex++) {
    metainfo[bufferIndex].free = true;
  }
}

/*
At this stage, we are not writing back from the buffer to the disk since we are
not modifying the buffer. So, we will define an empty destructor for now. In
subsequent stages, we will implement the write-back functionality here.
*/
StaticBuffer::~StaticBuffer(){}

// get free buffer index
int StaticBuffer::getFreeBuffer(int blockNum){
    if(blockNum<0||blockNum>=DISK_BLOCKS){
        return E_OUTOFBOUND;
    }
    int allocatedBuffer;
    for(allocatedBuffer = 0;allocatedBuffer<BUFFER_CAPACITY;allocatedBuffer++){
        if(metainfo[allocatedBuffer].free==true) break;
    }

    if(allocatedBuffer==BUFFER_CAPACITY) return E_OUTOFBOUND;
    metainfo[allocatedBuffer].blockNum = blockNum;
    metainfo[allocatedBuffer].free = false;
    return allocatedBuffer;
}


/* Get the buffer index where a particular block is stored
   or E_BLOCKNOTINBUFFER otherwise
*/
int StaticBuffer::getBufferNum(int blockNum){
    if(blockNum<0||blockNum>=DISK_BLOCKS){
        return E_OUTOFBOUND;
    }

    for(int bufferIndex = 0;bufferIndex<BUFFER_CAPACITY;bufferIndex++){
        if(metainfo[bufferIndex].blockNum==blockNum){
            return bufferIndex;
        }
    }

    return E_BLOCKNOTINBUFFER;
}