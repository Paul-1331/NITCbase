#include "BlockBuffer.h"

#include <cstdlib>
#include <cstring>

BlockBuffer::BlockBuffer(int blockNum){
    this->blockNum = blockNum;
}

// calls the parent class constructor
RecBuffer::RecBuffer(int blockNum): BlockBuffer::BlockBuffer(blockNum){}

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
