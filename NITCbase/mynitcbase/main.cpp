#include "Buffer/StaticBuffer.h"
#include "Cache/OpenRelTable.h"
#include "Disk_Class/Disk.h"
#include "FrontendInterface/FrontendInterface.h"
#include <cstring>
#include <iostream>

int main(int argc, char *argv[]) {
  Disk disk_run;
  StaticBuffer buffer;
  OpenRelTable cache;

  for(int i = 0;i<2;i++){
    RelCatEntry relCatBuf;
    int response = RelCacheTable::getRelCatEntry(i,&relCatBuf);
    if(response != SUCCESS){
        printf("Relation Catalog Entry not found.\n");
        exit(1);
    }
    printf("Relation: %s\n",relCatBuf.relName);
    for(int j = 0;j<relCatBuf.numAttrs;j++){
        AttrCatEntry attrCatBuf;
        response = AttrCacheTable::getAttrCatEntry(i,j,&attrCatBuf);
        if(response!=SUCCESS){
            printf("Attribute Catalog Entry not found.\n");
            exit(1);
        }
        const char *attrType = (attrCatBuf.attrType == NUMBER) ? "NUM" : "STR";
        printf("  %s: %s\n", attrCatBuf.attrName, attrType);
    }
    printf("\n");
  }

  /*
  for i = 0 and i = 1 (i.e RELCAT_RELID and ATTRCAT_RELID)

      get the relation catalog entry using RelCacheTable::getRelCatEntry()
      printf("Relation: %s\n", relname);

      for j = 0 to numAttrs of the relation - 1
          get the attribute catalog entry for (rel-id i, attribute offset j)
           in attrCatEntry using AttrCacheTable::getAttrCatEntry()

          printf("  %s: %s\n", attrName, attrType);
  */

  return 0;
}