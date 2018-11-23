#pragma once
/*
 *      Copyright (C) 2017-2018 Matthew Lundberg <matthew.k.lundberg@gmail.com>
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

#include "IntervalSet.h"
#include "Addon.h"
#include <json/json.h>

namespace PVRHDHomeRun
{

class GuideNumber
{
private:
    static const uint32_t SubchannelLimit = 10000;
public:
    GuideNumber(const Json::Value&);
    GuideNumber(const GuideNumber&) = default;
    GuideNumber(uint32_t id)
    {
        _channel = id / SubchannelLimit;
         id %= SubchannelLimit;
         _subchannel = id;
    }
    virtual ~GuideNumber() = default;

    std::string _guidenumber;
    std::string _guidename;

    uint32_t _channel;
    uint32_t _subchannel;

    std::string extendedName() const;
    std::string toString() const;

    uint32_t ID() const
    {
        // _subchannel < 1000, _nameidx < 100
        return (_channel * SubchannelLimit) + _subchannel;
    }
    operator uint32_t() const
    {
        return ID();
    }

    bool operator<(const GuideNumber&) const;
    bool operator==(const GuideNumber&) const;
};

class GuideEntry
{
public:
    GuideEntry(const Json::Value&);

    time_t      _starttime;
    time_t      _endtime;
    time_t      _originalairdate;
    std::string _title;
    std::string _episodenumber;
    std::string _episodetitle;
    std::string _synopsis;
    std::string _imageURL;
    std::string _seriesID;
    uint32_t    _genre;
    uint32_t    _id;

    bool operator<(const GuideEntry& rhs) const
    {
        return _starttime < rhs._starttime;
    }
    bool operator==(const GuideEntry& rhs) const
    {
        return
                _starttime         == rhs._starttime
               && _endtime         == rhs._endtime
               && _originalairdate == rhs._originalairdate
               && _title           == rhs._title
               && _episodenumber   == rhs._episodenumber
               && _synopsis        == rhs._synopsis
               && _imageURL        == rhs._imageURL
               && _seriesID        == rhs._seriesID
                ;
    }

public:
    operator Interval() const
    {
        return {_starttime, _endtime};
    }
    EPG_TAG Epg_Tag(uint32_t number) const;
};

class Guide
{
public:
    Guide(const Json::Value&);
    Guide() = default;

    bool AddEntry(GuideEntry&, uint32_t number);
    void AddRequest(const Interval& r)
    {
        _requests.Add(r);
    }
    void RemoveRequest(const Interval& i)
    {
        _requests.Remove(i);
    }
    void _age_out(uint32_t number, time_t limit);

    const IntervalSet& Times() const
    {
        return _times;
    }
    const IntervalSet& Requests() const
    {
        return _requests;
    }
    const std::string& GuideName() const
    {
        return _guidename;
    }
    const std::string& Affiliate() const
    {
        return _affiliate;
    }
    const std::string& ImageURL() const
    {
        return _imageURL;
    }
    const std::set<GuideEntry>& Entries() const
    {
        return _entries;
    }

private:
    std::string          _guidename;
    std::string          _affiliate;
    std::string          _imageURL;
    std::set<GuideEntry> _entries;
    uint32_t             _nextidx = 1;

    IntervalSet          _times;
    IntervalSet          _requests;
};


} // namespace PVRHDHomeRun
