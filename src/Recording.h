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
    std::string _image;
    std::string _poster;
    std::string _programid;
    std::string _seriesid;
    std::string _synposis;
    std::string _title;
    std::string _groupid;
    std::string _grouptitle;
    std::string _playurl;
    std::string _cmdurl;

    time_t _starttime;
    time_t _endtime;

    time_t rstarttime() const;
    time_t rendtime() const;

    operator PVR_RECORDING() const
    {
        return _pvr_recording();
    }
private:
    PVR_RECORDING _pvr_recording() const;
};

bool operator<(const RecordingEntry&, const RecordingEntry&);
bool operator==(const RecordingEntry&, const RecordingEntry&);
bool operator!=(const RecordingEntry&, const RecordingEntry&);

class Recording
{
    std::map<std::string, RecordingEntry> _records;
    std::map<std::string, std::set<const StorageDevice*>> _devices;
    bool _diff;

public:
    void UpdateBegin();
    void UpdateData(const Json::Value&, const StorageDevice*);
    bool UpdateEnd();
    size_t size();

    const std::map<std::string, RecordingEntry>& Records() const { return _records; };
    const std::map<std::string, std::set<const StorageDevice*>>& Devices() const { return _devices; };
};


} // namespace PVRHDHomeRun
