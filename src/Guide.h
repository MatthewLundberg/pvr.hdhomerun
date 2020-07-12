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
#include "Settings.h"
#include "Entry.h"
#include "UniqueID.h"
#include <json/json.h>
#include <kodi/addon-instance/pvr/EPG.h>

namespace PVRHDHomeRun
{

class PVR_HDHR;
class GuideNumber
{
private:
    static const uint32_t SubchannelLimit = 100000;
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
        return (_channel * SubchannelLimit) + _subchannel;
    }
    operator uint32_t() const
    {
        return ID();
    }
};

bool operator<(const GuideNumber&, const GuideNumber&);
bool operator==(const GuideNumber&, const GuideNumber&);

class GuideEntry : public Entry
{
private:
    uint32_t _id;
    friend class Guide;
    friend bool operator<(const GuideEntry&, const GuideEntry&);
    friend bool operator==(const GuideEntry&, const GuideEntry&);
public:
    GuideEntry(const Json::Value&);


public:
    operator Interval() const
    {
        return {_starttime, _endtime};
    }
    kodi::addon::PVREPGTag Epg_Tag(uint32_t number) const;
};

bool operator<(const GuideEntry&, const GuideEntry&);
bool operator==(const GuideEntry&, const GuideEntry&);

class Guide
{
public:
    Guide(const Json::Value&);
    Guide() = default;

    bool AddEntry(PVR_HDHR* parent, GuideEntry&, uint32_t number);
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
    UniqueID<uint32_t>   _sequence;

    IntervalSet          _times;
    IntervalSet          _requests;
    Lockable             _lock;
};


} // namespace PVRHDHomeRun
