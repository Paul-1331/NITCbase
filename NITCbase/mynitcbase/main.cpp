#include "Buffer/StaticBuffer.h"
#include "Cache/OpenRelTable.h"
#include "Disk_Class/Disk.h"
#include "FrontendInterface/FrontendInterface.h"
#include <cstring>
#include <iostream>

int main(int argc, char *argv[]) {
    Disk disk_run;

    RecBuffer relCatBuffer(RELCAT_BLOCK);
    HeadInfo relCatHeader;
    relCatBuffer.getHeader(&relCatHeader);

    // Loop through all relations in RelCat
    for (int i = 0; i < relCatHeader.numEntries; i++) {
        Attribute relCatRecord[RELCAT_NO_ATTRS];
        relCatBuffer.getRecord(relCatRecord, i);

        printf("Relation: %s\n", relCatRecord[RELCAT_REL_NAME_INDEX].sVal);

        // Traverse the AttrCat doubly-linked list
        int blockNum = ATTRCAT_BLOCK;
        while (blockNum != -1) {
            RecBuffer attrCatBuffer(blockNum);
            HeadInfo attrCatHeader;
            attrCatBuffer.getHeader(&attrCatHeader);

            for (int j = 0; j < attrCatHeader.numEntries; j++) {
                Attribute attrCatRecord[ATTRCAT_NO_ATTRS];
                attrCatBuffer.getRecord(attrCatRecord, j);

                // Print column details if relation names match
                if (strcmp(attrCatRecord[ATTRCAT_REL_NAME_INDEX].sVal,relCatRecord[RELCAT_REL_NAME_INDEX].sVal) == 0) {
                    const char *attrType = (attrCatRecord[ATTRCAT_ATTR_TYPE_INDEX].nVal == NUMBER) ? "NUM" : "STR";
                    printf("  %s: %s\n", attrCatRecord[ATTRCAT_ATTR_NAME_INDEX].sVal, attrType);
                }
            }
            // Advance to next block in AttrCat chain
            blockNum = attrCatHeader.rblock;
        }
        printf("\n");
    }
    return 0;
}

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