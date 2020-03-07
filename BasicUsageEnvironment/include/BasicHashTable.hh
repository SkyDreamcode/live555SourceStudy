/**********
This library is free software; you can redistribute it and/or modify it under
the terms of the GNU Lesser General Public License as published by the
Free Software Foundation; either version 3 of the License, or (at your
option) any later version. (See <http://www.gnu.org/copyleft/lesser.html>.)

This library is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
more details.

You should have received a copy of the GNU Lesser General Public License
along with this library; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
**********/
// Copyright (c) 1996-2019 Live Networks, Inc.  All rights reserved.
// Basic Hash Table implementation
// C++ header

#ifndef _BASIC_HASH_TABLE_HH
#define _BASIC_HASH_TABLE_HH

#ifndef _HASH_TABLE_HH
#include "HashTable.hh"
#endif
#ifndef _NET_COMMON_H
#include <NetCommon.h> // to ensure that "uintptr_t" is defined
#endif

// A simple hash table implementation, inspired by the hash table
// implementation used in Tcl 7.6: <http://www.tcl.tk/>

#define SMALL_HASH_TABLE_SIZE 4

class BasicHashTable: public HashTable {
private:
	class TableEntry; // forward

public:
  BasicHashTable(int keyType);
  virtual ~BasicHashTable();

  // Used to iterate through the members of the table:
  class Iterator; friend class Iterator; // to make Sun's C++ compiler happy
  class Iterator: public HashTable::Iterator {
  public:
    Iterator(BasicHashTable const& table);

  private: // implementation of inherited pure virtual functions
    void* next(char const*& key); // returns 0 if none

  private:
    BasicHashTable const& fTable;
    unsigned fNextIndex; // index of next bucket to be enumerated after this
    TableEntry* fNextEntry; // next entry in the current bucket
  };

private: // implementation of inherited pure virtual functions
  virtual void* Add(char const* key, void* value);
  // Returns the old value if different, otherwise 0
  virtual Boolean Remove(char const* key);
  virtual void* Lookup(char const* key) const;
  // Returns 0 if not found
  virtual unsigned numEntries() const;

private:
  class TableEntry {//这应该是哈希表的一个条目
  public:
    TableEntry* fNext; //用于指向计算的键的哈希值冲突的不同的键-值对中的下一个
    char const* key;//这个就是key
    void* value;//这个是值
  };

  TableEntry* lookupKey(char const* key, unsigned& index) const;
    // returns entry matching "key", or NULL if none
  Boolean keyMatches(char const* key1, char const* key2) const;
    // used to implement "lookupKey()"

  TableEntry* insertNewEntry(unsigned index, char const* key);
    // creates a new entry, and inserts it in the table
  void assignKey(TableEntry* entry, char const* key);
    // used to implement "insertNewEntry()"

  void deleteEntry(unsigned index, TableEntry* entry);
  void deleteKey(TableEntry* entry);
    // used to implement "deleteEntry()"

  void rebuild(); // rebuilds the table as its size increases

  unsigned hashIndexFromKey(char const* key) const;
    // used to implement many of the routines above
//随机返回哈希表的index，我们在初始化的时候 fDownShift= 0x28 fMask = 0x03,
//就是根据这个公式算出随机值
  unsigned randomIndex(uintptr_t i) const {
    return (unsigned)(((i*1103515245) >> fDownShift) & fMask);
  }

private:
  TableEntry** fBuckets; // pointer to bucket array
  TableEntry* fStaticBuckets[SMALL_HASH_TABLE_SIZE];// used for small tables
  unsigned fNumBuckets, fNumEntries, fRebuildSize, fDownShift, fMask;
  int fKeyType;
};

#endif
