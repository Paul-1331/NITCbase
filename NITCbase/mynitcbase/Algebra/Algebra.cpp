#include "Algebra.h"

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


/* used to select all the records that satisfy a condition.
the arguments of the function are
- srcRel - the source relation we want to select from
- targetRel - the relation we want to select into. (ignore for now)
- attr - the attribute that the condition is checking
- op - the operator of the condition
- strVal - the value that we want to compare against (represented as a string)
*/
int Algebra::select(char srcRel[ATTR_SIZE], char targetRel[ATTR_SIZE], char attr[ATTR_SIZE], int op, char strVal[ATTR_SIZE]) {
  
  int srcRelId = OpenRelTable::getRelId(srcRel);      // we'll implement this later
  if (srcRelId == E_RELNOTOPEN) {
    return E_RELNOTOPEN;
  }

  AttrCatEntry attrCatEntry;
  // get the attribute catalog entry for attr, using AttrCacheTable::getAttrcatEntry()
  // return E_ATTRNOTEXIST if it returns the error
  int ret = AttrCacheTable::getAttrCatEntry(srcRelId, attr, &attrCatEntry);
  if (ret != SUCCESS) {
    return E_ATTRNOTEXIST;
  }


  /*** Convert strVal (string) to an attribute of data type NUMBER or STRING ***/
  int type = attrCatEntry.attrType;
  Attribute attrVal;
  if (type == NUMBER) {
    if (isNumber(strVal)) {       // the isNumber() function is implemented above
      attrVal.nVal = atof(strVal);
    } else {
      return E_ATTRTYPEMISMATCH;
    }
  } else if (type == STRING) {
    strcpy(attrVal.sVal, strVal);
  }

  /*** Selecting records from the source relation ***/

  // Before calling the search function, reset the search to start from the first hit
  // using RelCacheTable::resetSearchIndex()
  RelCacheTable::resetSearchIndex(srcRelId);

  RelCatEntry relCatEntry;
  // get relCatEntry using RelCacheTable::getRelCatEntry()
  ret = RelCacheTable::getRelCatEntry(srcRelId, &relCatEntry);
  if (ret != SUCCESS) {
    return ret;
  }

  /************************
  The following code prints the contents of a relation directly to the output
  console. Direct console output is not permitted by the actual the NITCbase
  specification and the output can only be inserted into a new relation. We will
  be modifying it in the later stages to match the specification.
  ************************/

  printf("|");
  for (int i = 0; i < relCatEntry.numAttrs; ++i) {
    AttrCatEntry attrCatEntry;
    // get attrCatEntry at offset i using AttrCacheTable::getAttrCatEntry()
    AttrCacheTable::getAttrCatEntry(srcRelId, i, &attrCatEntry);

    printf(" %s |", attrCatEntry.attrName);
  }
  printf("\n");

  while (true) {
    RecId searchRes = BlockAccess::linearSearch(srcRelId, attr, attrVal, op);

    if (searchRes.block != -1 && searchRes.slot != -1) {

      // get the record at searchRes using BlockBuffer.getRecord
      RecBuffer blockBuffer(searchRes.block);
      Attribute record[relCatEntry.numAttrs];
      blockBuffer.getRecord(record, searchRes.slot);

      // print the attribute values in the same format as above
      printf("|");
      for (int i = 0; i < relCatEntry.numAttrs; ++i) {
        AttrCatEntry attrCatEntry;
        AttrCacheTable::getAttrCatEntry(srcRelId, i, &attrCatEntry);

        if (attrCatEntry.attrType == NUMBER) {
          printf(" %d |", (int)record[i].nVal);
        } else {
          printf(" %s |", record[i].sVal);
        }
      }
      printf("\n");

    } 
    else {
      // (all records over)
      break;
    }
  }

  return SUCCESS;
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

  // Repeatedly search for matching records and insert them into targetRel
  while (BlockAccess::search(srcRelId, record, attr, attrVal, op) == SUCCESS) {
      ret = BlockAccess::insert(targetRelId, record);
      if (ret != SUCCESS) {
          Schema::closeRel(targetRel);
          Schema::deleteRel(targetRel);
          return ret;
      }
  }

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
