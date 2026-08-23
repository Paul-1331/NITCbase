#include "BlockAccess.h"

#include <cstring>

RecId BlockAccess::linearSearch(int relId, char attrName[ATTR_SIZE], union Attribute attrVal, int op) {
    // get the previous search index of the relation relId from the relation cache
    // (use RelCacheTable::getSearchIndex() function)
    RecId prevRecId;
    int ret = RelCacheTable::getSearchIndex(relId,&prevRecId);
    if(ret!=SUCCESS){
        return RecId{-1,-1};
    }

    // get the first record block of the relation from the relation cache
    // (use RelCacheTable::getRelCatEntry() function of Cache Layer)
    RelCatEntry relCatEntry;
    ret = RelCacheTable::getRelCatEntry(relId,&relCatEntry);
    if(ret!=SUCCESS){
        return RecId{-1,-1};
    }

    /*
        To get the attribute offset for the attrName attribute
        from the attribute cache entry of the relation using
        AttrCacheTable::getAttrCatEntry()
    */
    AttrCatEntry attrCatEntry;
    ret = AttrCacheTable::getAttrCatEntry(relId,attrName,&attrCatEntry);
    if(ret!=SUCCESS){
        return RecId{-1,-1};
    }

    // let block and slot denote the record id of the record being currently checked
    int block,slot;

    // if the current search index record is invalid(i.e. both block and slot = -1)
    if (prevRecId.block == -1 && prevRecId.slot == -1)
    {
        // (no hits from previous search; search should start from the
        // first record itself)

        // block = first record block of the relation
        // slot = 0
        block = relCatEntry.firstBlk;
        slot = 0;
    }
    else
    {
        // (there is a hit from previous search; search should start from
        // the record next to the search index record)

        // block = search index's block
        // slot = search index's slot + 1
        block = prevRecId.block;
        slot = prevRecId.slot+1;
    }

    /* The following code searches for the next record in the relation
       that satisfies the given condition
       We start from the record id (block, slot) and iterate over the remaining
       records of the relation
    */
    while (block != -1)
    {

        /* create a RecBuffer object for block (use RecBuffer Constructor for
           existing block) */
        RecBuffer currentBlock(block);

        // get header of the block using RecBuffer::getHeader() function
        HeadInfo head;
        currentBlock.getHeader(&head);

        // If slot >= the number of slots per block(i.e. no more slots in this block)
        if(slot>=head.numSlots)
        {
            // update block = right block of block
            block = head.rblock;
            // update slot = 0
            slot = 0;
            continue;  // continue to the beginning of this while loop
        }

        // get slot map of the block using RecBuffer::getSlotMap() function
        unsigned char slotMap[head.numSlots];
        currentBlock.getSlotMap(slotMap);

        // if slot is free skip the loop
        // (i.e. check if slot'th entry in slot map of block contains SLOT_UNOCCUPIED)
        if(slotMap[slot]==SLOT_UNOCCUPIED)
        {
            // increment slot and continue to the next record slot
            slot++;
            continue;
        }

        // get the record with id (block, slot) using RecBuffer::getRecord()
        Attribute record [relCatEntry.numAttrs];
        currentBlock.getRecord(record,slot);


        // compare record's attribute value to the the given attrVal as below:        
        /* use the attribute offset to get the value of the attribute from
           current record */
        int cmpVal;  // will store the difference between the attributes
        // set cmpVal using compareAttrs()
        cmpVal = compareAttrs(record[attrCatEntry.offset],attrVal,attrCatEntry.attrType);

        /* Next task is to check whether this record satisfies the given condition.
           It is determined based on the output of previous comparison and
           the op value received.
           The following code sets the cond variable if the condition is satisfied.
        */
        if (
            (op == NE && cmpVal != 0) ||    // if op is "not equal to"
            (op == LT && cmpVal < 0) ||     // if op is "less than"
            (op == LE && cmpVal <= 0) ||    // if op is "less than or equal to"
            (op == EQ && cmpVal == 0) ||    // if op is "equal to"
            (op == GT && cmpVal > 0) ||     // if op is "greater than"
            (op == GE && cmpVal >= 0)       // if op is "greater than or equal to"
        ) {
            /*
            set the search index in the relation cache as
            the record id of the record that satisfies the given condition
            (use RelCacheTable::setSearchIndex function)
            */
            RecId matchId = {block,slot};
            RelCacheTable::setSearchIndex(relId,&matchId);
            return matchId;
        }

        slot++;
    }

    // no record in the relation with Id relid satisfies the given condition
    RecId noMatch = {-1, -1};
    RelCacheTable::setSearchIndex(relId, &noMatch);
    return noMatch;
}
