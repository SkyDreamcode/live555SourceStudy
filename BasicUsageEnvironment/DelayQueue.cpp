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
// Copyright (c) 1996-2019, Live Networks, Inc.  All rights reserved
//	Help by Carlo Bonamico to get working for Windows
// Delay queue
// Implementation

#include "DelayQueue.hh"
#include "GroupsockHelper.hh"

static const int MILLION = 1000000;

///// Timeval /////

int Timeval::operator>=(const Timeval& arg2) const {
  return seconds() > arg2.seconds()
    || (seconds() == arg2.seconds()
	&& useconds() >= arg2.useconds());
}

void Timeval::operator+=(const DelayInterval& arg2) {
  secs() += arg2.seconds(); usecs() += arg2.useconds();
  if (useconds() >= MILLION) {
    usecs() -= MILLION;
    ++secs();
  }
}

void Timeval::operator-=(const DelayInterval& arg2) {
  secs() -= arg2.seconds(); usecs() -= arg2.useconds();
  if ((int)useconds() < 0) {
    usecs() += MILLION;
    --secs();
  }
  if ((int)seconds() < 0)
    secs() = usecs() = 0;

}

DelayInterval operator-(const Timeval& arg1, const Timeval& arg2) {
  time_base_seconds secs = arg1.seconds() - arg2.seconds();
  time_base_seconds usecs = arg1.useconds() - arg2.useconds();

  if ((int)usecs < 0) {
    usecs += MILLION;
    --secs;
  }
  if ((int)secs < 0)
    return DELAY_ZERO;
  else
    return DelayInterval(secs, usecs);
}


///// DelayInterval /////

DelayInterval operator*(short arg1, const DelayInterval& arg2) {
  time_base_seconds result_seconds = arg1*arg2.seconds();
  time_base_seconds result_useconds = arg1*arg2.useconds();

  time_base_seconds carry = result_useconds/MILLION;
  result_useconds -= carry*MILLION;
  result_seconds += carry;

  return DelayInterval(result_seconds, result_useconds);
}

#ifndef INT_MAX
#define INT_MAX	0x7FFFFFFF
#endif
const DelayInterval DELAY_ZERO(0, 0);
const DelayInterval DELAY_SECOND(1, 0);
const DelayInterval DELAY_MINUTE = 60*DELAY_SECOND;
const DelayInterval DELAY_HOUR = 60*DELAY_MINUTE;
const DelayInterval DELAY_DAY = 24*DELAY_HOUR;
const DelayInterval ETERNITY(INT_MAX, MILLION-1);
// used internally to make the implementation work


///// DelayQueueEntry /////

intptr_t DelayQueueEntry::tokenCounter = 0;

DelayQueueEntry::DelayQueueEntry(DelayInterval delay)
  : fDeltaTimeRemaining(delay) {
  fNext = fPrev = this;//这个是初始化指针
  fToken = ++tokenCounter;
}

DelayQueueEntry::~DelayQueueEntry() {
}

void DelayQueueEntry::handleTimeout() {//原来是在这里删除的
  delete this;
}


///// DelayQueue /////
/*调用了父类的构造函数，ETERNITY是一个很大的值
父类的构造函数，填充了两个参数
fDeltaTimeRemaining：ETERNITY 一个很大的值 （时间差）
fNext = fPrev = this ：类中的Next和Prev指针
子类构造函数设的值：
fLastSyncTime：最后一次同步的时间。


*/
DelayQueue::DelayQueue()
  : DelayQueueEntry(ETERNITY) {
  fLastSyncTime = TimeNow();
}
//析构函数中有释放内存，但是还是不知道在哪里申请内存
DelayQueue::~DelayQueue() {
  while (fNext != this) {
    DelayQueueEntry* entryToRemove = fNext;
    removeEntry(entryToRemove);
    delete entryToRemove;
  }
}

void DelayQueue::addEntry(DelayQueueEntry* newEntry) {
  synchronize();

  DelayQueueEntry* cur = head();
  while (newEntry->fDeltaTimeRemaining >= cur->fDeltaTimeRemaining) {// 以这个时间差排序
    newEntry->fDeltaTimeRemaining -= cur->fDeltaTimeRemaining;// 每过一个条目，这个时间差就是和上一个条目的时间差，所以需要相减
    cur = cur->fNext;// 查找下一个
  }
  // 找到符合当前的位置了，插入到cur的前面，所以cur要往后退，所以cur的时间差要改

  cur->fDeltaTimeRemaining -= newEntry->fDeltaTimeRemaining;

  // Add "newEntry" to the queue, just before "cur":
  newEntry->fNext = cur;
  newEntry->fPrev = cur->fPrev;
  cur->fPrev = newEntry->fPrev->fNext = newEntry;
}

//entry：要更新的结点的指针
//newDelay：新的延时时间
void DelayQueue::updateEntry(DelayQueueEntry* entry, DelayInterval newDelay) {
  if (entry == NULL) return;

  removeEntry(entry); //删除结点，其实删除内部只是修改了值，并没有释放内存，这点要注意
  entry->fDeltaTimeRemaining = newDelay;
  addEntry(entry);//还是原来的指针，修改了时间差，就添加进入（这个是排序的队列，所以不能直接修改时间差）
}

void DelayQueue::updateEntry(intptr_t tokenToFind, DelayInterval newDelay) {
  DelayQueueEntry* entry = findEntryByToken(tokenToFind);
  updateEntry(entry, newDelay);
}


/*删除结点的时候需要注意：就是函数中没有提供释放结点的操作，只是返回了该结点的指针，应该是需要我们自己释放结点的内存。
相应的在添加的时候，也并没有申请内存，只是把结点的指针挂载在链表上。*/
void DelayQueue::removeEntry(DelayQueueEntry* entry) {
  if (entry == NULL || entry->fNext == NULL) return;//这个是因为是双向链表。所以需要这样判断

  entry->fNext->fDeltaTimeRemaining += entry->fDeltaTimeRemaining;//要删除一个结点了，需要把后一个结点的时间差更新一下
   //后面是调整链表的指针
  entry->fPrev->fNext = entry->fNext;
  entry->fNext->fPrev = entry->fPrev;
  entry->fNext = entry->fPrev = NULL;
  // in case we should try to remove it again
}

//这个才是真正的删除，通过token删除结点
DelayQueueEntry* DelayQueue::removeEntry(intptr_t tokenToFind) {
  DelayQueueEntry* entry = findEntryByToken(tokenToFind);//通过token找到对应的结点的指针
  removeEntry(entry);//删除结点
  return entry;
}
//属于查询的函数，查找下一个要执行的任务的时间差还有多少。
DelayInterval const& DelayQueue::timeToNextAlarm() {
  if (head()->fDeltaTimeRemaining == DELAY_ZERO) return DELAY_ZERO; // a common case

  synchronize();
  return head()->fDeltaTimeRemaining;
}
//处理时间到的事件，也是使用回调的方式处理的。
void DelayQueue::handleAlarm() {
  if (head()->fDeltaTimeRemaining != DELAY_ZERO) synchronize();

  if (head()->fDeltaTimeRemaining == DELAY_ZERO) {
    // This event is due to be handled:
    DelayQueueEntry* toRemove = head();
    removeEntry(toRemove); // do this first, in case handler accesses queue

    toRemove->handleTimeout();
  }
}
//到遍历的方式，然后去匹配，链表只能是从头到尾遍历
DelayQueueEntry* DelayQueue::findEntryByToken(intptr_t tokenToFind) {
  DelayQueueEntry* cur = head();
  while (cur != this) {
    if (cur->token() == tokenToFind) return cur;
    cur = cur->fNext;
  }

  return NULL;
}

void DelayQueue::synchronize() {
  // First, figure out how much time has elapsed since the last sync:
  _EventTime timeNow = TimeNow();
  if (timeNow < fLastSyncTime) {
    // The system clock has apparently gone back in time; reset our sync time and return:
    fLastSyncTime  = timeNow;
    return;
  }
  DelayInterval timeSinceLastSync = timeNow - fLastSyncTime;//算出两次同步的间隔时间
  fLastSyncTime = timeNow;//保存现在的时间为最后同步时间

  // Then, adjust the delay queue for any entries whose time is up:
  DelayQueueEntry* curEntry = head();
  while (timeSinceLastSync >= curEntry->fDeltaTimeRemaining) { //判断一下间隔任务中都比这个时间间隔低的结点
    timeSinceLastSync -= curEntry->fDeltaTimeRemaining; // 因为这是存储和上一个结点的时间差，所以需要减
    curEntry->fDeltaTimeRemaining = DELAY_ZERO;// 时间差已经过了的，把时间间隔请0
    curEntry = curEntry->fNext;// 找下一个
  }
  curEntry->fDeltaTimeRemaining -= timeSinceLastSync;//  当找到一个时间个比过去的时间间隔都要长的时候，更新时间间隔
  //为什么后面的时间间隔不需要改变，是因为我们当初保存的是和上一个结点的时间差，时间差是不会改变的，所以只要调整一个即可，这里比较巧妙。
}


///// _EventTime /////
//获取当前时间的封装
_EventTime TimeNow() {
  struct timeval tvNow;

  gettimeofday(&tvNow, NULL);

  return _EventTime(tvNow.tv_sec, tvNow.tv_usec);
}
//结束时间
const _EventTime THE_END_OF_TIME(INT_MAX);
