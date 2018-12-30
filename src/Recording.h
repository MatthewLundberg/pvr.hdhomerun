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

class StringEntry : public Entry
{
public:
    StringEntry(const Json::Value& v) : Entry(v) {};
    virtual ~StringEntry() = default;
    virtual const std::string& ID() const = 0;
};

class RecordingEntry : public StringEntry
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

    const std::string& ID() const override
    {
        return _programID;
    }

    operator PVR_RECORDING() const
    {
        return _pvr_recording();
    }
private:
    PVR_RECORDING _pvr_recording() const;
};

bool operator<(const RecordingEntry&, const RecordingEntry&);
bool operator==(const RecordingEntry&, const RecordingEntry&);

class RecordingRule : public StringEntry
{
public:
    RecordingRule(const Json::Value& json);
    std::string _recordingruleID;

    time_t      _datetimeonly = 0;
    std::string _channelonly;
    int         _startpadding = 0;
    int         _endpadding   = 0;

    const std::string& ID() const override
    {
        return _recordingruleID;
    }
};

bool operator<(const RecordingRule&, const RecordingRule&);
bool operator==(const RecordingRule&, const RecordingRule&);

class Recording
{
    std::map<std::string, RecordingEntry> _records;
    std::map<std::string, RecordingRule>  _rules;

    // Used to determine if records need to be removed
    std::set<std::string> _ids_in_use;
    bool _diff;
    void _update_begin()
    {
        _ids_in_use.clear();
        _diff = false;
    }
    template<typename T> bool _update_end(T& c)
    {
        auto it = c.begin();
        while (it != c.end())
        {
            const auto& id = it->first;
            const auto& dev = _ids_in_use.find(id);
            if (dev == _ids_in_use.end())
            {
                _diff = true;
                it = c.erase(it);
            }
            else
                ++ it;
        }
        return _diff;
    }
    template<typename T> void _update(T& c, const Json::Value& json)
    {
        for (const auto& j: json)
        {
            typename T::value_type::second_type entry(j);
            const auto& id = entry.ID();
            _ids_in_use.insert(id);

            auto i = c.find(id);
            if (i == c.end())
            {
                _diff = true;
                auto idc = id; // copy ID to allow a move for the structure
                c.emplace(std::move(idc), std::move(entry));
            }
            else
            {
                if ((i->second) != entry)
                {
                    _diff = true;
                    i->second = entry;
                }
            }
        }
    }

public:
    void UpdateBegin();
    void UpdateEntry(const Json::Value&);
    bool UpdateEntryEnd();
    void UpdateRule(const Json::Value&);
    bool UpdateRuleEnd();
    size_t size();

    const std::map<std::string, RecordingEntry>& Records() const { return _records; };
};

} // namespace PVRHDHomeRun
