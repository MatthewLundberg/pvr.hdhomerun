#pragma once
/*
 *      Copyright (C) 2017 Matthew Lundberg <matthew.k.lundberg@gmail.com>
 *      https://github.com/MatthewLundberg/pvr.hdhomerun
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XBMC; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#include <mutex>

namespace PVRHDHomeRun
{

class Lockable {
public:
    virtual ~Lockable() {}
    void LockObject() {
        _lock.lock();
    }
    bool TryLock() {
        return _lock.try_lock();
    }
    void UnlockObject() {
        _lock.unlock();
    }
private:
    std::recursive_mutex _lock;
};

class TryLock {
    TryLock(Lockable* obj) : _obj(obj)
    {}
    TryLock(Lockable& obj) : TryLock(&obj)
    {}
    ~TryLock()
    {
        if (_sts)
            _obj->UnlockObject();
    }
    operator bool() {
        if (_sts)
            return _sts;
        return _sts = _obj->TryLock();
    }
private:
    Lockable* _obj;
    bool      _sts;
};

class Lock {
public:
    Lock(Lockable* obj) : _obj(obj)
    {
        _obj->LockObject();
    }
    Lock(Lockable& obj) : Lock(&obj)
    {}
    ~Lock()
    {
        _obj->UnlockObject();
    }
private:
    Lockable* _obj;
};

} // namespace PVRHDHomeRun
