/*MT*
    
    MediaTomb - http://www.mediatomb.cc/
    
    autoscan_inotify.h - this file is part of MediaTomb.
    
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

/// \file autoscan_inotify.h
#ifndef __AUTOSCAN_INOTIFY_H__
#define __AUTOSCAN_INOTIFY_H__

#include <memory>
#include <mutex>
#include <unordered_map>

#include "zmm/zmmf.h"
#include "autoscan.h"
#include "mt_inotify.h"

#define INOTIFY_ROOT -1
#define INOTIFY_UNKNOWN_PARENT_WD -2

enum inotify_watch_type_t
{
    InotifyWatchTypeNonexisting,
    InotifyWatchTypeAutoscan
};

class AutoscanInotify : public zmm::Object
{
public:
    AutoscanInotify();
    virtual ~AutoscanInotify();
    void init();
    
    /// \brief shutdown the inotify thread
    /// 
    /// warning: currently doesn't remove all the remaining inotify watches!
    void shutdown();
    
    /// \brief Start monitoring a directory
    void monitor(std::shared_ptr<AutoscanDirectory> dir);
    
    /// \brief Stop monitoring a directory
    void unmonitor(std::shared_ptr<AutoscanDirectory> dir);
    
private:
    static void *staticThreadProc(void *arg);
    void threadProc();
    
    pthread_t thread;
    
    std::shared_ptr<Inotify> inotify;

    std::mutex mutex;
    using AutoLock = std::lock_guard<std::mutex>;

    std::shared_ptr<zmm::ObjectQueue<AutoscanDirectory> > monitorQueue;
    std::shared_ptr<zmm::ObjectQueue<AutoscanDirectory> > unmonitorQueue;
    
    // event mask with events to watch for (set by constructor);
    int events;
    
    enum watch_type_t
    {
        WatchAutoscanType,
        WatchMoveType
    };
    
    class Watch : public zmm::Object
    {
    public:
        Watch(watch_type_t type)
        {
            this->type = type;
        }
        watch_type_t getType() { return type; }
    private:
        watch_type_t type;
    };
    
    class WatchAutoscan : public Watch
    {
    public:
        WatchAutoscan(bool startPoint, std::shared_ptr<AutoscanDirectory> adir, zmm::String normalizedAutoscanPath) : Watch(WatchAutoscanType)
        {
            setAutoscanDirectory(adir);
            setNormalizedAutoscanPath(normalizedAutoscanPath);
            setNonexistingPathArray(nullptr);
            this->startPoint = startPoint;
            this->descendants = nullptr;
        }
        std::shared_ptr<AutoscanDirectory> getAutoscanDirectory() { return adir; }
        void setAutoscanDirectory(std::shared_ptr<AutoscanDirectory> adir) { this->adir = adir; }
        zmm::String getNormalizedAutoscanPath() { return normalizedAutoscanPath; }
        void setNormalizedAutoscanPath(zmm::String normalizedAutoscanPath) { this->normalizedAutoscanPath = normalizedAutoscanPath; }
        bool isStartPoint() { return startPoint; }
        std::shared_ptr<zmm::Array<zmm::StringBase> > getNonexistingPathArray() { return nonexistingPathArray; }
        void setNonexistingPathArray(std::shared_ptr<zmm::Array<zmm::StringBase> > nonexistingPathArray) { this->nonexistingPathArray = nonexistingPathArray; }
        void addDescendant(int wd)
        {
            if (descendants == nullptr)
                descendants = std::shared_ptr<zmm::IntArray>(new zmm::IntArray());
            descendants->append(wd);
        }
        std::shared_ptr<zmm::IntArray> getDescendants() { return descendants; }
    private:
        std::shared_ptr<AutoscanDirectory> adir;
        bool startPoint;
        std::shared_ptr<zmm::IntArray> descendants;
        zmm::String normalizedAutoscanPath;
        std::shared_ptr<zmm::Array<zmm::StringBase> > nonexistingPathArray;
    };
    
    class WatchMove : public Watch
    {
    public:
        WatchMove(int removeWd) : Watch(WatchMoveType)
        {
            this->removeWd = removeWd;
        }
        int getRemoveWd() { return removeWd; }
    private:
        int removeWd;
    };
    
    class Wd : public zmm::Object
    {
    public:
        Wd(zmm::String path, int wd, int parentWd)
        {
            wdWatches = std::shared_ptr<zmm::Array<Watch> >(new zmm::Array<Watch>(1));
            this->path = path;
            this->wd = wd;
            this->parentWd = parentWd;
        }
        zmm::String getPath() { return path; }
        int getWd() { return wd; }
        int getParentWd() { return parentWd; }
        void setParentWd(int parentWd) { this->parentWd = parentWd; }
        std::shared_ptr<zmm::Array<Watch> > getWdWatches() { return wdWatches; }
    private:
        std::shared_ptr<zmm::Array<Watch> > wdWatches;
        zmm::String path;
        int parentWd;
        int wd;
    };

    std::shared_ptr<std::unordered_map<int, std::shared_ptr<Wd>> > watches;
    
    zmm::String normalizePathNoEx(zmm::String path);
    
    void monitorUnmonitorRecursive(zmm::String startPath, bool unmonitor, std::shared_ptr<AutoscanDirectory> adir, zmm::String normalizedAutoscanPath, bool startPoint);
    int monitorDirectory(zmm::String path, std::shared_ptr<AutoscanDirectory> adir, zmm::String normalizedAutoscanPath, bool startPoint, std::shared_ptr<zmm::Array<zmm::StringBase> > pathArray = nullptr);
    void unmonitorDirectory(zmm::String path, std::shared_ptr<AutoscanDirectory> adir);
    
    std::shared_ptr<WatchAutoscan> getAppropriateAutoscan(std::shared_ptr<Wd> wdObj, std::shared_ptr<AutoscanDirectory> adir);
    std::shared_ptr<WatchAutoscan> getAppropriateAutoscan(std::shared_ptr<Wd> wdObj, zmm::String path);
    std::shared_ptr<WatchAutoscan> getStartPoint(std::shared_ptr<Wd> wdObj);
    
    bool removeFromWdObj(std::shared_ptr<Wd> wdObj, std::shared_ptr<Watch> toRemove);
    bool removeFromWdObj(std::shared_ptr<Wd> wdObj, std::shared_ptr<WatchAutoscan> toRemove);
    bool removeFromWdObj(std::shared_ptr<Wd> wdObj, std::shared_ptr<WatchMove> toRemove);
    
    void monitorNonexisting(zmm::String path, std::shared_ptr<AutoscanDirectory> adir, zmm::String normalizedAutoscanPath);
    void recheckNonexistingMonitor(int curWd, std::shared_ptr<zmm::Array<zmm::StringBase> > nonexistingPathArray, std::shared_ptr<AutoscanDirectory> adir, zmm::String normalizedAutoscanPath);
    void recheckNonexistingMonitors(int wd, std::shared_ptr<Wd> wdObj);
    void removeNonexistingMonitor(int wd, std::shared_ptr<Wd> wdObj, std::shared_ptr<zmm::Array<zmm::StringBase> > pathAr);
    
    int watchPathForMoves(zmm::String path, int wd);
    int addMoveWatch(zmm::String path, int removeWd, int parentWd);
    void checkMoveWatches(int wd, std::shared_ptr<Wd> wdObj);
    void removeWatchMoves(int wd);
    
    void addDescendant(int startPointWd, int addWd, std::shared_ptr<AutoscanDirectory> adir);
    void removeDescendants(int wd);
    
    /// \brief is set to true by shutdown() if the inotify thread should terminate
    bool shutdownFlag;
};

#endif // __AUTOSCAN_INOTIFY_H__
