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

int BlockAccess::renameRelation(char oldName[ATTR_SIZE], char newName[ATTR_SIZE]) {
    // Check if relation with name newName already exists
    RelCacheTable::resetSearchIndex(RELCAT_RELID);
    Attribute newRelationName;
    strcpy(newRelationName.sVal, newName);

    RecId searchRes = BlockAccess::linearSearch(RELCAT_RELID, (char*)RELCAT_ATTR_RELNAME, newRelationName, EQ);
    if (searchRes.slot != -1 || searchRes.block != -1) {
        return E_RELEXIST;
    }

    // Check if relation with name oldName exists
    RelCacheTable::resetSearchIndex(RELCAT_RELID);
    Attribute oldRelationName; 
    strcpy(oldRelationName.sVal, oldName);

    searchRes = BlockAccess::linearSearch(RELCAT_RELID, (char*)RELCAT_ATTR_RELNAME, oldRelationName, EQ);
    if (searchRes.slot == -1 && searchRes.block == -1) {
        return E_RELNOTEXIST;
    }

    // Update relation name in Relation Catalog record
    RecBuffer recBuffer(searchRes.block);
    Attribute relCatRecord[RELCAT_NO_ATTRS];

    int ret = recBuffer.getRecord(relCatRecord, searchRes.slot);
    if (ret != SUCCESS) {
        return ret;
    }

    strcpy(relCatRecord[RELCAT_REL_NAME_INDEX].sVal, newName);

    ret = recBuffer.setRecord(relCatRecord, searchRes.slot);
    if (ret != SUCCESS) {
        return ret;
    }

    // Update all corresponding entries in Attribute Catalog
    RelCacheTable::resetSearchIndex(ATTRCAT_RELID);
    int numOfAttr = (int)relCatRecord[RELCAT_NO_ATTRIBUTES_INDEX].nVal;

    for (int i = 0; i < numOfAttr; i++) {
        searchRes = BlockAccess::linearSearch(ATTRCAT_RELID, (char*)ATTRCAT_ATTR_RELNAME, oldRelationName, EQ);

        RecBuffer buf(searchRes.block);
        Attribute attrCatRecord[ATTRCAT_NO_ATTRS];

        ret = buf.getRecord(attrCatRecord, searchRes.slot);
        if (ret != SUCCESS) {
            return ret;
        }

        strcpy(attrCatRecord[ATTRCAT_REL_NAME_INDEX].sVal, newName);

        ret = buf.setRecord(attrCatRecord, searchRes.slot);
        if (ret != SUCCESS) {
            return ret;
        }
    }

    return SUCCESS;
}

int BlockAccess::renameAttribute(char relName[ATTR_SIZE], char oldName[ATTR_SIZE], char newName[ATTR_SIZE]) {

    /* reset the searchIndex of the relation catalog using
       RelCacheTable::resetSearchIndex() */
    RelCacheTable::resetSearchIndex(RELCAT_RELID);

    Attribute relNameAttr;    // set relNameAttr to relName
    strcpy(relNameAttr.sVal, relName);

    // Search for the relation with name relName in relation catalog using linearSearch()
    RecId relCatRecId = BlockAccess::linearSearch(RELCAT_RELID, (char*)RELCAT_ATTR_RELNAME, relNameAttr, EQ);

    // If relation with name relName does not exist (search returns {-1,-1})
    //    return E_RELNOTEXIST;
    if (relCatRecId.block == -1 && relCatRecId.slot == -1) {
        return E_RELNOTEXIST;
    }

    /* reset the searchIndex of the attribute catalog using
       RelCacheTable::resetSearchIndex() */
    RelCacheTable::resetSearchIndex(ATTRCAT_RELID);

    /* declare variable attrToRenameRecId used to store the attr-cat recId
       of the attribute to rename */
    RecId attrToRenameRecId{-1, -1};
    Attribute attrCatEntryRecord[ATTRCAT_NO_ATTRS];

    /* iterate over all Attribute Catalog Entry record corresponding to the
       relation to find the required attribute */
    while (true) {
        // linear search on the attribute catalog for RelName = relNameAttr
        RecId attrCatRecId = BlockAccess::linearSearch(ATTRCAT_RELID, (char*)ATTRCAT_ATTR_RELNAME, relNameAttr, EQ);

        // if there are no more attributes left to check (linearSearch returned {-1,-1})
        //     break;
        if (attrCatRecId.block == -1 && attrCatRecId.slot == -1) {
            break;
        }

        /* Get the record from the attribute catalog using RecBuffer.getRecord
           into attrCatEntryRecord */
        RecBuffer attrCatBuffer(attrCatRecId.block);
        int ret = attrCatBuffer.getRecord(attrCatEntryRecord, attrCatRecId.slot);
        if (ret != SUCCESS) {
            return ret;
        }

        // if attrCatEntryRecord.attrName = oldName
        //     attrToRenameRecId = block and slot of this record
        if (strcmp(attrCatEntryRecord[ATTRCAT_ATTR_NAME_INDEX].sVal, oldName) == 0) {
            attrToRenameRecId = attrCatRecId;
        }

        // if attrCatEntryRecord.attrName = newName
        //     return E_ATTREXIST;
        if (strcmp(attrCatEntryRecord[ATTRCAT_ATTR_NAME_INDEX].sVal, newName) == 0) {
            return E_ATTREXIST;
        }
    }

    // if attrToRenameRecId == {-1, -1}
    //     return E_ATTRNOTEXIST;
    if (attrToRenameRecId.block == -1 && attrToRenameRecId.slot == -1) {
        return E_ATTRNOTEXIST;
    }

    // Update the entry corresponding to the attribute in the Attribute Catalog Relation.
    /* declare a RecBuffer for attrToRenameRecId.block and get the record at
       attrToRenameRecId.slot */
    RecBuffer targetBuffer(attrToRenameRecId.block);
    int ret = targetBuffer.getRecord(attrCatEntryRecord, attrToRenameRecId.slot);
    if (ret != SUCCESS) {
        return ret;
    }

    // update the AttrName of the record with newName
    strcpy(attrCatEntryRecord[ATTRCAT_ATTR_NAME_INDEX].sVal, newName);

    // set back the record with RecBuffer.setRecord
    ret = targetBuffer.setRecord(attrCatEntryRecord, attrToRenameRecId.slot);
    if (ret != SUCCESS) {
        return ret;
    }

    return SUCCESS;
}
