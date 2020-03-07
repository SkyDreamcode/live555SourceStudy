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
// Implementation

#include "BasicHashTable.hh"
#include "strDup.hh"

#if defined(__WIN32__) || defined(_WIN32)
#else
#include <stddef.h>
#endif
#include <string.h>
#include <stdio.h>

// When there are this many entries per bucket, on average, rebuild
// the table to increase the number of buckets
#define REBUILD_MULTIPLIER 3
/*
fStaticBuckets：在 BasicHashTable 中元素比较少时，直接在这个数组中保存键值对，以此来优化当元素比较少的性能，降低内存分配的开销。
fBuckets：指向保存键-值对的 TableEntry 指针数组，在对象创建初期，它指向 fStaticBuckets，而在哈希桶扩容时，\
它指向新分配的 TableEntry 指针数组。对于容器中元素的访问，都通过fBuckets 来完成。
fNumBuckets ：用于保存 TableEntry 指针数组的长度。
fNumEntries ：用于保存容器中键-值对的个数。
fRebuildSize ：为哈希桶扩容的阈值，即当 BasicHashTable 中保存的键值对超过该值时，哈希桶需要扩容。
fDownShift 和 fMask 用于计算哈希值，并把哈希值映射到哈希桶容量范围内。
fKeyType：哈希表的key的类型，有两种：

int const STRING_HASH_KEYS = 0;     //一种是string类型的
int const ONE_WORD_HASH_KEYS = 1;	//一种是word类型的


*/
BasicHashTable::BasicHashTable(int keyType)
  : fBuckets(fStaticBuckets), fNumBuckets(SMALL_HASH_TABLE_SIZE),
    fNumEntries(0), fRebuildSize(SMALL_HASH_TABLE_SIZE*REBUILD_MULTIPLIER),
    fDownShift(28), fMask(0x3), fKeyType(keyType) {
  for (unsigned i = 0; i < SMALL_HASH_TABLE_SIZE; ++i) {
    fStaticBuckets[i] = NULL;
  }
}
//，遍历整个数组，把哈希表的条目全部删除。
BasicHashTable::~BasicHashTable() {
  // Free all the entries in the table:
  for (unsigned i = 0; i < fNumBuckets; ++i) {//循环删除数组中的元素
    TableEntry* entry;
    while ((entry = fBuckets[i]) != NULL) {//循环删除链表中的元素
      deleteEntry(i, entry);
    }
  }

  // Also free the bucket array, if it was dynamically allocated:
  if (fBuckets != fStaticBuckets) delete[] fBuckets;
}

void* BasicHashTable::Add(char const* key, void* value) {
  void* oldValue;
  unsigned index;//通过哈希函数算出的index
   //因为有fmask这个值，计算出来的index都是 & fmask，所以算出的结果是不会超出fmask+1
  TableEntry* entry = lookupKey(key, index);//返回一个空的或者是有值的entry
  if (entry != NULL) { //如果是有值的，就修改
    // There's already an item with this key
    oldValue = entry->value;
  } else {
    // There's no existing entry; create a new one:
    entry = insertNewEntry(index, key);//调整fNext指针，然后再写入新的entry
    oldValue = NULL;
  }
  entry->value = value;

  // If the table has become too large, rebuild it with more buckets:
  if (fNumEntries >= fRebuildSize) rebuild();//当元素超过的时候，扩容

  return oldValue;
}

Boolean BasicHashTable::Remove(char const* key) {
  unsigned index;
  TableEntry* entry = lookupKey(key, index);
  if (entry == NULL) return False; // no such entry

  deleteEntry(index, entry);

  return True;
}

void* BasicHashTable::Lookup(char const* key) const {
  unsigned index;
  TableEntry* entry = lookupKey(key, index);
  if (entry == NULL) return NULL; // no such entry

  return entry->value;
}

unsigned BasicHashTable::numEntries() const {
  return fNumEntries;
}

BasicHashTable::Iterator::Iterator(BasicHashTable const& table)
  : fTable(table), fNextIndex(0), fNextEntry(NULL) {//fTable：这应该就是存入的哈希表
}
//使用迭代器查找下一个，如果是有链表的存在，会查找链表下一个，没有链表的话，就跳转到下一个数组元素。
void* BasicHashTable::Iterator::next(char const*& key) {
  while (fNextEntry == NULL) {
    if (fNextIndex >= fTable.fNumBuckets) return NULL;

    fNextEntry = fTable.fBuckets[fNextIndex++];//找到fBuckets[index]内容
  }

  BasicHashTable::TableEntry* entry = fNextEntry;
  fNextEntry = entry->fNext;

  key = entry->key;
  return entry->value;
}

////////// Implementation of HashTable creation functions //////////
//这两个创建函数都是静态的，通过调用父类的接口，创建子类的对象，多态的用法
HashTable* HashTable::create(int keyType) {
  return new BasicHashTable(keyType);
}

HashTable::Iterator* HashTable::Iterator::create(HashTable const& hashTable) {
  // "hashTable" is assumed to be a BasicHashTable
  return new BasicHashTable::Iterator((BasicHashTable const&)hashTable);
}

////////// Implementation of internal member functions //////////

BasicHashTable::TableEntry* BasicHashTable//用key查找entry，返回一个空的或者是有值的entry。
::lookupKey(char const* key, unsigned& index) const {
  TableEntry* entry;
  index = hashIndexFromKey(key);

  for (entry = fBuckets[index]; entry != NULL; entry = entry->fNext) {
    if (keyMatches(key, entry->key)) break;
  }

  return entry;
}
//key匹配函数，也挺简单的，就是对比key的值是否相等。
Boolean BasicHashTable
::keyMatches(char const* key1, char const* key2) const {
  // The way we check the keys for a match depends upon their type:
  if (fKeyType == STRING_HASH_KEYS) {
    return (strcmp(key1, key2) == 0);
  } else if (fKeyType == ONE_WORD_HASH_KEYS) {
    return (key1 == key2);
  } else {
    unsigned* k1 = (unsigned*)key1;
    unsigned* k2 = (unsigned*)key2;

    for (int i = 0; i < fKeyType; ++i) {
      if (k1[i] != k2[i]) return False; // keys differ
    }
    return True;
  }
}
/*这是插入元素到哈希表中，通过调整fNext的指针指向下一个值，然后把当前的值赋值到fBuckets[index]中。\
记得查找的时候，要找到这个index的fNext的元素。*/
BasicHashTable::TableEntry* BasicHashTable
::insertNewEntry(unsigned index, char const* key) {
  TableEntry* entry = new TableEntry();
  entry->fNext = fBuckets[index];
  fBuckets[index] = entry;

  ++fNumEntries;
  assignKey(entry, key);

  return entry;
}
/*保存key的值，保存的时候，也会判断key的类型，然后再分别保存。如果是自定义的话，\
会根据fKeyType的长度，申请内存，然后再拷贝到新内存中。*/
void BasicHashTable::assignKey(TableEntry* entry, char const* key) {
  // The way we assign the key depends upon its type:
  if (fKeyType == STRING_HASH_KEYS) {
    entry->key = strDup(key);
  } else if (fKeyType == ONE_WORD_HASH_KEYS) {
    entry->key = key;
  } else if (fKeyType > 0) {
    unsigned* keyFrom = (unsigned*)key;
    unsigned* keyTo = new unsigned[fKeyType];
    for (int i = 0; i < fKeyType; ++i) keyTo[i] = keyFrom[i];

    entry->key = (char const*)keyTo;
  }
}
//通过二级指针，遍历链表一趟，就将元素移除出去了。这一句就是移除出去的*ep = entry->fNext;
void BasicHashTable::deleteEntry(unsigned index, TableEntry* entry) {
  TableEntry** ep = &fBuckets[index];

  Boolean foundIt = False;
  while (*ep != NULL) {
    if (*ep == entry) {//如果找到了
      foundIt = True;
      *ep = entry->fNext;//*ep的指针指向下一个，这样就把entry这个元素删除了，修改了指针。
      break;
    }
    ep = &((*ep)->fNext);//没有找到，找下一个
  }

  if (!foundIt) { // shouldn't happen
#ifdef DEBUG
    fprintf(stderr, "BasicHashTable[%p]::deleteEntry(%d,%p): internal error - not found (first entry %p", this, index, entry, fBuckets[index]);
    if (fBuckets[index] != NULL) fprintf(stderr, ", next entry %p", fBuckets[index]->fNext);
    fprintf(stderr, ")\n");
#endif
  }

  --fNumEntries;
  deleteKey(entry);
  delete entry;
}

void BasicHashTable::deleteKey(TableEntry* entry) {
  // The way we delete the key depends upon its type:
  if (fKeyType == ONE_WORD_HASH_KEYS) {
    entry->key = NULL;
  } else {
    delete[] (char*)entry->key;
    entry->key = NULL;
  }
}
/*在添加的时候，我们也看到了一个判断条件，如果元素个数大于一个数的时候，就进行扩容，扩容还是有好处的，\
如果不能确定一个哈希表多大的时候，还是需要支持扩容。扩容之前要说一下这个哈希表的存储结构，\
应该看了插入部分的都知道了吧，这个哈希表的结构就是一个TableEntry*[fNumBuckets] 的数组，\
由哈希函数算出要插入的数组下标，然后插入，当哈希冲突的时候，每一个数组的元素都有一个指针，\
指向下一个的指针，哈希冲突的时候会存到链表中。*/

void BasicHashTable::rebuild() {
  // Remember the existing table size:
  unsigned oldSize = fNumBuckets;
  TableEntry** oldBuckets = fBuckets;

  // Create the new sized table:
  fNumBuckets *= 4;
  fBuckets = new TableEntry*[fNumBuckets];//扩容后，指向一个新的内存
  for (unsigned i = 0; i < fNumBuckets; ++i) {//初始化
    fBuckets[i] = NULL;
  }
  fRebuildSize *= 4;//扩容的大小也乘以4
  fDownShift -= 2;//28-2 = 26
  fMask = (fMask<<2)|0x3; // 0x03 << 2 | 0x03 = 0x0f  = 16

  // Rehash the existing entries into the new table:
  for (TableEntry** oldChainPtr = oldBuckets; oldSize > 0;//查找index重复的值，哈希冲突
       --oldSize, ++oldChainPtr) {
    for (TableEntry* hPtr = *oldChainPtr; hPtr != NULL;
	 hPtr = *oldChainPtr) {
      *oldChainPtr = hPtr->fNext;//冲突是用链表的方式的，一直往下找

      unsigned index = hashIndexFromKey(hPtr->key);//获取新的哈希函数的index

      hPtr->fNext = fBuckets[index];//填写新的哈希表中
      fBuckets[index] = hPtr;
    }
  }

  // Free the old bucket array, if it was dynamically allocated:
  if (oldBuckets != fStaticBuckets) delete[] oldBuckets;
}
//这个函数是通过key值来生成哈希Index，通过看这个函数也看的出来key的值有两种类型，分别处理的方式不一样
unsigned BasicHashTable::hashIndexFromKey(char const* key) const {
  unsigned result = 0;

  if (fKeyType == STRING_HASH_KEYS) {
    while (1) {
      char c = *key++;
      if (c == 0) break;
      result += (result<<3) + (unsigned)c;
    }
    result &= fMask;
  } else if (fKeyType == ONE_WORD_HASH_KEYS) {
    result = randomIndex((uintptr_t)key);
  } else {
    unsigned* k = (unsigned*)key;
    uintptr_t sum = 0;
    for (int i = 0; i < fKeyType; ++i) {
      sum += k[i];
    }
    result = randomIndex(sum);
  }

  return result;
}
