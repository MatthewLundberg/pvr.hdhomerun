/*
 *      Copyright (C) 2017 Matthew Lundberg <matthew.k.lundberg@gmail.com>
 *      https://github.com/MatthewLundberg/pvr.hdhomerun
 *      http://www.pulse-eight.com/
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

#include "Utils.h"
#include "IntervalSet.h"

#include <iostream>
#include <sstream>
#include <iterator>
#include <numeric>

namespace PVRHDHomeRun
{

std::string Interval::toString() const
{
    std::stringstream ss;
    ss << '[' << FormatTime(_start) << ',' << FormatTime(_end) << ')';
    return ss.str();
}

std::string IntervalSet::toString() const
{
    std::stringstream ss;
    std::ostream_iterator<std::string> it(ss);
    std::copy(_intervals.begin(), _intervals.end(), it);
    return ss.str();
}

void IntervalSet::_rebalance()
{
    auto it = _intervals.begin();
    if (it == _intervals.end())
        return;

    auto prev = it;
    it ++;

    while (it != _intervals.end())
    {
        if (it->_start <= prev->_end)
        {
            auto& p = const_cast<Interval&>(*prev);

            if (it->_end > p._end)
                p._end = it->_end;

            it = _intervals.erase(it);
        }
        else
        {
            prev = it;
            it ++;
        }
    }
}
void IntervalSet::Add(const Interval& o, bool rebalance)
{
    _intervals.insert(o);
    if (rebalance)
        _rebalance();
}
void IntervalSet::Add(const IntervalSet& set)
{
    for (auto& o : set._intervals)
    {
        Add(o, false);
    }
    _rebalance();
}

void IntervalSet::Remove(const Interval& o)
{
    bool action;
    do {
        action = false;

        for (auto it = _intervals.begin(); it != _intervals.end(); it++)
        {
            auto& i = const_cast<Interval&>(*it);

            if (o._end <= i._start)
            {
                // current interval start after removal end, done
                break;
            }

            if (o._start >= i._end)
            {
                // Current interval end before removal start
                continue;
            }

            // o._end   > i._start
            // o._start < i._end

            if (o._start <= i._start && o._end >= i._end)
            {
                // Covers entire interval, remove it
                // Breaking out of loop, no need to update 'it'.
                _intervals.erase(it);
                action = true;
                break;
            }
            else if (o._start > i._start && o._end < i._end)
            {
                // Interval to be removed resides inside this interval, split current interval

                _intervals.insert({o._end, i._end});
                i._end = o._start;
                action = true;
                break;
            }
            else if (o._start <= i._start && o._end < i._end)
            {
                // o._end is within this interval, move start

                i._start = o._end;
                action = true;
            }
            else if (o._start > i._start && o._end >= i._end)
            {
                // o._start is within this interval, move end

                i._end = o._start;
                action = true;
            }
        }
    } while (action);
}
void IntervalSet::Remove(const IntervalSet& set)
{
    for (auto& o: set._intervals)
    {
        Remove(o);
    }
}
time_t IntervalSet::Length() const
{
    return std::accumulate(
            _intervals.begin(),
            _intervals.end(),
            0,
            [](time_t l, const Interval& i) {
        return l + i.Length();
    }
    );
}

} //namespace PVRHDHomeRun

