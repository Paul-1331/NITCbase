#include "BlockBuffer.h"

#include <cstdlib>
#include <cstring>

// constructor 2
BlockBuffer::BlockBuffer(int blockNum){
    this->blockNum = blockNum;
}

// constructor 1
BlockBuffer::BlockBuffer(char blockType){
    // allocate a block on the disk and a buffer in memory to hold the new block of
    // given type using getFreeBlock function and get the return error codes if any.

    int type = -1;
    if(blockType=='R'){
        type = REC;
    }
    else if(blockType=='I'){
        type = IND_INTERNAL;
    }
    else if(blockType=='L'){
        type = IND_LEAF;
    }
    else{
        this->blockNum = E_OUTOFBOUND; // error code written to blockNum
        return;
    }

    // set the blockNum field of the object to that of the allocated block
    // number if the method returned a valid block number,
    // otherwise set the error code returned as the block number.

    int allocatedBlock = this->getFreeBlock(blockType);
    this->blockNum = allocatedBlock;

    // (The caller must check if the constructor allocatted block successfully
    // by checking the value of block number field.)
}


// calls the parent class constructor
// constructor 1
RecBuffer::RecBuffer(int blockNum): BlockBuffer::BlockBuffer(blockNum){}

// constructor 2
RecBuffer::RecBuffer(): BlockBuffer::BlockBuffer('R'){}
// call parent non-default constructor with 'R' denoting record block.

int BlockBuffer::getHeader(struct HeadInfo*head){
    unsigned char* bufferPtr;
    int ret = loadBlockAndGetBufferPtr(&bufferPtr);
    if(ret!=SUCCESS){
        return ret;
    }

    memset(head, 0, sizeof(*head));

    memcpy(&head->blockType,bufferPtr+0,4);
    memcpy(&head->pblock,bufferPtr+4,4);
    memcpy(&head->lblock,bufferPtr+8,4);
    memcpy(&head->rblock,bufferPtr+12,4);
    memcpy(&head->numEntries,bufferPtr+16,4);
    memcpy(&head->numAttrs,bufferPtr+20,4);
    memcpy(&head->numSlots,bufferPtr+24,4);

    return SUCCESS;
}

int BlockBuffer::setHeader(struct HeadInfo*head){
    unsigned char *bufferPtr;
    int ret  = loadBlockAndGetBufferPtr(&bufferPtr);
    if(ret!=SUCCESS){
        return ret;
    }

    // cast bufferPtr to type HeadInfo*
    struct HeadInfo*bufferHeader = (struct HeadInfo*)bufferPtr;

    bufferHeader->blockType = head->blockType;
    bufferHeader->pblock = head->pblock;
    bufferHeader->lblock = head->lblock;
    bufferHeader->rblock = head->rblock;
    bufferHeader->numEntries = head->numEntries;
    bufferHeader->numAttrs = head->numAttrs;
    bufferHeader->numSlots = head->numSlots;

    ret = StaticBuffer::setDirtyBit(this->blockNum);
    if(ret!=SUCCESS){
        return ret;
    }
    return SUCCESS;
}

// load the record at slotNum into the argument pointer
int RecBuffer::getRecord(union Attribute*rec,int slotNum){
    struct HeadInfo head;
    // get the header using the this.getHeader() function

    this->getHeader(&head);

    int attrCount = head.numAttrs;
    int slotCount = head.numSlots;

    // Use loadBlockAndGetBufferPtr to retrieve pointer to buffer
    unsigned char *bufferPtr;
    int ret = loadBlockAndGetBufferPtr(&bufferPtr);
    if(ret!=SUCCESS){
        return ret;
    }

    /* record at slotNum will be at offset HEADER_SIZE + slotMapSize + (recordSize * slotNum)
     - each record will have size attrCount * ATTR_SIZE
     - slotMap will be of size slotCount
    */

    int recordSize = attrCount*ATTR_SIZE;
    unsigned char *slotPointer = bufferPtr + HEADER_SIZE + slotCount + (recordSize*slotNum); // calculate bufferPtr + offset

    // load the record into the rec data structure
    memcpy(rec, slotPointer, recordSize);

    return SUCCESS;
}

int BlockBuffer::loadBlockAndGetBufferPtr(unsigned char**buffPtr){
    int bufferNum = StaticBuffer::getBufferNum(this->blockNum);
    if(bufferNum==E_BLOCKNOTINBUFFER){
        bufferNum = StaticBuffer::getFreeBuffer(this->blockNum);

        if(bufferNum==E_OUTOFBOUND){
            return E_OUTOFBOUND;
        }

        Disk::readBlock(StaticBuffer::blocks[bufferNum],this->blockNum);
    }
    else{
        for(int bufferIndex = 0;bufferIndex<BUFFER_CAPACITY;bufferIndex++){
            if(!StaticBuffer::metainfo[bufferIndex].free){
                StaticBuffer::metainfo[bufferIndex].timeStamp++;
            }
        }
        StaticBuffer::metainfo[bufferNum].timeStamp = 0;
    }

    *buffPtr = StaticBuffer::blocks[bufferNum];
    return SUCCESS;
}

/* used to get the slotmap from a record block
NOTE: this function expects the caller to allocate memory for `*slotMap`
*/
int RecBuffer::getSlotMap(unsigned char *slotMap) {
  unsigned char *bufferPtr;

  // get the starting address of the buffer containing the block using loadBlockAndGetBufferPtr().
  int ret = loadBlockAndGetBufferPtr(&bufferPtr);
  if (ret != SUCCESS) {
    return ret;
  }

  struct HeadInfo head;
  // get the header of the block using getHeader() function
  this->getHeader(&head);

  int slotCount = head.numSlots;/* number of slots in block from header */

  // get a pointer to the beginning of the slotmap in memory by offsetting HEADER_SIZE
  unsigned char *slotMapInBuffer = bufferPtr + HEADER_SIZE;

  // copy the values from `slotMapInBuffer` to `slotMap` (size is `slotCount`)
  memcpy(slotMap,slotMapInBuffer,slotCount);

  return SUCCESS;
}

int RecBuffer::setSlotMap(unsigned char *slotMap) {
    unsigned char *bufferPtr;
    /* get the starting address of the buffer containing the block using
       loadBlockAndGetBufferPtr(&bufferPtr). */
    int ret = loadBlockAndGetBufferPtr(&bufferPtr);
    if (ret != SUCCESS) {
        return ret;
    }
    // get the header of the block using the getHeader() function
    struct HeadInfo head;
    this->getHeader(&head);

    int numSlots = head.numSlots;

    // the slotmap starts at bufferPtr + HEADER_SIZE. Copy the contents of the
    // argument `slotMap` to the buffer replacing the existing slotmap.
    // Note that size of slotmap is `numSlots`
    memcpy(bufferPtr + HEADER_SIZE, slotMap, numSlots);

    // update dirty bit using StaticBuffer::setDirtyBit
    // if setDirtyBit failed, return the value returned by the call
    ret = StaticBuffer::setDirtyBit(this->blockNum);
    if (ret != SUCCESS) {
        return ret;
    }

    return SUCCESS;
}

int compareAttrs(union Attribute attr1, union Attribute attr2, int attrType) {

    double diff;
    if(attrType == STRING) diff = strcmp(attr1.sVal, attr2.sVal);

    else diff = attr1.nVal - attr2.nVal;

    if (diff > 0) return 1;
    if (diff < 0)  return -1;
    return 0;
}

int RecBuffer::setRecord(union Attribute *rec, int slotNum) {
    unsigned char *bufferPtr;
    /* get the starting address of the buffer containing the block
       using loadBlockAndGetBufferPtr(&bufferPtr). */
    // if loadBlockAndGetBufferPtr(&bufferPtr) != SUCCESS
    // return the value returned by the call.
    int ret = loadBlockAndGetBufferPtr(&bufferPtr);
    if (ret != SUCCESS) {
        return ret;
    }

    /* get the header of the block using the getHeader() function */
    struct HeadInfo head;
    this->getHeader(&head);

    // get number of attributes in the block.
    int attrCount = head.numAttrs;

    // get the number of slots in the block.
    int slotCount = head.numSlots;

    // if input slotNum is not in the permitted range return E_OUTOFBOUND.
    if (slotNum < 0 || slotNum >= slotCount) {
        return E_OUTOFBOUND;
    }

    /* offset bufferPtr to point to the beginning of the record at required
       slot. the block contains the header, the slotmap, followed by all
       the records. so, for example,
       record at slot x will be at bufferPtr + HEADER_SIZE + (x*recordSize)
       copy the record from `rec` to buffer using memcpy
       (hint: a record will be of size ATTR_SIZE * numAttrs)
    */
    int recordSize = attrCount * ATTR_SIZE;
    unsigned char *slotPointer = bufferPtr + HEADER_SIZE + slotCount + (recordSize * slotNum);

    // Copy the record from rec into the block buffer
    memcpy(slotPointer, rec, recordSize);

    // update dirty bit using setDirtyBit()
    ret = StaticBuffer::setDirtyBit(this->blockNum);
    if (ret != SUCCESS) {
        return ret;
    }

    /* (the above function call should not fail since the block is already
       in buffer and the blockNum is valid. If the call does fail, there
       exists some other issue in the code) */

    return SUCCESS;
}

int BlockBuffer::setBlockType(int blockType){
    unsigned char*bufferPtr;
    int ret = loadBlockAndGetBufferPtr(&bufferPtr);
    if(ret!=SUCCESS){
        return ret;
    }

    // inside blocks physical 32 byte header, blockType is first field and it is a 4 byte integer (int32_t) occupying bytes 0,1,2,3
    *((int32_t*)bufferPtr) = blockType;
    //memcpy(bufferPtr, &blockType, sizeof(int32_t)); does same thing

    // unsigned char extracts lowest 8 bits. No useful data is lost
    StaticBuffer::blockAllocMap[this->blockNum] = (unsigned char)blockType;

    ret = StaticBuffer::setDirtyBit(this->blockNum);
    if(ret!=SUCCESS){
        return ret;
    }
    return SUCCESS;
}

int BlockBuffer::getFreeBlock(int blockType){
    // iterate through the StaticBuffer::blockAllocMap and find the block number
    // of a free block in the disk.
    int blockNum;
    for(blockNum = 0;blockNum<DISK_BLOCKS;blockNum++){
        if(StaticBuffer::blockAllocMap[blockNum]==UNUSED_BLK){
            break;
        }
    }
    // if no block is free, return E_DISKFULL.
    if(blockNum==DISK_BLOCKS){
        return E_DISKFULL;
    }
    // set the object's blockNum to the block number of the free block.
    this->blockNum = blockNum;

    // find a free buffer using StaticBuffer::getFreeBuffer() .
    int bufferIndex = StaticBuffer::getFreeBuffer(blockNum);
    if(bufferIndex==E_OUTOFBOUND){
        return E_OUTOFBOUND;
    }

    // initialize the header of the block passing a struct HeadInfo with values
    // pblock: -1, lblock: -1, rblock: -1, numEntries: 0, numAttrs: 0, numSlots: 0
    // to the setHeader() function.
    struct HeadInfo head;
    head.pblock = -1;
    head.lblock = -1;
    head.rblock = -1;
    head.numEntries = 0;
    head.numAttrs = 0;
    head.numSlots = 0;

    int ret = this->setHeader(&head);
    if(ret!=SUCCESS){
        return ret;
    }

    // update the block type of the block to the input block type using setBlockType().
    ret = this->setBlockType(blockType);
    if(ret!=SUCCESS){
        return ret;
    }

    // return block number of the free block.
    return blockNum;
}

int BlockBuffer::getBlockNum(){
    return this->blockNum;
}
