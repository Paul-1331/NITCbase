#include "BPlusTree.h"
#include <cstring>

int BPlusTree::numOfComparisons = 0;

RecId BPlusTree::bPlusSearch(int relId, char attrName[ATTR_SIZE], Attribute attrVal, int op) {
    // 1. Declare searchIndex which will be used to store search index for attrName.
    IndexId searchIndex;

    /* Get the search index corresponding to attribute with name attrName
       using AttrCacheTable::getSearchIndex(). */
    int retVal = AttrCacheTable::getSearchIndex(relId, attrName, &searchIndex);
    if (retVal != SUCCESS) {
        return RecId{-1, -1};
    }

    AttrCatEntry attrCatEntry;
    /* Load the attribute cache entry into attrCatEntry using
       AttrCacheTable::getAttrCatEntry(). */
    retVal = AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntry);
    if (retVal != SUCCESS) {
        return RecId{-1, -1};
    }

    // Declare variables block and index which will be used during search
    int block, index;

    if (searchIndex.block == -1 && searchIndex.index == -1) {
        // (search is done for the first time)

        // Start the search from the first entry of root.
        block = attrCatEntry.rootBlock;
        index = 0;

        // If attrName doesn't have a B+ tree (rootBlock == -1)
        if (block == -1) {
            return RecId{-1, -1};
        }

    } else {
        /* A valid searchIndex points to an entry in the leaf index of the attribute's
           B+ Tree which had previously satisfied the op for the given attrVal. */
        block = searchIndex.block;
        index = searchIndex.index + 1;  // search is resumed from the next index.

        // Load block into leaf using IndLeaf::IndLeaf().
        IndLeaf leaf(block);

        // Declare leafHead which will be used to hold the header of leaf.
        HeadInfo leafHead;
        leaf.getHeader(&leafHead);

        if (index >= leafHead.numEntries) {
            /* All the entries in the block have been searched; search from the
               beginning of the next leaf index block. */
            block = leafHead.rblock;
            index = 0;

            if (block == -1) {
                // (End of linked list reached - the search is done.)
                return RecId{-1, -1};
            }
        }
    }

    /****** Traverse through all the internal nodes according to value
            of attrVal and the operator op ******/

    /* (This section is only needed when:
       - search restarts from the root block (when searchIndex was reset by caller)
       - root is not a leaf
       If there was a valid search index, then we are already at a leaf block
       and the test condition in the following loop will fail)
    */

    while (StaticBuffer::getStaticBlockType(block) == IND_INTERNAL) {

        // Load the block into internalBlk using IndInternal::IndInternal().
        IndInternal internalBlk(block);

        HeadInfo intHead;
        internalBlk.getHeader(&intHead);

        if (intHead.numEntries == 0) {
            return RecId{-1, -1};
        }

        // Declare intEntry which will be used to store an entry of internalBlk.
        InternalEntry intEntry;

        if (op == NE || op == LT || op == LE) {
            /*
            - NE: need to search the entire linked list of leaf indices of the B+ Tree,
              starting from the leftmost leaf index. Thus, always move to the left.
            - LT and LE: the attribute values are arranged in ascending order in the
              leaf indices of the B+ Tree. Values that satisfy these conditions, if
              any exist, will always be found in the leftmost leaf index. Thus,
              always move to the left.
            */

            // Load entry in the first slot of the block into intEntry
            internalBlk.getEntry(&intEntry, 0);

            block = intEntry.lChild;

        } else {
            /*
            - EQ, GT and GE: move to the left child of the first entry that is
              greater than (or equal to) attrVal.
            */

            bool found = false;

            for (int i = 0; i < intHead.numEntries; ++i) {
                internalBlk.getEntry(&intEntry, i);

                int cmpVal = compareAttrs(intEntry.attrVal, attrVal, attrCatEntry.attrType);
                BPlusTree::numOfComparisons++;

                if ((op == EQ || op == GE) && cmpVal >= 0) {
                    block = intEntry.lChild;
                    found = true;
                    break;
                } else if (op == GT && cmpVal > 0) {
                    block = intEntry.lChild;
                    found = true;
                    break;
                }
            }

            if (!found) {
                // Move to the right child of the last entry of the block
                block = intEntry.rChild;
            }
        }
    }

    // NOTE: `block` now holds the block number of a leaf index block.

    /****** Identify the first leaf index entry from the current position
            that satisfies our condition (moving right) ******/

    while (block != -1) {
        // Load the block into leafBlk using IndLeaf::IndLeaf().
        IndLeaf leafBlk(block);
        HeadInfo leafHead;
        leafBlk.getHeader(&leafHead);

        // Declare leafEntry which will be used to store an entry from leafBlk
        Index leafEntry;

        while (index < leafHead.numEntries) {
            // Load entry corresponding to index into leafEntry
            leafBlk.getEntry(&leafEntry, index);

            int cmpVal = compareAttrs(leafEntry.attrVal, attrVal, attrCatEntry.attrType);
            BPlusTree::numOfComparisons++;

            if (
                (op == EQ && cmpVal == 0) ||
                (op == LE && cmpVal <= 0) ||
                (op == LT && cmpVal < 0)  ||
                (op == GT && cmpVal > 0)  ||
                (op == GE && cmpVal >= 0) ||
                (op == NE && cmpVal != 0)
            ) {
                // (Entry satisfying the condition found)

                // Set search index to {block, index}
                IndexId newSearchIndex = {block, index};
                AttrCacheTable::setSearchIndex(relId, attrName, &newSearchIndex);

                // Return the recId {leafEntry.block, leafEntry.slot}
                return RecId{leafEntry.block, leafEntry.slot};

            } else if ((op == EQ || op == LE || op == LT) && cmpVal > 0) {
                /* Future entries will not satisfy EQ, LE, LT since values
                   are arranged in ascending order in the leaves */
                return RecId{-1, -1};
            }

            // Search next index
            ++index;
        }

        /* LT and LE cannot have matching entries in later leaves once the
           current leaf has been exhausted. EQ may span leaves when duplicate
           keys cross a leaf boundary. */
        if (op == LE || op == LT) {
            break;
        }

        // EQ, GT, GE, and NE may have matching entries in later leaves.
        block = leafHead.rblock;
        index = 0;
    }

    // No entry satisfying the op was found; return RecId {-1, -1}
    return RecId{-1, -1};
}

int BPlusTree::bPlusCreate(int relId, char attrName[ATTR_SIZE]) {
    if (relId == RELCAT_RELID || relId == ATTRCAT_RELID) {
        return E_NOTPERMITTED;
    }

    // Fetch the attribute catalog entry for `attrName`
    AttrCatEntry attrCatEntry;
    int retVal = AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntry);
    if (retVal != SUCCESS) {
        return retVal;
    }

    // If an index already exists for the attribute, nothing more needs to be done
    if (attrCatEntry.rootBlock != -1) {
        return SUCCESS;
    }

    /****** Creating a new B+ Tree ******/

    // Allocate a free leaf block for the root using Constructor 2 (which allocates a new block)
    IndLeaf rootBlockBuf;

    // Retrieve the block number of the allocated leaf block
    int rootBlock = rootBlockBuf.getBlockNum();

    // Check if the disk is full
    if (rootBlock == E_DISKFULL) {
        return E_DISKFULL;
    }

    // Update the rootBlock field in the Attribute Cache Table
    attrCatEntry.rootBlock = rootBlock;
    AttrCacheTable::setAttrCatEntry(relId, attrName, &attrCatEntry);

    // Fetch the relation catalog entry to traverse data blocks
    RelCatEntry relCatEntry;
    retVal = RelCacheTable::getRelCatEntry(relId, &relCatEntry);
    if (retVal != SUCCESS) {
        return retVal;
    }

    int block = relCatEntry.firstBlk;

    /***** Traverse all the blocks in the relation and insert records
           one by one into the B+ Tree *****/
    while (block != -1) {
        // Instantiate a RecBuffer object for `block`
        RecBuffer recBuffer(block);

        unsigned char slotMap[relCatEntry.numSlotsPerBlk];
        recBuffer.getSlotMap(slotMap);

        // Iterate through all slots of the block
        for (int slot = 0; slot < relCatEntry.numSlotsPerBlk; ++slot) {
            // Check if slot is occupied
            if (slotMap[slot] == SLOT_OCCUPIED) {
                Attribute record[relCatEntry.numAttrs];
                recBuffer.getRecord(record, slot);

                RecId recId{block, slot};
                Attribute attrVal = record[attrCatEntry.offset];

                // Insert the attribute value and recId into the B+ tree
                retVal = bPlusInsert(relId, attrName, attrVal, recId);

                if (retVal == E_DISKFULL) {
                    // bPlusInsert handles destroying the tree if disk runs out of space
                    return E_DISKFULL;
                }
            }
        }

        // Advance to the next record block using the header's rblock
        HeadInfo head;
        recBuffer.getHeader(&head);
        block = head.rblock;
    }

    return SUCCESS;
}

int BPlusTree::bPlusDestroy(int rootBlockNum) {
    if (rootBlockNum < 0 || rootBlockNum >= DISK_BLOCKS) {
        return E_OUTOFBOUND;
    }

    // Determine block type using StaticBuffer::getStaticBlockType()
    int type = StaticBuffer::getStaticBlockType(rootBlockNum);

    if (type == IND_LEAF) {
        // Declare an instance of IndLeaf for rootBlockNum using Constructor 1 (existing block)
        IndLeaf leaf(rootBlockNum);

        // Release the leaf block using BlockBuffer::releaseBlock()
        leaf.releaseBlock();

        return SUCCESS;

    } else if (type == IND_INTERNAL) {
        // Declare an instance of IndInternal for rootBlockNum using Constructor 1 (existing block)
        IndInternal internalBlk(rootBlockNum);

        // Load the header of the block
        HeadInfo header;
        internalBlk.getHeader(&header);

        /* Iterate through entries of internalBlk:
           - The rchild of entry i is the same as the lchild of entry i + 1.
           - Destroy the lchild of the first entry (index 0).
           - Destroy the rchild of ALL entries (indices 0 to numEntries - 1).
           This ensures every child subtree is visited exactly once without duplicates.
        */
        InternalEntry entry;

        if (header.numEntries > 0) {
            // First entry's left child
            internalBlk.getEntry(&entry, 0);
            bPlusDestroy(entry.lChild);

            // Right child of each entry
            for (int i = 0; i < header.numEntries; ++i) {
                internalBlk.getEntry(&entry, i);
                bPlusDestroy(entry.rChild);
            }
        }

        // Release the current internal node block
        internalBlk.releaseBlock();

        return SUCCESS;

    } else {
        // Block is not an index block (neither IND_LEAF nor IND_INTERNAL)
        return E_INVALIDBLOCK;
    }
}

int BPlusTree::bPlusInsert(int relId, char attrName[ATTR_SIZE], Attribute attrVal, RecId recId) {
    // Get the attribute catalog entry corresponding to attrName
    AttrCatEntry attrCatEntry;
    int retVal = AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntry);
    if (retVal != SUCCESS) {
        return retVal;
    }

    int blockNum = attrCatEntry.rootBlock;

    // Check if an index exists on the attribute
    if (blockNum == -1) {
        return E_NOINDEX;
    }

    // Find the leaf block to which insertion is to be done
    int leafBlkNum = findLeafToInsert(blockNum, attrVal, attrCatEntry.attrType);

    // Construct the leaf index entry
    Index entry;
    entry.attrVal = attrVal;
    entry.block = recId.block;
    entry.slot = recId.slot;

    // Insert into the leaf block (propagates upwards if splitting is necessary)
    retVal = insertIntoLeaf(relId, attrName, leafBlkNum, entry);

    // Handle disk exhaustion
    if (retVal == E_DISKFULL) {
        // Destroy the existing B+ tree rooted at the initial root block
        bPlusDestroy(blockNum);

        // Reset the root block in the attribute catalog cache entry to -1
        attrCatEntry.rootBlock = -1;
        AttrCacheTable::setAttrCatEntry(relId, attrName, &attrCatEntry);

        return E_DISKFULL;
    }

    return SUCCESS;
}

int BPlusTree::findLeafToInsert(int rootBlock, Attribute attrVal, int attrType) {
    int blockNum = rootBlock;

    // Traverse downwards until reaching a leaf block
    while (StaticBuffer::getStaticBlockType(blockNum) != IND_LEAF) {
        // Declare an IndInternal object for blockNum using Constructor 1 (existing block)
        IndInternal internalBlk(blockNum);

        // Get header of the block
        HeadInfo intHead;
        internalBlk.getHeader(&intHead);

        /* Iterate through all the entries in the internal node to find the 
           first entry whose attribute value >= value to be inserted. */
        InternalEntry entry;
        bool found = false;

        for (int i = 0; i < intHead.numEntries; ++i) {
            internalBlk.getEntry(&entry, i);

            int cmpVal = compareAttrs(entry.attrVal, attrVal, attrType);

            if (cmpVal >= 0) {
                // First entry where entry.attrVal >= attrVal found
                blockNum = entry.lChild;
                found = true;
                break;
            }
        }

        // If no entry had entry.attrVal >= attrVal, follow the rightmost pointer
        if (!found) {
            // entry still holds the last entry (i = intHead.numEntries - 1)
            blockNum = entry.rChild;
        }
    }

    return blockNum;
}

int BPlusTree::insertIntoLeaf(int relId, char attrName[ATTR_SIZE], int blockNum, Index indexEntry) {
    // Get the attribute cache entry corresponding to attrName
    AttrCatEntry attrCatEntry;
    int retVal = AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntry);
    if (retVal != SUCCESS) {
        return retVal;
    }

    // Declare an IndLeaf instance for the leaf block using Constructor 1 (existing block)
    IndLeaf leafBlk(blockNum);

    // Store the header of the leaf index block into blockHeader
    HeadInfo blockHeader;
    leafBlk.getHeader(&blockHeader);

    // Temporary array to hold existing entries + the new index entry
    Index indices[blockHeader.numEntries + 1];

    /* Iterate through all the entries in the block and insert `indexEntry`
          at the appropriate position in the `indices` array to maintain ascending order. */
    int i = 0;
    bool inserted = false;

    for (int oldIdx = 0; oldIdx < blockHeader.numEntries; ++oldIdx) {
        Index currEntry;
        leafBlk.getEntry(&currEntry, oldIdx);

        int cmpVal = compareAttrs(currEntry.attrVal, indexEntry.attrVal, attrCatEntry.attrType);

        // When currEntry > indexEntry and not yet inserted, place indexEntry first
        if (!inserted && cmpVal > 0) {
            indices[i++] = indexEntry;
            inserted = true;
        }

        indices[i++] = currEntry;
    }

    // If indexEntry is larger than all existing entries in the block
    if (!inserted) {
        indices[i++] = indexEntry;
    }

    // Case 1: Leaf block has NOT reached maximum capacity (can fit without splitting)
    if (blockHeader.numEntries != MAX_KEYS_LEAF) {
        // Increment numEntries in header and write back
        blockHeader.numEntries++;
        leafBlk.setHeader(&blockHeader);

        // Populate the block entries using IndLeaf::setEntry()
        for (int idx = 0; idx < blockHeader.numEntries; ++idx) {
            leafBlk.setEntry(&indices[idx], idx);
        }

        return SUCCESS;
    }

    // Case 2: Leaf block is FULL. Split the entries between two leaf blocks.
    int newRightBlk = splitLeaf(blockNum, indices);

    // If splitLeaf() failed to allocate a new block
    if (newRightBlk == E_DISKFULL) {
        return E_DISKFULL;
    }

    // Check if the current leaf block was NOT the root (pblock != -1)
    if (blockHeader.pblock != -1) {
        /* Create a struct InternalEntry with:
           - attrVal = middle value from `indices` (last key of left block)
           - lChild = currentBlock (blockNum)
           - rChild = newRightBlk
        */
        InternalEntry intEntry;
        intEntry.attrVal = indices[MIDDLE_INDEX_LEAF].attrVal;
        intEntry.lChild = blockNum;
        intEntry.rChild = newRightBlk;

        // Propagate the entry into the parent internal node
        return insertIntoInternal(relId, attrName, blockHeader.pblock, intEntry);

    } else {
        /* The current block was the root block and has now split.
           A new internal index block must be allocated and made the root of the tree. */
        return createNewRoot(relId, attrName, indices[MIDDLE_INDEX_LEAF].attrVal, blockNum, newRightBlk);
    }
}

int BPlusTree::splitLeaf(int leafBlockNum, Index indices[]) {
    // 1. Declare rightBlk using Constructor 2 (no arguments) to allocate a new leaf block
    IndLeaf rightBlk;

    // 2. Declare leftBlk using Constructor 1 to open the existing leaf block
    IndLeaf leftBlk(leafBlockNum);

    int rightBlkNum = rightBlk.getBlockNum();
    int leftBlkNum = leafBlockNum;

    // 3. Check if block allocation succeeded
    if (rightBlkNum == E_DISKFULL) {
        return E_DISKFULL;
    }

    // 4. Retrieve headers of both blocks
    HeadInfo leftBlkHeader, rightBlkHeader;
    leftBlk.getHeader(&leftBlkHeader);
    rightBlk.getHeader(&rightBlkHeader);

    // 5. Configure right block header and write back
    rightBlkHeader.numEntries = (MAX_KEYS_LEAF + 1) / 2; // 32 entries
    rightBlkHeader.pblock = leftBlkHeader.pblock;
    rightBlkHeader.lblock = leftBlkNum;
    rightBlkHeader.rblock = leftBlkHeader.rblock;
    rightBlk.setHeader(&rightBlkHeader);

    // 6. If leftBlk had an existing right sibling, update its backward pointer
    if (rightBlkHeader.rblock != -1) {
        IndLeaf siblingBlk(rightBlkHeader.rblock);
        HeadInfo siblingHeader;
        siblingBlk.getHeader(&siblingHeader);
        siblingHeader.lblock = rightBlkNum;
        siblingBlk.setHeader(&siblingHeader);
    }

    // 7. Configure left block header and write back
    leftBlkHeader.numEntries = (MAX_KEYS_LEAF + 1) / 2; // 32 entries
    leftBlkHeader.rblock = rightBlkNum;
    leftBlk.setHeader(&leftBlkHeader);

    // 8. Distribute entries: first 32 to left block, next 32 to right block
    for (int i = 0; i < 32; ++i) {
        leftBlk.setEntry(&indices[i], i);
        rightBlk.setEntry(&indices[i + 32], i);
    }

    return rightBlkNum;
}

int BPlusTree::insertIntoInternal(int relId, char attrName[ATTR_SIZE], int intBlockNum, InternalEntry intEntry) {
    // Get the attribute cache entry corresponding to attrName
    AttrCatEntry attrCatEntry;
    int retVal = AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntry);
    if (retVal != SUCCESS) {
        return retVal;
    }

    // Declare an instance of IndInternal for intBlockNum using Constructor 1 (existing block)
    IndInternal intBlk(intBlockNum);

    // Load blockHeader with the header of intBlk
    HeadInfo blockHeader;
    intBlk.getHeader(&blockHeader);

    // Declare array to store all existing entries + the new entry
    InternalEntry internalEntries[blockHeader.numEntries + 1];

    /* Iterate through all entries in the block and insert `intEntry`
          at the appropriate position to maintain ascending order. */
    int i = 0;
    bool inserted = false;

    for (int oldIdx = 0; oldIdx < blockHeader.numEntries; ++oldIdx) {
        InternalEntry currEntry;
        intBlk.getEntry(&currEntry, oldIdx);

        int cmpVal = compareAttrs(currEntry.attrVal, intEntry.attrVal, attrCatEntry.attrType);

        if (!inserted && cmpVal > 0) {
            internalEntries[i++] = intEntry;
            inserted = true;
        }

        internalEntries[i++] = currEntry;
    }

    // If intEntry is greater than all existing entries in the block
    if (!inserted) {
        internalEntries[i++] = intEntry;
    }

    /* Update the child pointer chaining:
          In an internal node, adjacent entries share child pointers.
          Find the position where intEntry was inserted, and ensure the
          entry immediately following it has its lChild set to intEntry.rChild.
    */
    for (int idx = 0; idx < blockHeader.numEntries; ++idx) {
        internalEntries[idx + 1].lChild = internalEntries[idx].rChild;
    }

    // Case 1: Internal node has NOT reached maximum capacity (fits without splitting)
    if (blockHeader.numEntries != MAX_KEYS_INTERNAL) {
        // Increment numEntries in header and write back
        blockHeader.numEntries++;
        intBlk.setHeader(&blockHeader);

        // Populate entries in the block using IndInternal::setEntry()
        for (int idx = 0; idx < blockHeader.numEntries; ++idx) {
            intBlk.setEntry(&internalEntries[idx], idx);
        }

        return SUCCESS;
    }

    // Case 2: Internal node is FULL. Split entries across two internal blocks.
    int newRightBlk = splitInternal(intBlockNum, internalEntries);

    if (newRightBlk == E_DISKFULL) {
        // Disk full: destroy the dangling right subtree rooted at intEntry.rChild
        bPlusDestroy(intEntry.rChild);
        return E_DISKFULL;
    }

    // Propagate the middle entry upward
    if (blockHeader.pblock != -1) {
        /* The current block was not the root. Insert the middle value
           at index MIDDLE_INDEX_INTERNAL (index 50) into the parent block recursively. */
        InternalEntry parentEntry;
        parentEntry.lChild = intBlockNum;
        parentEntry.rChild = newRightBlk;
        parentEntry.attrVal = internalEntries[MIDDLE_INDEX_INTERNAL].attrVal;

        return insertIntoInternal(relId, attrName, blockHeader.pblock, parentEntry);

    } else {
        /* The current block was the root block and has now split.
           A new root node needs to be allocated and configured. */
        return createNewRoot(relId, attrName,
                               internalEntries[MIDDLE_INDEX_INTERNAL].attrVal,
                               intBlockNum, newRightBlk);
    }
}

int BPlusTree::splitInternal(int intBlockNum, InternalEntry internalEntries[]) {
    // Declare rightBlk using Constructor 2 (no arguments) to allocate a new internal block
    IndInternal rightBlk;

    // Declare leftBlk using Constructor 1 to bind to the existing internal block
    IndInternal leftBlk(intBlockNum);

    int rightBlkNum = rightBlk.getBlockNum();
    int leftBlkNum = intBlockNum;

    // Check if block allocation succeeded
    if (rightBlkNum == E_DISKFULL) {
        return E_DISKFULL;
    }

    // Retrieve the headers of both the left block and right block
    HeadInfo leftBlkHeader, rightBlkHeader;
    leftBlk.getHeader(&leftBlkHeader);
    rightBlk.getHeader(&rightBlkHeader);

    // Configure right block header and write back
    rightBlkHeader.numEntries = MAX_KEYS_INTERNAL / 2; // 50 entries
    rightBlkHeader.pblock = leftBlkHeader.pblock;
    rightBlk.setHeader(&rightBlkHeader);

    // Configure left block header and write back
    leftBlkHeader.numEntries = MAX_KEYS_INTERNAL / 2; // 50 entries
    leftBlk.setHeader(&leftBlkHeader);

    /*    Distribute entries:
          - Left block gets indices 0 to 49 (first 50 entries).
          - Index 50 (MIDDLE_INDEX_INTERNAL) is promoted up to the parent.
          - Right block gets indices 51 to 100 (next 50 entries, shifted to 0..49).
    */
    for (int i = 0; i < 50; ++i) {
        leftBlk.setEntry(&internalEntries[i], i);
        rightBlk.setEntry(&internalEntries[i + 51], i);
    }

    /*    Update parent pointers (pblock) of all children moved to the new right block.
          An internal node with 50 keys has 51 children:
          - lChild of entry 0 (which is internalEntries[51].lChild)
          - rChild of all 50 entries (i = 0 to 49 of rightBlk)
    */
    int type = StaticBuffer::getStaticBlockType(internalEntries[51].lChild);

    if (type == IND_INTERNAL) {
        // First child of right block
        IndInternal childBlk(internalEntries[51].lChild);
        HeadInfo childHeader;
        childBlk.getHeader(&childHeader);
        childHeader.pblock = rightBlkNum;
        childBlk.setHeader(&childHeader);

        // Remaining 50 children of right block
        for (int i = 0; i < 50; ++i) {
            InternalEntry entry;
            rightBlk.getEntry(&entry, i);

            IndInternal child(entry.rChild);
            HeadInfo header;
            child.getHeader(&header);
            header.pblock = rightBlkNum;
            child.setHeader(&header);
        }
    } else if (type == IND_LEAF) {
        // First child of right block
        IndLeaf childBlk(internalEntries[51].lChild);
        HeadInfo childHeader;
        childBlk.getHeader(&childHeader);
        childHeader.pblock = rightBlkNum;
        childBlk.setHeader(&childHeader);

        // Remaining 50 children of right block
        for (int i = 0; i < 50; ++i) {
            InternalEntry entry;
            rightBlk.getEntry(&entry, i);

            IndLeaf child(entry.rChild);
            HeadInfo header;
            child.getHeader(&header);
            header.pblock = rightBlkNum;
            child.setHeader(&header);
        }
    }

    return rightBlkNum;
}

int BPlusTree::createNewRoot(int relId, char attrName[ATTR_SIZE], Attribute attrVal, int lChild, int rChild) {
    // Get the attribute cache entry corresponding to attrName
    AttrCatEntry attrCatEntry;
    int retVal = AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntry);
    if (retVal != SUCCESS) {
        return retVal;
    }

    // Allocate a new internal index block using Constructor 2 (no arguments)
    IndInternal newRootBlk;
    int newRootBlkNum = newRootBlk.getBlockNum();

    // Handle disk exhaustion
    if (newRootBlkNum == E_DISKFULL) {
        /* Failed to obtain a new block. Destroy the right subtree rooted at rChild 
           which was newly split and not yet connected to the main tree. */
        bPlusDestroy(rChild);
        return E_DISKFULL;
    }

    // Update the header of the new root block with numEntries = 1
    HeadInfo newRootHeader;
    newRootBlk.getHeader(&newRootHeader);
    newRootHeader.numEntries = 1;
    newRootBlk.setHeader(&newRootHeader);

    // Populate the single entry of the new root
    InternalEntry entry;
    entry.lChild = lChild;
    entry.attrVal = attrVal;
    entry.rChild = rChild;
    newRootBlk.setEntry(&entry, 0);

    /*    Update the parent pointers (pblock) of lChild and rChild to newRootBlkNum.
          Determine whether children are internal nodes or leaf nodes. */
    int leftChildType = StaticBuffer::getStaticBlockType(lChild);

    if (leftChildType == IND_INTERNAL) {
        IndInternal leftChildBlk(lChild);
        HeadInfo leftHeader;
        leftChildBlk.getHeader(&leftHeader);
        leftHeader.pblock = newRootBlkNum;
        leftChildBlk.setHeader(&leftHeader);

        IndInternal rightChildBlk(rChild);
        HeadInfo rightHeader;
        rightChildBlk.getHeader(&rightHeader);
        rightHeader.pblock = newRootBlkNum;
        rightChildBlk.setHeader(&rightHeader);
    } else {
        // leftChildType == IND_LEAF
        IndLeaf leftChildBlk(lChild);
        HeadInfo leftHeader;
        leftChildBlk.getHeader(&leftHeader);
        leftHeader.pblock = newRootBlkNum;
        leftChildBlk.setHeader(&leftHeader);

        IndLeaf rightChildBlk(rChild);
        HeadInfo rightHeader;
        rightChildBlk.getHeader(&rightHeader);
        rightHeader.pblock = newRootBlkNum;
        rightChildBlk.setHeader(&rightHeader);
    }

    // Update rootBlock in the attribute cache entry and mark it dirty
    attrCatEntry.rootBlock = newRootBlkNum;
    AttrCacheTable::setAttrCatEntry(relId, attrName, &attrCatEntry);

    return SUCCESS;
}