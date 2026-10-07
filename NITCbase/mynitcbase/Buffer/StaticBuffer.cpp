#include "StaticBuffer.h"

// the declarations for this class can be found at "StaticBuffer.h"

unsigned char StaticBuffer::blocks[BUFFER_CAPACITY][BLOCK_SIZE];
struct BufferMetaInfo StaticBuffer::metainfo[BUFFER_CAPACITY];
unsigned char StaticBuffer::blockAllocMap[DISK_BLOCKS];

StaticBuffer::StaticBuffer() {
    // BLOCK_SIZE = 2048 bytes  DISK_BLOCK = 2048*4 = 8192 bytes
    for(int blockNum = 0;blockNum<4;blockNum++){
        Disk::readBlock(StaticBuffer::blockAllocMap+(blockNum*BLOCK_SIZE),blockNum);
    }

    for (int bufferIndex = 0;bufferIndex<BUFFER_CAPACITY;bufferIndex++) {
        metainfo[bufferIndex].free = true;
        metainfo[bufferIndex].dirty = false;
        metainfo[bufferIndex].timeStamp = -1;
        metainfo[bufferIndex].blockNum = -1;
    }
}

// write back all modified blocks on system exit
StaticBuffer::~StaticBuffer(){
    
    for(int blockNum = 0;blockNum<4;blockNum++){
        Disk::writeBlock(StaticBuffer::blockAllocMap+(blockNum*BLOCK_SIZE),blockNum);
    }

    for(int bufferIndex = 0;bufferIndex<BUFFER_CAPACITY;bufferIndex++){
        if(metainfo[bufferIndex].free==false&&metainfo[bufferIndex].dirty==true){
            Disk::writeBlock(StaticBuffer::blocks[bufferIndex], metainfo[bufferIndex].blockNum);
        }
    }
}

// get free buffer index
int StaticBuffer::getFreeBuffer(int blockNum){
    if(blockNum<0||blockNum>=DISK_BLOCKS){
        return E_OUTOFBOUND;
    }

    for(int bufferIndex = 0;bufferIndex<BUFFER_CAPACITY;bufferIndex++){
        if(!metainfo[bufferIndex].free){
            metainfo[bufferIndex].timeStamp++;
        }
    }

    int allocatedBuffer;
    for(allocatedBuffer = 0;allocatedBuffer<BUFFER_CAPACITY;allocatedBuffer++){
        if(metainfo[allocatedBuffer].free==true) break;
    }

    if(allocatedBuffer==BUFFER_CAPACITY){
        int maxTimeStamp = -1;
        int lruBufferIndex = -1;

        for(int bufferIndex = 0;bufferIndex<BUFFER_CAPACITY;bufferIndex++){
            if(metainfo[bufferIndex].timeStamp>maxTimeStamp){
                maxTimeStamp = metainfo[bufferIndex].timeStamp;
                lruBufferIndex = bufferIndex;
            }
        }

        allocatedBuffer = lruBufferIndex;

        if(metainfo[allocatedBuffer].dirty){
            Disk::writeBlock(StaticBuffer::blocks[allocatedBuffer],metainfo[allocatedBuffer].blockNum);
        }
    }
    
    metainfo[allocatedBuffer].blockNum = blockNum;
    metainfo[allocatedBuffer].free = false;
    metainfo[allocatedBuffer].dirty = false;
    metainfo[allocatedBuffer].timeStamp = 0;

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

int StaticBuffer::setDirtyBit(int blockNum) {
    // Find the buffer index corresponding to the block using getBufferNum()
    int bufferNum = StaticBuffer::getBufferNum(blockNum);

    if (bufferNum == E_BLOCKNOTINBUFFER) {
        return E_BLOCKNOTINBUFFER;
    }

    if (bufferNum == E_OUTOFBOUND) {
        return E_OUTOFBOUND;
    }

    // The bufferNum is valid: set dirty bit to true
    metainfo[bufferNum].dirty = true;

    return SUCCESS;
}