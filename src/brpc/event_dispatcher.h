// Copyright (c) 2014 Baidu, Inc.
// 
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// 
//     http://www.apache.org/licenses/LICENSE-2.0
// 
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Authors: Ge,Jun (gejun@baidu.com)
//          Rujie Jiang (jiangrujie@baidu.com)

#ifndef BRPC_EVENT_DISPATCHER_H
#define BRPC_EVENT_DISPATCHER_H

#include "butil/macros.h"                     // DISALLOW_COPY_AND_ASSIGN
#include "bthread/types.h"                   // bthread_t, bthread_attr_t
#include "brpc/socket.h"                     // Socket, SocketId


namespace brpc {

// brpc的网络框架
// brpc使用epoll+ET网络模型，基于其实现网络数据收发
// ET与LT两种模式主要差别在于编程模型上，如果我们使用主线程唤醒的方式读取数据
// 所有数据读取操作在主线程完成，这种编程模式下ET和LT效率差不多，因为ET与LT的
// 差别就在于ET下如果一个fd上的数据没有读完，epoll不会再将这个fd唤醒通知用户
// 而LT模式下则是只要这个fd上有未读取完的数据就会一直唤醒用户进行读取。
// 所以如果在多线程模型中，用户唤醒之后，将fd分发到其他线程中，不占用主线程资源
// 这种编程模式下ET的效率会比LT高，因为主线程不需要被频繁唤醒
// Dispatch edge-triggered events of file descriptors to consumers
// running in separate bthreads.
class EventDispatcher {
friend class Socket;
public:
    EventDispatcher();
    
    virtual ~EventDispatcher();

    // Start this dispatcher in a bthread.
    // Use |*consumer_thread_attr| (if it's not NULL) as the attribute to
    // create bthreads running user callbacks.
    // Returns 0 on success, -1 otherwise.
    virtual int Start(const bthread_attr_t* consumer_thread_attr);

    // True iff this dispatcher is running in a bthread
    bool Running() const;

    // Stop bthread of this dispatcher.
    void Stop();

    // Suspend calling thread until bthread of this dispatcher stops.
    void Join();

    // When edge-triggered events happen on `fd', call
    // `on_edge_triggered_events' of `socket_id'.
    // Notice that this function also transfers ownership of `socket_id',
    // When the file descriptor is removed from internal epoll, the Socket
    // will be dereferenced once additionally.
    // Returns 0 on success, -1 otherwise.
    int AddConsumer(SocketId socket_id, int fd);

    // Watch EPOLLOUT event on `fd' into epoll device. If `pollin' is
    // true, EPOLLIN event will also be included and EPOLL_CTL_MOD will
    // be used instead of EPOLL_CTL_ADD. When event arrives,
    // `Socket::HandleEpollOut' will be called with `socket_id'
    // Returns 0 on success, -1 otherwise and errno is set
    int AddEpollOut(SocketId socket_id, int fd, bool pollin);
    
    // Remove EPOLLOUT event on `fd'. If `pollin' is true, EPOLLIN event
    // will be kept and EPOLL_CTL_MOD will be used instead of EPOLL_CTL_DEL
    // Returns 0 on success, -1 otherwise and errno is set
    int RemoveEpollOut(SocketId socket_id, int fd, bool pollin);

private:
    DISALLOW_COPY_AND_ASSIGN(EventDispatcher);

    // Calls Run()
    static void* RunThis(void* arg);

    // Thread entry.
    void Run();

    // Remove the file descriptor `fd' from epoll.
    int RemoveConsumer(int fd);

    // The epoll to watch events.
    int _epfd;

    // false unless Stop() is called.
    volatile bool _stop;

    // identifier of hosting bthread
    bthread_t _tid;

    // The attribute of bthreads calling user callbacks.
    bthread_attr_t _consumer_thread_attr;

    // Pipe fds to wakeup EventDispatcher from `epoll_wait' in order to quit
    int _wakeup_fds[2];
};

EventDispatcher& GetGlobalEventDispatcher(int fd);

} // namespace brpc


#endif  // BRPC_EVENT_DISPATCHER_H
