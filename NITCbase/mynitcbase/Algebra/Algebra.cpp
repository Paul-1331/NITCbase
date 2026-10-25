#include "Algebra.h"
#include "../BPlusTree/BPlusTree.h"

#include <cstring>
#include <cstdio>
#include <cstdlib>

// will return if a string can be parsed as a floating point number
bool isNumber(char *str) {
  int len;
  float ignore;
  /*
    sscanf returns the number of elements read, so if there is no float matching
    the first %f, ret will be 0, else it'll be 1

    %n gets the number of characters read. this scanf sequence will read the
    first float ignoring all the whitespace before and after. and the number of
    characters read that far will be stored in len. if len == strlen(str), then
    the string only contains a float with/without whitespace. else, there's other
    characters.
  */
  int ret = sscanf(str, "%f %n", &ignore, &len);
  return ret == 1 && len == strlen(str);
}

int Algebra::insert(char relName[ATTR_SIZE], int nAttrs, char record[][ATTR_SIZE]){
  if (strcmp(relName, RELCAT_RELNAME) == 0 || strcmp(relName, ATTRCAT_RELNAME) == 0) {
      return E_NOTPERMITTED;
  }

  int relId = OpenRelTable::getRelId(relName);

  if (relId == E_RELNOTOPEN) {
      return E_RELNOTOPEN;
  }

  // get the relation catalog entry from relation cache
  RelCatEntry relCatEntry;
  int ret = RelCacheTable::getRelCatEntry(relId, &relCatEntry);
  if (ret != SUCCESS) {
      return ret;
  }

  if (relCatEntry.numAttrs != nAttrs) {
      return E_NATTRMISMATCH;
  }

  Attribute recordValues[nAttrs];

  /*
    Converting 2D char array of record values to Attribute array recordValues
  */
  for(int i = 0;i<nAttrs;i++)
  {
      // get the attr-cat entry for the i'th attribute from the attr-cache
      AttrCatEntry attrCatEntry;
      ret = AttrCacheTable::getAttrCatEntry(relId, i, &attrCatEntry);
      if (ret != SUCCESS) {
          return ret;
      }

      int type = attrCatEntry.attrType;

      if (type == NUMBER)
      {
          // if the char array record[i] can be converted to a number
          if(isNumber(record[i]))
          {
            /* convert the char array to numeral and store it
                at recordValues[i].nVal using atof() */
            recordValues[i].nVal = atof(record[i]);  
          }
          else
          {
              return E_ATTRTYPEMISMATCH;
          }
      }
      else if (type == STRING)
      {
        // copy record[i] to recordValues[i].sVal
        strcpy(recordValues[i].sVal, record[i]);
      }
  }

  // insert the record by calling BlockAccess::insert() function
  int retVal = BlockAccess::insert(relId, recordValues);

  return retVal;
}

int Algebra::select(char srcRel[ATTR_SIZE], char targetRel[ATTR_SIZE], char attr[ATTR_SIZE], int op, char strVal[ATTR_SIZE]) {
  // Get the srcRel's rel-id using OpenRelTable::getRelId()
  int srcRelId = OpenRelTable::getRelId(srcRel);
  if (srcRelId == E_RELNOTOPEN) {
      return E_RELNOTOPEN;
  }

  // Get the attr-cat entry for attr using AttrCacheTable::getAttrCatEntry()
  AttrCatEntry attrCatEntry;
  int ret = AttrCacheTable::getAttrCatEntry(srcRelId, attr, &attrCatEntry);
  if (ret != SUCCESS) {
      return E_ATTRNOTEXIST;
  }

  /*** Convert strVal to an attribute of data type NUMBER or STRING ***/
  Attribute attrVal;
  int type = attrCatEntry.attrType;

  if (type == NUMBER) {
      if (isNumber(strVal)) {
          attrVal.nVal = atof(strVal);
      } 
      else {
          return E_ATTRTYPEMISMATCH;
      }
  } 
  else if (type == STRING) {
      strcpy(attrVal.sVal, strVal);
  }

  /*** Creating and opening the target relation ***/
  RelCatEntry srcRelCatEntry;
  ret = RelCacheTable::getRelCatEntry(srcRelId, &srcRelCatEntry);
  if (ret != SUCCESS) {
      return ret;
  }

  int src_nAttrs = srcRelCatEntry.numAttrs;
  char attr_names[src_nAttrs][ATTR_SIZE];
  int attr_types[src_nAttrs];

  // Iterate through all attributes to extract their names and data types
  for (int i = 0; i < src_nAttrs; ++i) {
      AttrCatEntry entry;
      AttrCacheTable::getAttrCatEntry(srcRelId, i, &entry);
      strcpy(attr_names[i], entry.attrName);
      attr_types[i] = entry.attrType;
  }

  // Create the target relation schema
  ret = Schema::createRel(targetRel, src_nAttrs, attr_names, attr_types);
  if (ret != SUCCESS) {
      return ret;
  }

  // Open the newly created target relation
  int targetRelId = OpenRelTable::openRel(targetRel);
  if (targetRelId < 0) {
      Schema::deleteRel(targetRel);
      return targetRelId;
  }

  /*** Selecting and inserting records into the target relation ***/
  Attribute record[src_nAttrs];

  // Reset search cursor to ensure full scan from the first record
  RelCacheTable::resetSearchIndex(srcRelId);
  AttrCacheTable::resetSearchIndex(srcRelId, attr);

  BPlusTree::numOfComparisons = 0;
  // Repeatedly search for matching records and insert them into targetRel
  while (BlockAccess::search(srcRelId, record, attr, attrVal, op) == SUCCESS) {
      ret = BlockAccess::insert(targetRelId, record);
      if (ret != SUCCESS) {
          Schema::closeRel(targetRel);
          Schema::deleteRel(targetRel);
          return ret;
      }
  }
  printf("Number of Comparisons: %d\n",BPlusTree::numOfComparisons);

  Schema::closeRel(targetRel);
  return SUCCESS;
}

int Algebra::project(char srcRel[ATTR_SIZE], char targetRel[ATTR_SIZE]) {
  int srcRelId = OpenRelTable::getRelId(srcRel);
  if (srcRelId == E_RELNOTOPEN) {
      return E_RELNOTOPEN;
  }

  // Get RelCatEntry of srcRel using RelCacheTable::getRelCatEntry()
  RelCatEntry srcRelCatEntry;
  int ret = RelCacheTable::getRelCatEntry(srcRelId, &srcRelCatEntry);
  if (ret != SUCCESS) {
      return ret;
  }

  // Get the number of attributes present in the source relation
  int numAttrs = srcRelCatEntry.numAttrs;

  // Declare arrays to store attribute names and types
  char attrNames[numAttrs][ATTR_SIZE];
  int attrTypes[numAttrs];

  // Populate attrNames and attrTypes using AttrCacheTable::getAttrCatEntry()
  for (int i = 0; i < numAttrs; ++i) {
      AttrCatEntry attrCatEntry;
      ret = AttrCacheTable::getAttrCatEntry(srcRelId, i, &attrCatEntry);
      if (ret != SUCCESS) {
          return ret;
      }
      strcpy(attrNames[i], attrCatEntry.attrName);
      attrTypes[i] = attrCatEntry.attrType;
  }

  /*** Creating and opening the target relation ***/

  // Create the target relation by calling Schema::createRel()
  ret = Schema::createRel(targetRel, numAttrs, attrNames, attrTypes);
  if (ret != SUCCESS) {
      return ret;
  }

  // Open the newly created target relation
  int targetRelId = OpenRelTable::openRel(targetRel);
  if (targetRelId < 0) {
      // If opening fails, delete the target relation and return the error code
      Schema::deleteRel(targetRel);
      return targetRelId;
  }

  /*** Inserting projected records into the target relation ***/
  RelCacheTable::resetSearchIndex(srcRelId);
  Attribute record[numAttrs];

  // Read every record sequentially and insert into targetRel
  while (BlockAccess::project(srcRelId, record) == SUCCESS) {
      ret = BlockAccess::insert(targetRelId, record);
      if (ret != SUCCESS) {
          // If insert fails (e.g. disk full), clean up target relation
          Schema::closeRel(targetRel);
          Schema::deleteRel(targetRel);
          return ret;
      }
  }

  Schema::closeRel(targetRel);
  return SUCCESS;
}

int Algebra::project(char srcRel[ATTR_SIZE], char targetRel[ATTR_SIZE], int tar_nAttrs, char tar_Attrs[][ATTR_SIZE]) {
    int srcRelId = OpenRelTable::getRelId(srcRel);
    if (srcRelId == E_RELNOTOPEN) {
        return E_RELNOTOPEN;
    }

    // Get RelCatEntry of srcRel using RelCacheTable::getRelCatEntry()
    RelCatEntry srcRelCatEntry;
    int ret = RelCacheTable::getRelCatEntry(srcRelId, &srcRelCatEntry);
    if (ret != SUCCESS) {
        return ret;
    }

    // Get the number of attributes present in the source relation
    int src_nAttrs = srcRelCatEntry.numAttrs;

    // Declare arrays for target attribute mapping
    int attr_offset[tar_nAttrs];
    int attr_types[tar_nAttrs];

    /*** Checking if attributes of target are present in the source relation
         and storing their offsets and types ***/
    for (int i = 0; i < tar_nAttrs; ++i) {
        AttrCatEntry attrCatEntry;
        ret = AttrCacheTable::getAttrCatEntry(srcRelId, tar_Attrs[i], &attrCatEntry);
        if (ret != SUCCESS) {
            return E_ATTRNOTEXIST;
        }

        attr_offset[i] = attrCatEntry.offset;
        attr_types[i] = attrCatEntry.attrType;
    }

    /*** Creating and opening the target relation ***/

    // Create the target relation schema
    ret = Schema::createRel(targetRel, tar_nAttrs, tar_Attrs, attr_types);
    if (ret != SUCCESS) {
        return ret;
    }

    // Open the newly created target relation
    int targetRelId = OpenRelTable::openRel(targetRel);
    if (targetRelId < 0) {
        Schema::deleteRel(targetRel);
        return targetRelId;
    }

    /*** Inserting projected records into the target relation ***/

    RelCacheTable::resetSearchIndex(srcRelId);
    Attribute record[src_nAttrs];

    // Iterate over all source records and project specified columns
    while (BlockAccess::project(srcRelId, record) == SUCCESS) {
        Attribute proj_record[tar_nAttrs];

        for (int attr_iter = 0; attr_iter < tar_nAttrs; ++attr_iter) {
            proj_record[attr_iter] = record[attr_offset[attr_iter]];
        }

        ret = BlockAccess::insert(targetRelId, proj_record);
        if (ret != SUCCESS) {
            Schema::closeRel(targetRel);
            Schema::deleteRel(targetRel);
            return ret;
        }
    }

    Schema::closeRel(targetRel);
    return SUCCESS;
}

int Algebra::join(char srcRelation1[ATTR_SIZE], char srcRelation2[ATTR_SIZE], 
                  char targetRelation[ATTR_SIZE], 
                  char attribute1[ATTR_SIZE], char attribute2[ATTR_SIZE]) {

    // Get the relation IDs of both source relations
    int srcRelId1 = OpenRelTable::getRelId(srcRelation1);
    int srcRelId2 = OpenRelTable::getRelId(srcRelation2);

    // If either of the two source relations is not open
    if (srcRelId1 == E_RELNOTOPEN || srcRelId2 == E_RELNOTOPEN) {
        return E_RELNOTOPEN;
    }

    // Fetch the attribute catalog entries for the join attributes
    AttrCatEntry attrCatEntry1, attrCatEntry2;
    int ret = AttrCacheTable::getAttrCatEntry(srcRelId1, attribute1, &attrCatEntry1);
    if (ret != SUCCESS) {
        return E_ATTRNOTEXIST;
    }

    ret = AttrCacheTable::getAttrCatEntry(srcRelId2, attribute2, &attrCatEntry2);
    if (ret != SUCCESS) {
        return E_ATTRNOTEXIST;
    }

    // Check if join attributes have matching types
    if (attrCatEntry1.attrType != attrCatEntry2.attrType) {
        return E_ATTRTYPEMISMATCH;
    }

    // Fetch the relation catalog entries to know the number of attributes
    RelCatEntry relCatEntry1, relCatEntry2;
    ret = RelCacheTable::getRelCatEntry(srcRelId1, &relCatEntry1);
    if (ret != SUCCESS) {
        return ret;
    }

    ret = RelCacheTable::getRelCatEntry(srcRelId2, &relCatEntry2);
    if (ret != SUCCESS) {
        return ret;
    }

    int numOfAttributes1 = relCatEntry1.numAttrs;
    int numOfAttributes2 = relCatEntry2.numAttrs;

    // Check for duplicate attribute names between the two relations
    // (excluding the join attributes attribute1 and attribute2)
    for (int i = 0; i < numOfAttributes1; ++i) {
        AttrCatEntry entry1;
        AttrCacheTable::getAttrCatEntry(srcRelId1, i, &entry1);

        // Skip the join attribute of relation 1
        if (strcmp(entry1.attrName, attribute1) == 0) {
            continue;
        }

        for (int j = 0; j < numOfAttributes2; ++j) {
            AttrCatEntry entry2;
            AttrCacheTable::getAttrCatEntry(srcRelId2, j, &entry2);

            // Skip the join attribute of relation 2
            if (strcmp(entry2.attrName, attribute2) == 0) {
                continue;
            }

            // If another attribute has the same name in both relations
            if (strcmp(entry1.attrName, entry2.attrName) == 0) {
                return E_DUPLICATEATTR;
            }
        }
    }

    // Check if an index exists on attribute2 in srcRelation2. If not, create it.
    if (attrCatEntry2.rootBlock == -1) {
        int createIdxRet = BPlusTree::bPlusCreate(srcRelId2, attribute2);
        if (createIdxRet != SUCCESS) {
            return createIdxRet; // E_DISKFULL
        }
    }

    // Prepare the schema for the target relation
    // Number of attributes in target = numOfAttributes1 + numOfAttributes2 - 1
    int numOfAttributesInTarget = numOfAttributes1 + numOfAttributes2 - 1;

    char targetRelAttrNames[numOfAttributesInTarget][ATTR_SIZE];
    int targetRelAttrTypes[numOfAttributesInTarget];

    int targetIdx = 0;

    // First: copy all attributes from srcRelation1
    for (int i = 0; i < numOfAttributes1; ++i) {
        AttrCatEntry entry;
        AttrCacheTable::getAttrCatEntry(srcRelId1, i, &entry);

        strcpy(targetRelAttrNames[targetIdx], entry.attrName);
        targetRelAttrTypes[targetIdx] = entry.attrType;
        targetIdx++;
    }

    // Second: copy all attributes from srcRelation2 except attribute2
    for (int i = 0; i < numOfAttributes2; ++i) {
        AttrCatEntry entry;
        AttrCacheTable::getAttrCatEntry(srcRelId2, i, &entry);

        if (strcmp(entry.attrName, attribute2) == 0) {
            continue; // exclude the second join attribute
        }

        strcpy(targetRelAttrNames[targetIdx], entry.attrName);
        targetRelAttrTypes[targetIdx] = entry.attrType;
        targetIdx++;
    }

    // Create the target relation schema on disk
    ret = Schema::createRel(targetRelation, numOfAttributesInTarget, 
                            targetRelAttrNames, targetRelAttrTypes);
    if (ret != SUCCESS) {
        return ret; // E_RELEXIST, E_DISKFULL
    }

    // Open the target relation
    int targetRelId = OpenRelTable::openRel(targetRelation);
    if (targetRelId < 0) {
        Schema::deleteRel(targetRelation);
        return targetRelId; // e.g. E_CACHEFULL
    }

    // Execute the Equi-Join using an Index Nested Loop Join (INLJ)
    Attribute record1[numOfAttributes1];
    Attribute record2[numOfAttributes2];
    Attribute targetRecord[numOfAttributesInTarget];

    // Reset the outer relation scan cursor
    RelCacheTable::resetSearchIndex(srcRelId1);

    // Outer loop: Iterate over all records in srcRelation1
    while (BlockAccess::project(srcRelId1, record1) == SUCCESS) {

        // Reset search indices for srcRelation2 to restart index search from beginning
        RelCacheTable::resetSearchIndex(srcRelId2);
        AttrCacheTable::resetSearchIndex(srcRelId2, attribute2);

        // Inner loop: Find all records in srcRelation2 where record2.attribute2 == record1.attribute1
        while (BlockAccess::search(srcRelId2, record2, attribute2, 
                                   record1[attrCatEntry1.offset], EQ) == SUCCESS) {

            // Build targetRecord:
            // Copy all attributes from record1
            int tarAttrIdx = 0;
            for (int i = 0; i < numOfAttributes1; ++i) {
                targetRecord[tarAttrIdx++] = record1[i];
            }

            // Copy all attributes from record2 except attribute2
            for (int i = 0; i < numOfAttributes2; ++i) {
                if (i == attrCatEntry2.offset) {
                    continue;
                }
                targetRecord[tarAttrIdx++] = record2[i];
            }

            // Insert into the target relation
            int insertRet = BlockAccess::insert(targetRelId, targetRecord);
            if (insertRet != SUCCESS && insertRet != E_INDEX_BLOCKS_RELEASED) {
                // Disk full during record insertion
                OpenRelTable::closeRel(targetRelId);
                Schema::deleteRel(targetRelation);
                return E_DISKFULL;
            }
        }
    }

    // Close the target relation
    OpenRelTable::closeRel(targetRelId);

    return SUCCESS;
}
