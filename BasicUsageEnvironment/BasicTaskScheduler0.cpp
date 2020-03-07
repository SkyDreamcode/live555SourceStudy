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
// Basic Usage Environment: for a simple, non-scripted, console application
// Implementation

#include "BasicUsageEnvironment0.hh"
#include "HandlerSet.hh"

////////// A subclass of DelayQueueEntry,
//////////     used to implement BasicTaskScheduler0::scheduleDelayedTask()
//定义了一个DelayQueueEntry的子类，保存到延时队列中的一个结点元素，以后只要申请这个类的对象，插入到队列中即可。
/*
构造函数，申请了一个结点的内存，是在这里申请的。还是把改填充的参数也都填充了。
fProc：回调函数，还是用了函数指针的方式
fClientData：客户端的数据
virtual void handleTimeout()：执行回调的函数，首先执行的是我们绑定进去的函数指针，其次就是执行父类的回调函数。\
刚刚看了，父类的对调函数就是释放掉这个结点的内存，是需要执行的。


*/
class AlarmHandler: public DelayQueueEntry {
public:
  AlarmHandler(TaskFunc* proc, void* clientData, DelayInterval timeToDelay)
    : DelayQueueEntry(timeToDelay), fProc(proc), fClientData(clientData) {
  }

private: // redefined virtual functions
  virtual void handleTimeout() {
    (*fProc)(fClientData);
    DelayQueueEntry::handleTimeout();
  }

private:
  TaskFunc* fProc;
  void* fClientData;
};


////////// BasicTaskScheduler0 //////////

BasicTaskScheduler0::BasicTaskScheduler0()
  : fLastHandledSocketNum(-1), fTriggersAwaitingHandling(0), fLastUsedTriggerMask(1), fLastUsedTriggerNum(MAX_NUM_EVENT_TRIGGERS-1) {
  fHandlers = new HandlerSet;
  for (unsigned i = 0; i < MAX_NUM_EVENT_TRIGGERS; ++i) {
    fTriggeredEventHandlers[i] = NULL;
    fTriggeredEventClientDatas[i] = NULL;
  }
}

BasicTaskScheduler0::~BasicTaskScheduler0() {
  delete fHandlers;
}

TaskToken BasicTaskScheduler0::scheduleDelayedTask(int64_t microseconds,
						 TaskFunc* proc,
						 void* clientData) {
  if (microseconds < 0) microseconds = 0;
  DelayInterval timeToDelay((long)(microseconds/1000000), (long)(microseconds%1000000));// 把ms单位转换成DelayInterval 类
  AlarmHandler* alarmHandler = new AlarmHandler(proc, clientData, timeToDelay);//// 创建一个任务
  fDelayQueue.addEntry(alarmHandler); // 添加到等待队列中

  return (void*)(alarmHandler->token());// 返回这个任务的token，这个就是这个任务的标记
}


//这是删除一个任务，这次看到了删除条目怎么用了，也是使用了token，利用这个token去删除对应的条目，\
//然后返回这个条目的指针，删除的内部是没有做内存释放，所以需要外部手动释放。
void BasicTaskScheduler0::unscheduleDelayedTask(TaskToken& prevTask) {
  DelayQueueEntry* alarmHandler = fDelayQueue.removeEntry((intptr_t)prevTask);
  prevTask = NULL;
  delete alarmHandler;
}

void BasicTaskScheduler0::doEventLoop(char volatile* watchVariable) {
  // Repeatedly loop, handling readble sockets and timed events:
  while (1) {
    if (watchVariable != NULL && *watchVariable != 0) break;
    SingleStep();//这个SingleStep()是虚函数，要让子类实现的，所以这个函数等下看了子类再分析。
  }
}


//首先看到的就是一个循环判断，因为我们是用位来表示的，所以需要循环判断哪一个位是空闲的，然后进行插入。
EventTriggerId BasicTaskScheduler0::createEventTrigger(TaskFunc* eventHandlerProc) {
  unsigned i = fLastUsedTriggerNum;// 刚开始时31
  EventTriggerId mask = fLastUsedTriggerMask;// 刚开始是1

  do {
    i = (i+1)%MAX_NUM_EVENT_TRIGGERS;// i = (31+1) % 32
    mask >>= 1;// 右移1    mask =0
    if (mask == 0) mask = 0x80000000;
	// mask = 0x80000000

    if (fTriggeredEventHandlers[i] == NULL) {
      // This trigger number is free; use it:
      fTriggeredEventHandlers[i] = eventHandlerProc;
      fTriggeredEventClientDatas[i] = NULL; // sanity

      fLastUsedTriggerMask = mask;// 最后一个mask状态
      fLastUsedTriggerNum = i;// 最后一个num，为了优化做的吧

      return mask;//插入成功返回掩码
    }
  } while (i != fLastUsedTriggerNum);
	// 通过插入函数可以看出，mask=0x80000000 的时候  ，数据填充到fTriggeredEventHandlers[0]，
	//这是一个反向对应过程mask最高为对应数组最低位

  // All available event triggers are allocated; return 0 instead:
  return 0;
}


/*删除用户事件，首先把就绪状态的标记清0，也就是没有就绪了。
其次就是删除的时候做了一个判断，这个判断就是如果只有一个事件的时候，
就不需要循环，直接删除，这个是做了优化了，如果有多个事件，没方法了，只能遍历删除了。*/
void BasicTaskScheduler0::deleteEventTrigger(EventTriggerId eventTriggerId) {
	//把对应的位请0，fTriggersAwaitingHandling的位为1的时候就表示已经事件已就绪

  fTriggersAwaitingHandling &=~ eventTriggerId;

  if (eventTriggerId == fLastUsedTriggerMask) { // common-case optimization:\
  											    // 常见情况优化：这如果只有一个用户事件的话，就可以直接操作
    fTriggeredEventHandlers[fLastUsedTriggerNum] = NULL;// fLastUsedTriggerNum：表示最后一个num，\
    													//如果上面判断条件成立，是可以这样直接删除
    fTriggeredEventClientDatas[fLastUsedTriggerNum] = NULL;
  } else {
    // "eventTriggerId" should have just one bit set.
    // However, we do the reasonable thing if the user happened to 'or' together two or more "EventTriggerId"s:
    EventTriggerId mask = 0x80000000;
    for (unsigned i = 0; i < MAX_NUM_EVENT_TRIGGERS; ++i) {
      if ((eventTriggerId&mask) != 0) {
	fTriggeredEventHandlers[i] = NULL;
	fTriggeredEventClientDatas[i] = NULL;
      }
      mask >>= 1;
    }
  }
}

/*这个是触发用户事件；
第一个先遍历，找到合适的位置把用户数据插入；
第二把准备就绪中的对应的位置1。
逻辑很简单，但是我有一点不明白，是不是应该有一个函数来查询这个fTriggersAwaitingHandling变量，然后遍历，
看看哪一个为置1，然后拿出对应的函数和数据执行，这个要看到后面才知道。
————————————————
版权声明：本文为CSDN博主「酱油师兄」的原创文章，遵循 CC 4.0 BY-SA 版权协议，转载请附上原文出处链接及本声明。
原文链接：https://blog.csdn.net/C1033177205/article/details/104442276*/

void BasicTaskScheduler0::triggerEvent(EventTriggerId eventTriggerId, void* clientData) {
  // First, record the "clientData".  (Note that we allow "eventTriggerId" to be a combination of bits for multiple events.)
  EventTriggerId mask = 0x80000000;
  for (unsigned i = 0; i < MAX_NUM_EVENT_TRIGGERS; ++i) {
    if ((eventTriggerId&mask) != 0) {
      fTriggeredEventClientDatas[i] = clientData;
    }
    mask >>= 1;
  }

  // Then, note this event as being ready to be handled.
  // (Note that because this function (unlike others in the library) can be called from an external thread, we do this last, to
  //  reduce the risk of a race condition.)
  fTriggersAwaitingHandling |= eventTriggerId;
}


////////// HandlerSet (etc.) implementation //////////

HandlerDescriptor::HandlerDescriptor(HandlerDescriptor* nextHandler)
  : conditionSet(0), handlerProc(NULL) {
  // Link this descriptor into a doubly-linked list:
  if (nextHandler == this) { // initialization
    fNextHandler = fPrevHandler = this;
  } else {
    fNextHandler = nextHandler;
    fPrevHandler = nextHandler->fPrevHandler;
    nextHandler->fPrevHandler = this;
    fPrevHandler->fNextHandler = this;
  }
}

HandlerDescriptor::~HandlerDescriptor() {
  // Unlink this descriptor from a doubly-linked list:
  fNextHandler->fPrevHandler = fPrevHandler;
  fPrevHandler->fNextHandler = fNextHandler;
}

HandlerSet::HandlerSet()
  : fHandlers(&fHandlers) {
  fHandlers.socketNum = -1; // shouldn't ever get looked at, but in case...
}

HandlerSet::~HandlerSet() {
  // Delete each handler descriptor:
  while (fHandlers.fNextHandler != &fHandlers) {
    delete fHandlers.fNextHandler; // changes fHandlers->fNextHandler
  }
}

void HandlerSet
::assignHandler(int socketNum, int conditionSet, TaskScheduler::BackgroundHandlerProc* handlerProc, void* clientData) {
  // First, see if there's already a handler for this socket:
  HandlerDescriptor* handler = lookupHandler(socketNum);
  if (handler == NULL) { // No existing handler, so create a new descr:
    handler = new HandlerDescriptor(fHandlers.fNextHandler);
    handler->socketNum = socketNum;
  }

  handler->conditionSet = conditionSet;
  handler->handlerProc = handlerProc;
  handler->clientData = clientData;
}

void HandlerSet::clearHandler(int socketNum) {
  HandlerDescriptor* handler = lookupHandler(socketNum);
  delete handler;
}

void HandlerSet::moveHandler(int oldSocketNum, int newSocketNum) {
  HandlerDescriptor* handler = lookupHandler(oldSocketNum);
  if (handler != NULL) {
    handler->socketNum = newSocketNum;
  }
}

HandlerDescriptor* HandlerSet::lookupHandler(int socketNum) {
  HandlerDescriptor* handler;
  HandlerIterator iter(*this);
  while ((handler = iter.next()) != NULL) {
    if (handler->socketNum == socketNum) break;
  }
  return handler;
}

HandlerIterator::HandlerIterator(HandlerSet& handlerSet)
  : fOurSet(handlerSet) {
  reset();
}

HandlerIterator::~HandlerIterator() {
}

void HandlerIterator::reset() {
  fNextPtr = fOurSet.fHandlers.fNextHandler;
}

HandlerDescriptor* HandlerIterator::next() {
  HandlerDescriptor* result = fNextPtr;
  if (result == &fOurSet.fHandlers) { // no more
    result = NULL;
  } else {
    fNextPtr = fNextPtr->fNextHandler;
  }

  return result;
}
