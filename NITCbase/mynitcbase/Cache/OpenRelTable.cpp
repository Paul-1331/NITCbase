#include "OpenRelTable.h"

#include <cstring>
#include <cstdlib>

OpenRelTable::OpenRelTable() {

  // initialize relCache and attrCache with nullptr
  for (int i = 0; i < MAX_OPEN; ++i) {
    RelCacheTable::relCache[i] = nullptr;
    AttrCacheTable::attrCache[i] = nullptr;
  }

  /************ Setting up Relation Cache entries ************/
  // (we need to populate relation cache with entries for the relation catalog
  //  and attribute catalog.)

  /**** setting up Relation Catalog relation in the Relation Cache Table****/
  RecBuffer relCatBlock(RELCAT_BLOCK);

  Attribute relCatRecord[RELCAT_NO_ATTRS];
  relCatBlock.getRecord(relCatRecord, RELCAT_SLOTNUM_FOR_RELCAT);

  struct RelCacheEntry relCacheEntry;
  RelCacheTable::recordToRelCatEntry(relCatRecord, &relCacheEntry.relCatEntry);
  relCacheEntry.recId.block = RELCAT_BLOCK;
  relCacheEntry.recId.slot = RELCAT_SLOTNUM_FOR_RELCAT;

  // allocate this on the heap because we want it to persist outside this function
  RelCacheTable::relCache[RELCAT_RELID] = (struct RelCacheEntry*)malloc(sizeof(RelCacheEntry));
  *(RelCacheTable::relCache[RELCAT_RELID]) = relCacheEntry;



  /**** setting up Attribute Catalog relation in the Relation Cache Table ****/

  // set up the relation cache entry for the attribute catalog similarly
  // from the record at RELCAT_SLOTNUM_FOR_ATTRCAT
  relCatBlock.getRecord(relCatRecord,RELCAT_SLOTNUM_FOR_ATTRCAT);

  // set the value at RelCacheTable::relCache[ATTRCAT_RELID]
  RelCacheTable::recordToRelCatEntry(relCatRecord, &relCacheEntry.relCatEntry);
  relCacheEntry.recId.block = RELCAT_BLOCK;
  relCacheEntry.recId.slot = RELCAT_SLOTNUM_FOR_ATTRCAT;

  // allocate this on the heap because we want it to persist outside this function
  RelCacheTable::relCache[ATTRCAT_RELID] = (struct RelCacheEntry*)malloc(sizeof(RelCacheEntry));
  *(RelCacheTable::relCache[ATTRCAT_RELID]) = relCacheEntry;

  // --- Students Relation (Slot 2) ---
  relCatBlock.getRecord(relCatRecord, RELCAT_SLOTNUM_FOR_ATTRCAT + 1);
  RelCacheTable::recordToRelCatEntry(relCatRecord, &relCacheEntry.relCatEntry);
  relCacheEntry.recId.block = RELCAT_BLOCK;
  relCacheEntry.recId.slot = RELCAT_SLOTNUM_FOR_ATTRCAT + 1;

  RelCacheTable::relCache[ATTRCAT_RELID + 1] = (struct RelCacheEntry*)malloc(sizeof(RelCacheEntry));
  *(RelCacheTable::relCache[ATTRCAT_RELID + 1]) = relCacheEntry;


  /************ Setting up Attribute cache entries ************/
  // (we need to populate attribute cache with entries for the relation catalog
  //  and attribute catalog.)

  /**** setting up Relation Catalog relation in the Attribute Cache Table ****/
  RecBuffer attrCatBlock(ATTRCAT_BLOCK);
  Attribute attrCatRecord[RELCAT_NO_ATTRS];

  // iterate through all the attributes of the relation catalog and create a linked
  // list of AttrCacheEntry (slots 0 to 5)
  // for each of the entries, set
  //    attrCacheEntry.recId.block = ATTRCAT_BLOCK;
  //    attrCacheEntry.recId.slot = i   (0 to 5)
  //    and attrCacheEntry.next appropriately
  // NOTE: allocate each entry dynamically using malloc

  AttrCacheEntry*attrCacheEntry;
  AttrCacheEntry*listHead = nullptr;
  AttrCacheEntry*prev = nullptr;

  for(int j = 0;j<RELCAT_NO_ATTRS;j++){
    attrCatBlock.getRecord(attrCatRecord,j);
    attrCacheEntry = (AttrCacheEntry*)malloc(sizeof(AttrCacheEntry));
    if(j==0) listHead = attrCacheEntry;

    AttrCacheTable::recordToAttrCatEntry(attrCatRecord,&attrCacheEntry->attrCatEntry);
    attrCacheEntry->recId.block = ATTRCAT_BLOCK;
    attrCacheEntry->recId.slot = j;

    if(prev!=nullptr) prev->next = attrCacheEntry;
    prev = attrCacheEntry;
  }
  // set the next field in the last entry to nullptr
  attrCacheEntry->next = nullptr;

  AttrCacheTable::attrCache[RELCAT_RELID] = listHead;

  /**** setting up Attribute Catalog relation in the Attribute Cache Table ****/

  prev = nullptr;

  // set up the attributes of the attribute cache similarly.
  // read slots 6-11 from attrCatBlock and initialise recId appropriately

  for(int j = 6;j<6+ATTRCAT_NO_ATTRS;j++){
    attrCatBlock.getRecord(attrCatRecord,j);
    attrCacheEntry = (AttrCacheEntry*)malloc(sizeof(AttrCacheEntry));
    if(j==6) listHead = attrCacheEntry;

    AttrCacheTable::recordToAttrCatEntry(attrCatRecord,&attrCacheEntry->attrCatEntry);
    attrCacheEntry->recId.block = ATTRCAT_BLOCK;
    attrCacheEntry->recId.slot = j;

    if(prev!=nullptr) prev->next = attrCacheEntry;
    prev = attrCacheEntry;
  }
  attrCacheEntry->next = nullptr;

  // set the value at AttrCacheTable::attrCache[ATTRCAT_RELID]
  AttrCacheTable::attrCache[ATTRCAT_RELID] = listHead;

  prev = nullptr;
  int numStudentAttrs = RelCacheTable::relCache[ATTRCAT_RELID + 1]->relCatEntry.numAttrs;

  for (int j = 12; j < 12 + numStudentAttrs; j++) {
    attrCatBlock.getRecord(attrCatRecord, j);
    attrCacheEntry = (AttrCacheEntry*)malloc(sizeof(AttrCacheEntry));
    if (j == 12) listHead = attrCacheEntry;

    AttrCacheTable::recordToAttrCatEntry(attrCatRecord, &attrCacheEntry->attrCatEntry);
    attrCacheEntry->recId.block = ATTRCAT_BLOCK;
    attrCacheEntry->recId.slot = j;

    if (prev != nullptr) prev->next = attrCacheEntry;
    prev = attrCacheEntry;
  }
  attrCacheEntry->next = nullptr;
  AttrCacheTable::attrCache[ATTRCAT_RELID + 1] = listHead;
}

OpenRelTable::~OpenRelTable() {
  // free all the memory that you allocated in the constructor

  for(int i = 0;i<MAX_OPEN;i++){

    // free Relation Cache entry for slot i
    if(RelCacheTable::relCache[i]!=nullptr){
      free(RelCacheTable::relCache[i]);
      RelCacheTable::relCache[i] = nullptr;
    }

    // Traverse and free the linked list of Attribute Cache entries for slot i
    if(AttrCacheTable::attrCache[i]!=nullptr){
      AttrCacheEntry*curr = AttrCacheTable::attrCache[i];
      AttrCacheEntry*next = nullptr;
      while(curr!=nullptr){
        next = curr->next;
        free(curr);
        curr = next;
      }
      AttrCacheTable::attrCache[i] = nullptr;
    }
  }
}