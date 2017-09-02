/*MT*
    
    MediaTomb - http://www.mediatomb.cc/
    
    update_manager.cc - this file is part of MediaTomb.
    
    Copyright (C) 2005 Gena Batyan <bgeradz@mediatomb.cc>,
                       Sergey 'Jin' Bostandzhyan <jin@mediatomb.cc>
    
    Copyright (C) 2006-2010 Gena Batyan <bgeradz@mediatomb.cc>,
                            Sergey 'Jin' Bostandzhyan <jin@mediatomb.cc>,
                            Leonhard Wimmer <leo@mediatomb.cc>
    
    MediaTomb is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License version 2
    as published by the Free Software Foundation.
    
    MediaTomb is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
    
    You should have received a copy of the GNU General Public License
    version 2 along with MediaTomb; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
    
    $Id$
*/

/// \file update_manager.cc

#include "update_manager.h"

#include "upnp_cds.h"
#include "storage.h"
#include "tools.h"
#include <sys/types.h>
#include <csignal>
#include <chrono>

/* following constants in milliseconds */
#define SPEC_INTERVAL 2000
#define MIN_SLEEP 1

#define MAX_OBJECT_IDS 1000
#define MAX_OBJECT_IDS_OVERLOAD 30
#define OBJECT_ID_HASH_CAPACITY 3109

using namespace zmm;
using namespace std;

UpdateManager::UpdateManager() : Singleton<UpdateManager>(),
    objectIDHash(make_shared<unordered_set<int>>()),
    shutdownFlag(false), flushPolicy(FLUSH_SPEC),
    lastContainerChanged(INVALID_OBJECT_ID)
{
}

void UpdateManager::init()
{
    /*
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    */
    
    pthread_create(
        &updateThread,
        nullptr, // &attr, // attr
        UpdateManager::staticThreadProc,
        this
    );
    
    //cond->wait();
    //pthread_attr_destroy(&attr);
}

void UpdateManager::shutdown()
{
    SPDLOG_TRACE(l, "start");
    unique_lock<mutex_type> lock(mutex);
    shutdownFlag = true;
    SPDLOG_TRACE(l, "signalling...\n");
    cond.notify_one();
    lock.unlock();
    SPDLOG_TRACE(l, "waiting for thread\n");
    if (updateThread)
        pthread_join(updateThread, nullptr);
    updateThread = 0;
    SPDLOG_TRACE(l, "end");
}

void UpdateManager::containersChanged(Ref<IntArray> objectIDs, int flushPolicy)
{
    if (objectIDs == nullptr)
        return;
    unique_lock<mutex_type> lock(mutex);
    // signalling thread if it could have been idle, because 
    // there were no unprocessed updates
    bool signal = (! haveUpdates());
    // signalling if the flushPolicy changes, so the thread recalculates
    // the sleep time
    if (flushPolicy > this->flushPolicy)
    {
        this->flushPolicy = flushPolicy;
        signal = true;
    }
    int size = objectIDs->size();
    int hashSize = objectIDHash->size();
    bool split = (hashSize + size >= MAX_OBJECT_IDS + MAX_OBJECT_IDS_OVERLOAD);
    for (int i = 0; i < size; i++)
    {
        int objectID = objectIDs->get(i);
        if (objectID != lastContainerChanged)
        {
            //SPDLOG_TRACE(l, "containerChanged. id: %d, signal: %d\n", objectID, signal);
            objectIDHash->insert(objectID);
            if (split && objectIDHash->size() > MAX_OBJECT_IDS)
            {
                while(objectIDHash->size() > MAX_OBJECT_IDS)
                {
                    SPDLOG_TRACE(l, "in-between signalling...\n");
                    cond.notify_one();
                    lock.unlock();
                    lock.lock();
                }
            }
        }
    }
    if (objectIDHash->size() >= MAX_OBJECT_IDS)
        signal = true;
    if (signal)
    {
        SPDLOG_TRACE(l, "signalling...\n");
        cond.notify_one();
    }
}

void UpdateManager::containerChanged(int objectID, int flushPolicy)
{
    if (objectID == INVALID_OBJECT_ID)
        return;
    AutoLock lock(mutex);
    if (objectID != lastContainerChanged || flushPolicy > this->flushPolicy)
    {
        // signalling thread if it could have been idle, because 
        // there were no unprocessed updates
        bool signal = (! haveUpdates());
        SPDLOG_TRACE(l, "containerChanged. id: {}, signal: {}", objectID, signal);
        objectIDHash->insert(objectID);
        
        // signalling if the hash gets too full
        if (objectIDHash->size() >= MAX_OBJECT_IDS)
            signal = true;
        
        // very simple caching, but it get's a lot of hits
        lastContainerChanged = objectID;
        
        // signalling if the flushPolicy changes, so the thread recalculates
        // the sleep time
        if (flushPolicy > this->flushPolicy)
        {
            this->flushPolicy = flushPolicy;
            signal = true;
        }
        if (signal)
        {
            SPDLOG_TRACE(l, "signalling...\n");
            cond.notify_one();
        }
    }
    else
    {
        SPDLOG_TRACE(l, "last container changed!\n");
    }
}

/* private stuff */

void UpdateManager::threadProc()
{
    struct timespec lastUpdate;
    getTimespecNow(&lastUpdate);
    
    unique_lock<mutex_type> lock(mutex);
    //cond.notify_one();
    while (! shutdownFlag)
    {
        if (haveUpdates())
        {
            long sleepMillis = 0;
            struct timespec now;
            getTimespecNow(&now);
            long timeDiff = getDeltaMillis(&lastUpdate, &now);
            switch (flushPolicy)
            {
                case FLUSH_SPEC:
                    sleepMillis = SPEC_INTERVAL - timeDiff;
                    break;
                case FLUSH_ASAP:
                    sleepMillis = 0;
                    break;
            }
            bool sendUpdates = true;
            if (sleepMillis >= MIN_SLEEP && objectIDHash->size() < MAX_OBJECT_IDS)
            {
                struct timespec timeout;
                getTimespecAfterMillis(sleepMillis, &timeout, &now);
                SPDLOG_TRACE(l, "threadProc: sleeping for %ld millis\n", sleepMillis);
                
                cv_status ret = cond.wait_for(lock, chrono::milliseconds(sleepMillis));
                
                if (! shutdownFlag)
                {
                    if (ret == cv_status::timeout)
                        sendUpdates = false;
                }
                else
                    sendUpdates = false;
            }
            
            if (sendUpdates)
            {
                SPDLOG_TRACE(l, "sending updates...\n");
                lastContainerChanged = INVALID_OBJECT_ID;
                flushPolicy = FLUSH_SPEC;
                String updateString;

                try
                {
                    updateString = Storage::getInstance()->incrementUpdateIDs(objectIDHash);
                    objectIDHash->clear(); // hash_data_array will be invalid after clear()
                }
                catch (const Exception & e)
                {
                    e.printStackTrace();
                    l->error("Fatal error when sending updates: {}", e.getMessage().c_str());
                    l->error("Forcing MediaTomb shutdown.\n");
                    kill(0, SIGINT);
                }
                lock.unlock(); // we don't need to hold the lock during the sending of the updates
                if (string_ok(updateString))
                {
                    try
                    {
                    ContentDirectoryService::getInstance()->subscription_update(updateString);
                    SPDLOG_TRACE(l, "updates sent.\n");
                    getTimespecNow(&lastUpdate);
                    }
                    catch (const Exception & e)
                    {
                        l->error("Fatal error when sending updates: {}", e.getMessage().c_str());
                        l->error("Forcing MediaTomb shutdown.\n");
                        kill(0, SIGINT);
                    }
                }
                else
                {
                    SPDLOG_TRACE(l, "NOT sending updates (string empty or invalid).\n");
                }
                lock.lock();
            }
        }
        else
        {
            //nothing to do
            cond.wait(lock);
        }
    }
}

void *UpdateManager::staticThreadProc(void *arg)
{
    auto l = spdlog::get("log");
    SPDLOG_TRACE(l, "starting update thread... thread: {}", pthread_self());
    auto *inst = (UpdateManager *)arg;
    inst->threadProc();
    Storage::getInstance()->threadCleanup();
    
    SPDLOG_TRACE(l, "update thread shut down. thread: {}", pthread_self());
    return nullptr;
}
