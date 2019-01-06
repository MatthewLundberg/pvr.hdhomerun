#pragma once
/*
 *      Copyright (C) 2017-2019 Matthew Lundberg <matthew.k.lundberg@gmail.com>
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

#include "Lockable.h"
#include <set>

namespace PVRHDHomeRun
{

template<typename T>
class UniqueID : public Lockable
{
public:
    UniqueID() = default;

    UniqueID(const UniqueID&) = delete;
    UniqueID& operator=(const UniqueID&) = delete;

    T acquire()
    {
        Lock lock(this);

        if (_free.size())
        {
            auto it = _free.begin();
            auto value = *it;
            _free.erase(it);
            return value;
        }
        auto value = _next;
        _next ++;
        return value;
    }

    void release(T value)
    {
        Lock lock(this);

        _free.insert(value);

        while (_free.size())
        {
            value = *_free.rbegin();
            if (value < (_next - 1))
                break;
            _free.erase(value);
            _next --;
        }
    }

private:
    std::set<T> _free;
    T           _next = 0;
};

}
