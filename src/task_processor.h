/*MT*
*/

#ifndef __TASK_PROCESSOR_H__
#define __TASK_PROCESSOR_H__

#include <condition_variable>
#include "common.h"
#include "online_service.h"
#include "singleton.h"
#include "generic_task.h"

class TPFetchOnlineContentTask : public GenericTask
{
public:
    TPFetchOnlineContentTask(std::shared_ptr<OnlineService> service, 
                             std::shared_ptr<Layout> layout, bool cancellable,
                             bool unscheduled_refresh);
    virtual void run();

protected:
    std::shared_ptr<OnlineService> service;
    std::shared_ptr<Layout> layout;
    bool unscheduled_refresh;

};

class TaskProcessor : public Singleton<TaskProcessor>
{
public:    
    TaskProcessor();
    virtual ~TaskProcessor();
    void addTask(std::shared_ptr<GenericTask> task);
    virtual void init();
    virtual void shutdown();
    std::shared_ptr<zmm::Array<GenericTask> > getTasklist();
    std::shared_ptr<GenericTask> getCurrentTask();
    void invalidateTask(unsigned int taskID);

protected:
    pthread_t taskThread;
    std::condition_variable cond;
    bool shutdownFlag;
    bool working;
    unsigned int taskID;
    std::shared_ptr<zmm::ObjectQueue<GenericTask> > taskQueue;
    std::shared_ptr<GenericTask> currentTask;

    static void *staticThreadProc(void *arg);

    void threadProc();
};

#endif//__TASK_PROCESSOR_H__
