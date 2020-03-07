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
// Generic Hash Table
// Implementation

#include "HashTable.hh"

HashTable::HashTable() {
}

HashTable::~HashTable() {
}

HashTable::Iterator::Iterator() {
}

HashTable::Iterator::~Iterator() {}
//这个是hashTable的函数，不过使用了迭代器next方法，原来就是找到下一个元素，然后删除，所以这个函数叫RemoveNext。
void* HashTable::RemoveNext() {
  Iterator* iter = Iterator::create(*this);
  char const* key;
  void* removedValue = iter->next(key);//Iterator 类的 next(char const*& key) 接收一个传出参数，用于将键返回给调用者。
  if (removedValue != 0) Remove(key);

  delete iter;
  return removedValue;
}

void* HashTable::getFirst() {
  Iterator* iter = Iterator::create(*this);
  char const* key;
  void* firstValue = iter->next(key);

  delete iter;
  return firstValue;
}
