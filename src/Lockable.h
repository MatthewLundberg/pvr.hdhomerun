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

template <typename T>
class auto_lock {
public:
    auto_lock(T& obj) : _obj(obj)
    {
        _obj.lock();
    }
    ~auto_lock()
    {
        _obj.unlock();
    }
private:
    T& _obj;
};
template <typename T> auto_lock<T> Lock(T& mutex) { return auto_lock<T>(mutex); }

template <typename T>
class auto_try_lock {
    auto_try_lock(T& obj) : _obj(obj)
    {}
    ~auto_try_lock()
    {
        auto Lock = Lock(_lock);
        if (_sts)
            _obj->unlock();
    }
    operator bool() {
        auto Lock = Lock(_lock);
        if (_sts)
            return _sts;
        return _sts = _obj.try_lock();
    }
private:
    std::mutex _lock;
    T&         _obj;
    bool       _sts;
};
template <typename T> auto_try_lock<T> TryLock(T& mutex) { return auto_try_lock<T>(mutex); }


} // namespace PVRHDHomeRun
