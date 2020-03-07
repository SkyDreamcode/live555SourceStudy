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
// Delay queue
// C++ header

#ifndef _DELAY_QUEUE_HH
#define _DELAY_QUEUE_HH

#ifndef _NET_COMMON_H
#include "NetCommon.h"
#endif

#ifdef TIME_BASE
typedef TIME_BASE time_base_seconds;
#else
typedef long time_base_seconds;
#endif

///// A "Timeval" can be either an absolute time, or a time interval /////

class Timeval {
public:
  time_base_seconds seconds() const {
    return fTv.tv_sec;
  }
  time_base_seconds seconds() {
    return fTv.tv_sec;
  }
  time_base_seconds useconds() const {
    return fTv.tv_usec;
  }
  time_base_seconds useconds() {
    return fTv.tv_usec;
  }

  int operator>=(Timeval const& arg2) const;
  int operator<=(Timeval const& arg2) const {
    return arg2 >= *this;
  }
  int operator<(Timeval const& arg2) const {
    return !(*this >= arg2);
  }
  int operator>(Timeval const& arg2) const {
    return arg2 < *this;
  }
  int operator==(Timeval const& arg2) const {
    return *this >= arg2 && arg2 >= *this;
  }
  int operator!=(Timeval const& arg2) const {
    return !(*this == arg2);
  }

  void operator+=(class DelayInterval const& arg2);
  void operator-=(class DelayInterval const& arg2);
  // returns ZERO iff arg2 >= arg1

protected:
  Timeval(time_base_seconds seconds, time_base_seconds useconds) {
    fTv.tv_sec = seconds; fTv.tv_usec = useconds;
  }

private:
  time_base_seconds& secs() {
    return (time_base_seconds&)fTv.tv_sec;
  }
  time_base_seconds& usecs() {
    return (time_base_seconds&)fTv.tv_usec;
  }

  struct timeval fTv;
};

#ifndef max
inline Timeval max(Timeval const& arg1, Timeval const& arg2) {
  return arg1 >= arg2 ? arg1 : arg2;
}
#endif
#ifndef min
inline Timeval min(Timeval const& arg1, Timeval const& arg2) {
  return arg1 <= arg2 ? arg1 : arg2;
}
#endif

class DelayInterval operator-(Timeval const& arg1, Timeval const& arg2);
// returns ZERO iff arg2 >= arg1


///// DelayInterval /////
/*
延时间隔类，这个类的作用定义延时间隔的，因为Timeval的构造方法保护化，\
外部不能访问，只有子类可以访问，这时候，子类就可以调用父类的初始化函数，进行初始化时间间隔。

*/
class DelayInterval: public Timeval {
public:
  DelayInterval(time_base_seconds seconds, time_base_seconds useconds)
    : Timeval(seconds, useconds) {}
};

DelayInterval operator*(short arg1, DelayInterval const& arg2);

extern DelayInterval const DELAY_ZERO;
extern DelayInterval const DELAY_SECOND;
extern DelayInterval const DELAY_MINUTE;
extern DelayInterval const DELAY_HOUR;
extern DelayInterval const DELAY_DAY;

///// _EventTime /////
//跟延时时间用的方式一样，不过这个参数是有默认值的
class _EventTime: public Timeval {
public:
  _EventTime(unsigned secondsSinceEpoch = 0,
	    unsigned usecondsSinceEpoch = 0)
    // We use the Unix standard epoch: January 1, 1970
    : Timeval(secondsSinceEpoch, usecondsSinceEpoch) {}
};

_EventTime TimeNow();

extern _EventTime const THE_END_OF_TIME;


///// DelayQueueEntry /////
//延时队列中存储了一个条目的信息，延时队列就是需要一个一个这样结点组成。
class DelayQueueEntry {
public:
  virtual ~DelayQueueEntry();

  intptr_t token() {
    return fToken;
  }

protected: // abstract base class
  DelayQueueEntry(DelayInterval delay);

  virtual void handleTimeout();//子类需要实现这个函数，这个函数就是时间到的时候，需要执行的任务。

private:
  friend class DelayQueue;
  DelayQueueEntry* fNext;
  DelayQueueEntry* fPrev;
  /*
  1，延时队列是按照fDeltaTimeRemaining这个时间间隔的值排序的。
  2，fDeltaTimeRemaining这个时间间隔是和上一个结点的fDeltaTimeRemaining这个值算出来的时间间隔。

  */
  DelayInterval fDeltaTimeRemaining;//// 用于表示延迟任务需要被执行的时间距当前时间的间隔,还需要多久能执行这个任务

  intptr_t fToken;//这个是标识哪一个结点的id，开始时由全局变量tokenCounter维护。
  static intptr_t tokenCounter;//这是一个静态变量，这个变量表示多少个条目
};

///// DelayQueue /////
//队列定义继承了上面的队列条目。
class DelayQueue: public DelayQueueEntry {
public:
  DelayQueue();
  virtual ~DelayQueue();

  void addEntry(DelayQueueEntry* newEntry); // returns a token for the entry
  void updateEntry(DelayQueueEntry* entry, DelayInterval newDelay);
  void updateEntry(intptr_t tokenToFind, DelayInterval newDelay);
  void removeEntry(DelayQueueEntry* entry); // but doesn't delete it
  DelayQueueEntry* removeEntry(intptr_t tokenToFind); // but doesn't delete it

  DelayInterval const& timeToNextAlarm();
  void handleAlarm();

private:
/*
直接返回Next指针，这个next指针现在是this，也就是新建的条目的地址。
新添加的条目的时间差，肯定比第一个条目小，所以就直接插入，插入到第一条的前面，变成新的第一条。
如果是大于当前条目，就需要循环遍历，这个队列是按照时间差排序的，找到适合的时间差，然后插入。

*/
  DelayQueueEntry* head() { return fNext; }
  DelayQueueEntry* findEntryByToken(intptr_t token);
  void synchronize(); // bring the 'time remaining' fields up-to-date

  _EventTime fLastSyncTime;//最后一次同步的时间
};

#endif
