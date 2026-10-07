#include "OpenRelTable.h"

#include <cstring>
#include <cstdlib>

OpenRelTableMetaInfo OpenRelTable::tableMetaInfo[MAX_OPEN];

OpenRelTable::OpenRelTable() {

  // Initialize relCache, attrCache to nullptr and tableMetaInfo to free
  for (int i = 0; i < MAX_OPEN; ++i) {
    RelCacheTable::relCache[i] = nullptr;
    AttrCacheTable::attrCache[i] = nullptr;
    tableMetaInfo[i].free = true;
  }

  /************ Setting up Relation Cache entries ************/
  RecBuffer relCatBlock(RELCAT_BLOCK);
  Attribute relCatRecord[RELCAT_NO_ATTRS];
  RelCacheEntry relCacheEntry;

  /**** setting up Relation Catalog relation in the Relation Cache Table ****/
  relCatBlock.getRecord(relCatRecord, RELCAT_SLOTNUM_FOR_RELCAT);
  RelCacheTable::recordToRelCatEntry(relCatRecord, &relCacheEntry.relCatEntry);
  relCacheEntry.recId.block = RELCAT_BLOCK;
  relCacheEntry.recId.slot = RELCAT_SLOTNUM_FOR_RELCAT;
  relCacheEntry.searchIndex = {-1, -1};

  RelCacheTable::relCache[RELCAT_RELID] = (RelCacheEntry*)malloc(sizeof(RelCacheEntry));
  *(RelCacheTable::relCache[RELCAT_RELID]) = relCacheEntry;

  /**** setting up Attribute Catalog relation in the Relation Cache Table ****/
  relCatBlock.getRecord(relCatRecord, RELCAT_SLOTNUM_FOR_ATTRCAT);
  RelCacheTable::recordToRelCatEntry(relCatRecord, &relCacheEntry.relCatEntry);
  relCacheEntry.recId.block = RELCAT_BLOCK;
  relCacheEntry.recId.slot = RELCAT_SLOTNUM_FOR_ATTRCAT;
  relCacheEntry.searchIndex = {-1, -1};

  RelCacheTable::relCache[ATTRCAT_RELID] = (RelCacheEntry*)malloc(sizeof(RelCacheEntry));
  *(RelCacheTable::relCache[ATTRCAT_RELID]) = relCacheEntry;

  /************ Setting up Attribute Cache entries ************/
  RecBuffer attrCatBlock(ATTRCAT_BLOCK);
  Attribute attrCatRecord[ATTRCAT_NO_ATTRS];

  AttrCacheEntry* attrCacheEntry = nullptr;
  AttrCacheEntry* listHead = nullptr;
  AttrCacheEntry* prev = nullptr;

  // Relation Catalog Attributes (Slots 0 to 5)
  for (int j = 0; j < RELCAT_NO_ATTRS; j++) {
    attrCatBlock.getRecord(attrCatRecord, j);
    attrCacheEntry = (AttrCacheEntry*)malloc(sizeof(AttrCacheEntry));
    if (j == 0) listHead = attrCacheEntry;

    AttrCacheTable::recordToAttrCatEntry(attrCatRecord, &attrCacheEntry->attrCatEntry);
    attrCacheEntry->recId.block = ATTRCAT_BLOCK;
    attrCacheEntry->recId.slot = j;
    attrCacheEntry->next = nullptr;

    if (prev != nullptr) prev->next = attrCacheEntry;
    prev = attrCacheEntry;
  }
  AttrCacheTable::attrCache[RELCAT_RELID] = listHead;

  // Attribute Catalog Attributes (Slots 6 to 11)
  listHead = nullptr;
  prev = nullptr;

  for (int j = 6; j < 6 + ATTRCAT_NO_ATTRS; j++) {
    attrCatBlock.getRecord(attrCatRecord, j);
    attrCacheEntry = (AttrCacheEntry*)malloc(sizeof(AttrCacheEntry));
    if (j == 6) listHead = attrCacheEntry;

    AttrCacheTable::recordToAttrCatEntry(attrCatRecord, &attrCacheEntry->attrCatEntry);
    attrCacheEntry->recId.block = ATTRCAT_BLOCK;
    attrCacheEntry->recId.slot = j;
    attrCacheEntry->next = nullptr;

    if (prev != nullptr) prev->next = attrCacheEntry;
    prev = attrCacheEntry;
  }
  AttrCacheTable::attrCache[ATTRCAT_RELID] = listHead;

  /************ Setting up tableMetaInfo entries ************/
  tableMetaInfo[RELCAT_RELID].free = false;
  strcpy(tableMetaInfo[RELCAT_RELID].relName, RELCAT_RELNAME);

  tableMetaInfo[ATTRCAT_RELID].free = false;
  strcpy(tableMetaInfo[ATTRCAT_RELID].relName, ATTRCAT_RELNAME);
}

OpenRelTable::~OpenRelTable() {
  // 1. Close all open user relations from rel-id = 2 onwards
  for (int i = 2; i < MAX_OPEN; i++) {
    if (!tableMetaInfo[i].free) {
      OpenRelTable::closeRel(i);
    }
  }

  // 2. Free memory allocated for catalog relations (slots 0 and 1)
  for (int i = 0; i < 2; i++) {
    if (RelCacheTable::relCache[i] != nullptr) {
      free(RelCacheTable::relCache[i]);
      RelCacheTable::relCache[i] = nullptr;
    }

    if (AttrCacheTable::attrCache[i] != nullptr) {
      AttrCacheEntry* curr = AttrCacheTable::attrCache[i];
      AttrCacheEntry* next = nullptr;
      while (curr != nullptr) {
        next = curr->next;
        free(curr);
        curr = next;
      }
      AttrCacheTable::attrCache[i] = nullptr;
    }
  }
}

int OpenRelTable::getRelId(char relName[ATTR_SIZE]) {
  for (int i = 0; i < MAX_OPEN; ++i) {
    if (!tableMetaInfo[i].free && strcmp(tableMetaInfo[i].relName, relName) == 0) {
      return i;
    }
  }
  return E_RELNOTOPEN;
}

int OpenRelTable::getFreeOpenRelTableEntry() {
  for (int i = 2; i < MAX_OPEN; ++i) {
    if (tableMetaInfo[i].free) {
      return i;
    }
  }
  return E_CACHEFULL;
}

int OpenRelTable::openRel(char relName[ATTR_SIZE]) {
  int relId = OpenRelTable::getRelId(relName);
  if (relId >= 0) {
    return relId;
  }

  relId = OpenRelTable::getFreeOpenRelTableEntry();
  if (relId == E_CACHEFULL) {
    return E_CACHEFULL;
  }

  /****** Setting up Relation Cache entry for the relation ******/
  RelCacheTable::resetSearchIndex(RELCAT_RELID);

  Attribute relNameAttr;
  strcpy(relNameAttr.sVal, relName);

  RecId relcatRecId = BlockAccess::linearSearch(RELCAT_RELID, (char*)RELCAT_ATTR_RELNAME, relNameAttr, EQ);

  if (relcatRecId.block == -1 && relcatRecId.slot == -1) {
    return E_RELNOTEXIST;
  }

  RecBuffer relCatBlock(relcatRecId.block);
  Attribute relCatRecord[RELCAT_NO_ATTRS];
  relCatBlock.getRecord(relCatRecord, relcatRecId.slot);

  RelCacheEntry* relCacheEntry = (RelCacheEntry*)malloc(sizeof(RelCacheEntry));
  RelCacheTable::recordToRelCatEntry(relCatRecord, &relCacheEntry->relCatEntry);
  relCacheEntry->recId = relcatRecId;
  relCacheEntry->searchIndex = {-1, -1};

  RelCacheTable::relCache[relId] = relCacheEntry;

  /****** Setting up Attribute Cache entry for the relation ******/
  AttrCacheEntry* listHead = nullptr;
  AttrCacheEntry* attrCacheEntry = nullptr;
  AttrCacheEntry* prev = nullptr;

  RelCacheTable::resetSearchIndex(ATTRCAT_RELID);

  for (int i = 0; i < relCacheEntry->relCatEntry.numAttrs; ++i) {
    RecId attrcatRecId = BlockAccess::linearSearch(ATTRCAT_RELID, (char*)ATTRCAT_ATTR_RELNAME, relNameAttr, EQ);

    RecBuffer attrCatBlock(attrcatRecId.block);
    Attribute attrCatRecord[ATTRCAT_NO_ATTRS];
    attrCatBlock.getRecord(attrCatRecord, attrcatRecId.slot);

    attrCacheEntry = (AttrCacheEntry*)malloc(sizeof(AttrCacheEntry));
    if (i == 0) {
      listHead = attrCacheEntry;
    }

    AttrCacheTable::recordToAttrCatEntry(attrCatRecord, &attrCacheEntry->attrCatEntry);
    attrCacheEntry->recId = attrcatRecId;
    attrCacheEntry->next = nullptr;

    if (prev != nullptr) {
      prev->next = attrCacheEntry;
    }
    prev = attrCacheEntry;
  }

  AttrCacheTable::attrCache[relId] = listHead;

  /****** Setting up metadata in the Open Relation Table ******/
  tableMetaInfo[relId].free = false;
  strcpy(tableMetaInfo[relId].relName, relName);

  return relId;
}

int OpenRelTable::closeRel(int relId) {
  if (relId == RELCAT_RELID || relId == ATTRCAT_RELID) {
    return E_NOTPERMITTED;
  }

  if (relId < 0 || relId >= MAX_OPEN) {
    return E_OUTOFBOUND;
  }

  if (tableMetaInfo[relId].free) {
    return E_RELNOTOPEN;
  }

  if (RelCacheTable::relCache[relId] != nullptr) {
    if(RelCacheTable::relCache[relId]->dirty){
      Attribute record[RELCAT_NO_ATTRS];
      RelCacheTable::relCatEntryToRecord(&(RelCacheTable::relCache[relId]->relCatEntry),record);

      RecId recId = RelCacheTable::relCache[relId]->recId;
      // declaring an object of RecBuffer class to write back to the buffer
      RecBuffer relCatBlock(recId.block);
      // Write back to the buffer using relCatBlock.setRecord() with recId.slot
      relCatBlock.setRecord(record,recId.slot);
    }

    free(RelCacheTable::relCache[relId]);
    RelCacheTable::relCache[relId] = nullptr;
  }

  if (AttrCacheTable::attrCache[relId] != nullptr) {
    AttrCacheEntry* curr = AttrCacheTable::attrCache[relId];
    AttrCacheEntry* next = nullptr;
    while (curr != nullptr) {
      next = curr->next;
      free(curr);
      curr = next;
    }
    AttrCacheTable::attrCache[relId] = nullptr;
  }

  tableMetaInfo[relId].free = true;
  return SUCCESS;
}