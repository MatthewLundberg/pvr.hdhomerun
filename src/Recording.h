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

#include "Entry.h"
#include <string>
#include <cstdint>
#include <set>
#include <map>
#include <json/json.h>
#include <libXBMC_pvr.h>

namespace PVRHDHomeRun
{

class StorageDevice;

class RecordingEntry : public Entry
{
public:
    RecordingEntry(const Json::Value&);

    std::string _category;
    std::string _affiliate;
    std::string _channelimg;
    std::string _channelname;
    std::string _channelnum;
    std::string _programID;
    std::string _groupID;
    std::string _grouptitle;
    std::string _playurl;
    std::string _cmdurl;

    time_t _recordstarttime;
    time_t _recordendtime;

    operator PVR_RECORDING() const
    {
        return _pvr_recording();
    }
private:
    PVR_RECORDING _pvr_recording() const;
};

bool operator<(const RecordingEntry&, const RecordingEntry&);
bool operator==(const RecordingEntry&, const RecordingEntry&);

class RecordingRule : public Entry
{
public:
    RecordingRule(const Json::Value& json);
    std::string _recordingruleID;

    time_t      _datetimeonly = 0;
    std::string _channelonly;
    int         _startpadding = 0;
    int         _endpadding   = 0;
};

bool operator<(const RecordingRule&, const RecordingRule&);
bool operator==(const RecordingRule&, const RecordingRule&);

class Recording
{
    std::map<std::string, RecordingEntry> _records;

    // Used to determine if records need to be removed
    std::map<std::string, std::set<const StorageDevice*>> _devices;
    bool _diff;
    template<typename T> bool _update_end(T& c)
    {
        auto it = c.begin();
        while (it != c.end())
        {
            const auto& id = it->first;
            const auto& dev = _devices.find(id);
            if (dev == _devices.end())
            {
                _diff = true;
                it = c.erase(it);
            }
            else
                ++ it;
        }
        return _diff;
    }

public:
    void UpdateBegin();
    void UpdateEntry(const Json::Value&, const StorageDevice*);
    bool UpdateEntryEnd();
    size_t size();

    const std::map<std::string, RecordingEntry>& Records() const { return _records; };
};

} // namespace PVRHDHomeRun
