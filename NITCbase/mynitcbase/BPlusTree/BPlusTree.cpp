#include "BPlusTree.h"
#include <cstring>

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

        /* Only for NE operation do we have to check the entire linked list;
           for all other ops it is guaranteed that the block being searched
           will have an entry, if it exists, satisfying that op. */
        if (op != NE) {
            break;
        }

        // Move to the next block in the linked list
        block = leafHead.rblock;
        index = 0;
    }

    // No entry satisfying the op was found; return RecId {-1, -1}
    return RecId{-1, -1};
}