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

    *buffPtr = StaticBuffer::blocks[bufferNum];
    return SUCCESS;
}