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

#include <ctime>
#include <set>
#include <string>

namespace PVRHDHomeRun
{
class Interval
{
public:
    Interval(time_t s, time_t e)
        : _start(s)
        , _end(e)
    {}
    Interval(const Interval&) = default;

    bool Contains(time_t t) const
    {
        return (t >= _start) && (t < _end);
    }
    bool operator<(const Interval& rhs) const
    {
        return _start < rhs._start;
    }
    time_t Start() const
    {
        return _start;
    }
    time_t End() const
    {
        return _end;
    }
    time_t Length() const
    {
        return _end - _start;
    }

    std::string toString() const;
    operator std::string() const
    {
        return toString();
    }

    time_t _start;
    time_t _end;
};
class IntervalSet
{
public:
    IntervalSet() = default;
    IntervalSet(const Interval& i)
    {
        _intervals.insert(i);
    }

    void Add(const Interval&, bool rebalance = true);
    void Add(const IntervalSet&);

    void Remove(const Interval&);
    void Remove(const IntervalSet&);

    std::string toString() const;
    operator std::string() const
    {
        return toString();
    }

    bool Contains(time_t t) const
    {
        for (auto& i : _intervals)
        {
            if (i.Contains(t))
                return true;
        }
        return false;
    }
    time_t Length() const;
    bool Empty() const
    {
        return _intervals.size() == 0;
    }
    time_t Start() const
    {
        return _intervals.begin()->_start;
    }
    time_t End() const
    {
        return _intervals.rbegin()->_end;
    }
    const Interval& Last() const
    {
        return *_intervals.rbegin();
    }
    const size_t Count() const
    {
        return _intervals.size();
    }

    void _rebalance();
    std::set<Interval> _intervals;
};

} // namespace PVRHDHomeRun
