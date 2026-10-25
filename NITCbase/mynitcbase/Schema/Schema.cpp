#include "Schema.h"

#include <cmath>
#include <cstring>


int Schema::openRel(char relName[ATTR_SIZE]) {
  int ret = OpenRelTable::openRel(relName);

  // the OpenRelTable::openRel() function returns the rel-id if successful
  // a valid rel-id will be within the range 0 <= relId < MAX_OPEN and any
  // error codes will be negative
  if(ret >= 0){
    return SUCCESS;
  }

  //otherwise it returns an error message
  return ret;
}

int Schema::closeRel(char relName[ATTR_SIZE]) {
  if (strcmp(relName,RELCAT_RELNAME)==0||strcmp(relName,ATTRCAT_RELNAME)==0) {
    return E_NOTPERMITTED;
  }

  // this function returns the rel-id of a relation if it is open or
  // E_RELNOTOPEN if it is not. we will implement this later.
  int relId = OpenRelTable::getRelId(relName);

  if (relId==E_RELNOTOPEN) {
    return E_RELNOTOPEN;
  }

  return OpenRelTable::closeRel(relId);
}

int Schema::renameRel(char oldRelName[ATTR_SIZE], char newRelName[ATTR_SIZE]) {
  if ( strcmp(oldRelName, RELCAT_RELNAME) == 0 || strcmp(oldRelName, ATTRCAT_RELNAME) == 0 || strcmp(newRelName, RELCAT_RELNAME) == 0 || strcmp(newRelName, ATTRCAT_RELNAME) == 0){
    return E_NOTPERMITTED;
  }

  int relId = OpenRelTable::getRelId(oldRelName);
  if ( relId != E_RELNOTOPEN ){
    return E_RELOPEN;
  }

  int retVal = BlockAccess::renameRelation(oldRelName, newRelName);
  return retVal;
}

int Schema::renameAttr(char *relName, char *oldAttrName, char *newAttrName){  
  if ( strcmp(relName, RELCAT_RELNAME) == 0 || strcmp(relName, ATTRCAT_RELNAME) == 0 ){
    return E_NOTPERMITTED;
  }

  int relId = OpenRelTable::getRelId(relName);
  if ( relId != E_RELNOTOPEN ){
    return E_RELOPEN;
  }

  int retVal = BlockAccess::renameAttribute(relName, oldAttrName, newAttrName);
  return retVal;
}

int Schema::createRel(char relName[], int nAttrs, char attrs[][ATTR_SIZE], int attrtype[]){
  Attribute relNameAsAttribute;
  strcpy(relNameAsAttribute.sVal,relName);

  RecId targetRelId;

  RelCacheTable::resetSearchIndex(RELCAT_RELID);
  targetRelId = BlockAccess::linearSearch(RELCAT_RELID,(char*)RELCAT_ATTR_RELNAME,relNameAsAttribute,EQ);

  if(targetRelId.block != -1 && targetRelId.slot != -1) {
    return E_RELEXIST;
  }

  for(int i = 0;i<nAttrs;i++){
    for(int j = i+1;j<nAttrs;j++){
      if(strcmp(attrs[i],attrs[j])==0){
        return E_DUPLICATEATTR;
      }
    }
  }

  Attribute relCatRecord[RELCAT_NO_ATTRS];
  strcpy(relCatRecord[RELCAT_REL_NAME_INDEX].sVal,relName);
  relCatRecord[RELCAT_NO_ATTRIBUTES_INDEX].nVal = nAttrs;
  relCatRecord[RELCAT_NO_RECORDS_INDEX].nVal = 0;
  relCatRecord[RELCAT_FIRST_BLOCK_INDEX].nVal = -1;
  relCatRecord[RELCAT_LAST_BLOCK_INDEX].nVal = -1;
  relCatRecord[RELCAT_NO_SLOTS_PER_BLOCK_INDEX].nVal = floor((2016.0 / (16.0 * nAttrs + 1.0)));

  int retVal = BlockAccess::insert(RELCAT_RELID, relCatRecord);
  if (retVal != SUCCESS){
      return retVal;
  }

  for(int i = 0;i<nAttrs;i++){
    Attribute attrCatRecord[ATTRCAT_NO_ATTRS];
    strcpy(attrCatRecord[ATTRCAT_REL_NAME_INDEX].sVal, relName);
    strcpy(attrCatRecord[ATTRCAT_ATTR_NAME_INDEX].sVal, attrs[i]);
    attrCatRecord[ATTRCAT_ATTR_TYPE_INDEX].nVal = attrtype[i];
    attrCatRecord[ATTRCAT_PRIMARY_FLAG_INDEX].nVal = -1;
    attrCatRecord[ATTRCAT_ROOT_BLOCK_INDEX].nVal = -1;
    attrCatRecord[ATTRCAT_OFFSET_INDEX].nVal = i;

    retVal = BlockAccess::insert(ATTRCAT_RELID,attrCatRecord);
    if(retVal!=SUCCESS){
      Schema::deleteRel(relName);
      return E_DISKFULL;
    }
  }
  return SUCCESS;
}

int Schema::deleteRel(char*relName){
  if(strcmp(relName,RELCAT_RELNAME)==0||strcmp(relName,ATTRCAT_RELNAME)==0){
    return E_NOTPERMITTED;
  }

  int relId = OpenRelTable::getRelId(relName);
  if(relId!=E_RELNOTOPEN){
    return E_RELOPEN;
  }

  return BlockAccess::deleteRelation(relName);
}

int Schema::createIndex(char relName[ATTR_SIZE], char attrName[ATTR_SIZE]) {
  if (strcmp(relName, RELCAT_RELNAME) == 0 || strcmp(relName, ATTRCAT_RELNAME) == 0) {
    return E_NOTPERMITTED;
  }

  int relId = OpenRelTable::getRelId(relName);

  if (relId == E_RELNOTOPEN) {
    return E_RELNOTOPEN;
  }
  
  return BPlusTree::bPlusCreate(relId, attrName);
}

int Schema::dropIndex(char relName[ATTR_SIZE], char attrName[ATTR_SIZE]) {
  if (strcmp(relName, RELCAT_RELNAME) == 0 || strcmp(relName, ATTRCAT_RELNAME) == 0) {
    return E_NOTPERMITTED;
  }

  int relId = OpenRelTable::getRelId(relName);

  if (relId == E_RELNOTOPEN) {
    return E_RELNOTOPEN;
  }

  // Fetch the attribute catalog entry corresponding to the attribute
  AttrCatEntry attrCatEntry;
  int ret = AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntry);
  if (ret != SUCCESS) {
    return E_ATTRNOTEXIST;
  }

  // Check if an index exists for this attribute
  int rootBlock = attrCatEntry.rootBlock;
  if (rootBlock == -1) {
    return E_NOINDEX;
  }

  BPlusTree::bPlusDestroy(rootBlock);

  // Update the rootBlock to -1 in the Attribute Cache entry and mark it dirty
  attrCatEntry.rootBlock = -1;
  AttrCacheTable::setAttrCatEntry(relId, attrName, &attrCatEntry);

  return SUCCESS;
}
