#include "Buffer/StaticBuffer.h"
#include "Cache/OpenRelTable.h"
#include "Disk_Class/Disk.h"
#include "FrontendInterface/FrontendInterface.h"
#include <cstring>
#include <iostream>

// Qn2. To update the Schema of Students relation, change the name of Class to Batch
int main(int argc, char *argv[]) {
    Disk disk_run;

    RecBuffer relCatBuffer(RELCAT_BLOCK);
    HeadInfo relCatHeader;
    relCatBuffer.getHeader(&relCatHeader);

    // Find "Student" relation and rename "Class" attribute to "Batch"
    int blockNum = ATTRCAT_BLOCK;
    bool updated = false;

    while (blockNum != -1 && !updated) {
        RecBuffer attrCatBuffer(blockNum);
        HeadInfo attrCatHeader;
        attrCatBuffer.getHeader(&attrCatHeader);

        for (int j = 0; j < attrCatHeader.numEntries; j++) {
            Attribute attrCatRecord[ATTRCAT_NO_ATTRS];
            attrCatBuffer.getRecord(attrCatRecord, j);

            // Match table "Student" (or "Students") and attribute "Class"
            if ((strcmp(attrCatRecord[ATTRCAT_REL_NAME_INDEX].sVal, "Student") == 0 ||
                 strcmp(attrCatRecord[ATTRCAT_REL_NAME_INDEX].sVal, "Students") == 0) &&
                strcmp(attrCatRecord[ATTRCAT_ATTR_NAME_INDEX].sVal, "Class") == 0) {

                unsigned char buffer[BLOCK_SIZE];
                // Read the CURRENT block where this attribute was found
                Disk::readBlock(buffer, blockNum);

                int slotCount = attrCatHeader.numSlots;
                int attrCount = attrCatHeader.numAttrs;
                int recordSize = attrCount * ATTR_SIZE;

                // Calculate pointer to AttrName field (Offset = HEADER_SIZE + slotMap + (recordSize * j) + 16)
                unsigned char *slotPointer = buffer + HEADER_SIZE + slotCount + (recordSize * j) + ATTR_SIZE;

                char newAttrName[] = "Batch";
                memset(slotPointer, 0, ATTR_SIZE); // Clear old attribute string
                strcpy((char *)slotPointer, newAttrName);

                // Write modified block back to disk
                Disk::writeBlock(buffer, blockNum);
                updated = true;
                break;
            }
        }
        blockNum = attrCatHeader.rblock;
    }

    // Print relation catalog again to confirm the updated schema
    for (int i = 0; i < relCatHeader.numEntries; i++) {
        Attribute relCatRecord[RELCAT_NO_ATTRS];
        relCatBuffer.getRecord(relCatRecord, i);

        printf("Relation: %s\n", relCatRecord[RELCAT_REL_NAME_INDEX].sVal);

        int currentBlock = ATTRCAT_BLOCK;
        while (currentBlock != -1) {
            RecBuffer attrCatBuffer(currentBlock);
            HeadInfo attrCatHeader;
            attrCatBuffer.getHeader(&attrCatHeader);

            for (int j = 0; j < attrCatHeader.numEntries; j++) {
                Attribute attrCatRecord[ATTRCAT_NO_ATTRS];
                attrCatBuffer.getRecord(attrCatRecord, j);

                if (strcmp(attrCatRecord[ATTRCAT_REL_NAME_INDEX].sVal,relCatRecord[RELCAT_REL_NAME_INDEX].sVal) == 0) {
                    const char *attrType = (attrCatRecord[ATTRCAT_ATTR_TYPE_INDEX].nVal == NUMBER) ? "NUM" : "STR";
                    printf("  %s: %s\n", attrCatRecord[ATTRCAT_ATTR_NAME_INDEX].sVal, attrType);
                }
            }
            currentBlock = attrCatHeader.rblock;
        }
        printf("\n");
    }
    return 0;
}

// Qn1. To read across multiple blocks of the attribute catalog
// int main(int argc, char *argv[]) {
//     Disk disk_run;

//     RecBuffer relCatBuffer(RELCAT_BLOCK);
//     HeadInfo relCatHeader;
//     relCatBuffer.getHeader(&relCatHeader);

//     // Loop through all relations in RelCat
//     for (int i = 0; i < relCatHeader.numEntries; i++) {
//         Attribute relCatRecord[RELCAT_NO_ATTRS];
//         relCatBuffer.getRecord(relCatRecord, i);

//         printf("Relation: %s\n", relCatRecord[RELCAT_REL_NAME_INDEX].sVal);

//         // Traverse the AttrCat doubly-linked list
//         int blockNum = ATTRCAT_BLOCK;
//         while (blockNum != -1) {
//             RecBuffer attrCatBuffer(blockNum);
//             HeadInfo attrCatHeader;
//             attrCatBuffer.getHeader(&attrCatHeader);

//             for (int j = 0; j < attrCatHeader.numEntries; j++) {
//                 Attribute attrCatRecord[ATTRCAT_NO_ATTRS];
//                 attrCatBuffer.getRecord(attrCatRecord, j);

//                 // Print column details if relation names match
//                 if (strcmp(attrCatRecord[ATTRCAT_REL_NAME_INDEX].sVal,relCatRecord[RELCAT_REL_NAME_INDEX].sVal) == 0) {
//                     const char *attrType = (attrCatRecord[ATTRCAT_ATTR_TYPE_INDEX].nVal == NUMBER) ? "NUM" : "STR";
//                     printf("  %s: %s\n", attrCatRecord[ATTRCAT_ATTR_NAME_INDEX].sVal, attrType);
//                 }
//             }
//             // Advance to next block in AttrCat chain
//             blockNum = attrCatHeader.rblock;
//         }
//         printf("\n");
//     }
//     return 0;
// }

// int main(int argc, char *argv[]) {
//   Disk disk_run;
  
//   // create objects for the relation catalog and attribute catalog
//   RecBuffer relCatBuffer(RELCAT_BLOCK);
//   RecBuffer attrCatBuffer(ATTRCAT_BLOCK);

//   HeadInfo relCatHeader;
//   HeadInfo attrCatHeader;

//   // load the headers of both blocks into the relCatHeader and the attrCatHeader
//   // (we will implement these functions later)
//   relCatBuffer.getHeader(&relCatHeader);
//   attrCatBuffer.getHeader(&attrCatHeader);

//   for(int i = 0;i<relCatHeader.numEntries;i++){ // total relation count
//     Attribute relCatRecord[RELCAT_NO_ATTRS];  // will store the record from the relation catalog
    
//     relCatBuffer.getRecord(relCatRecord,i);
    
//     printf("Relation: %s\n", relCatRecord[RELCAT_REL_NAME_INDEX].sVal);

//     for(int j = 0;j<attrCatHeader.numEntries;j++){

//       // declare attrCatRecord and load the attribute catalog entry into it
//       Attribute attrCatRecord[ATTRCAT_NO_ATTRS];
//       attrCatBuffer.getRecord(attrCatRecord,j);

//       if(strcmp(attrCatRecord[ATTRCAT_REL_NAME_INDEX].sVal,relCatRecord[RELCAT_REL_NAME_INDEX].sVal)==0){
//         const char*attrType = attrCatRecord[ATTRCAT_ATTR_TYPE_INDEX].nVal == NUMBER ? "NUM": "STR";

//         printf("  %s: %s\n", attrCatRecord[ATTRCAT_ATTR_NAME_INDEX].sVal,attrType);
//       }
//     }
//     printf("\n");
//   }
  
//   return 0;
// }