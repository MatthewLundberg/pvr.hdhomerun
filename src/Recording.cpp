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

#include "Recording.h"
#include <iostream>

namespace PVRHDHomeRun
{

RecordingEntry::RecordingEntry(const Json::Value& v)
{
    _category    = v["Category"].asString();
    _affiliate   = v["ChannelAffiliate"].asString();
    _channelimg  = v["ChannelImageURL"].asString();
    _channelname = v["ChannelName"].asString();
    _channelnum  = v["ChannelNumber"].asString();
    _episodenum  = v["EpisodeNumber"].asString();
    _episodetitle= v["EpisodeTitle"].asString();
    _image       = v["ImageURL"].asString();
    _poster      = v["PosterURL"].asString();
    _programid   = v["ProgramID"].asString();
    _seriesid    = v["SeriesID"].asString();
    _synposis    = v["Synopsis"].asString();
    _title       = v["Title"].asString();
    _groupid     = v["DisplayGroupID"].asString();
    _grouptitle  = v["DisplayGroupTitle"].asString();
    _playurl     = v["PlayURL"].asString();
    _cmdurl      = v["CmdURL"].asString();

    _aired       = v["OriginalAirdate"].asUInt64();
    _rstarttime  = v["RecordStartTime"].asUInt64();
    _rendtime    = v["RecordEndTime"].asUInt64();
    _starttime   = v["StartTime"].asUInt64();
    _endtime     = v["EndTime"].asUInt64();
}

bool operator==(const RecordingEntry& a, const RecordingEntry& b)
{
    return a._category      == b._category &&
            a._affiliate    == b._affiliate &&
            a._channelimg   == b._channelimg &&
            a._channelname  == b._channelname &&
            a._channelnum   == b._channelnum &&
            a._episodenum   == b._episodenum &&
            a._episodetitle == b._episodetitle &&
            a._image        == b._image &&
            a._poster       == b._poster &&
            a._programid    == b._programid &&
            a._seriesid     == b._seriesid &&
            a._synposis     == b._synposis &&
            a._title        == b._title &&
            a._groupid      == b._groupid &&
            a._grouptitle   == b._grouptitle &&
            a._playurl      == b._playurl &&
            a._cmdurl       == b._cmdurl &&

            a._aired        == b._aired &&
            a._rstarttime   == b._rstarttime &&
            a._rendtime     == b._rendtime &&
            a._starttime    == b._starttime &&
            a._endtime      == b._endtime
            ;
}
bool operator!=(const RecordingEntry& a, const RecordingEntry& b)
{
    return !(a==b);
}

bool operator<(const RecordingEntry& x, const RecordingEntry&y)
{
    return x._programid < y._programid;
}

void Recording::UpdateBegin()
{
    std::cout << __FUNCTION__ << std::endl;

    _devices.clear();
    _diff = false;
}
bool Recording::UpdateEnd()
{
    std::cout << __FUNCTION__ << std::endl;

    // Look for now-missing items
    auto ri = _records.begin();
    while (ri != _records.end())
    {
        const auto& id = ri->first;
        const auto& i = _devices.find(id);
        if (i == _devices.end())
        {
            _diff = true;
            ri = _records.erase(ri);
        }
        else
            ++ ri;
    }
    return _diff;
}

void Recording::UpdateData(const Json::Value& json, const StorageDevice* device)
{
    for (const auto& j : json)
    {
        RecordingEntry entry(j);
        const auto& id = entry._programid;
        auto i = _records.find(id);
        if (i == _records.end())
        {
            _diff = true;

            auto id = entry._programid; // Copy _programid to allow a move for everything else.
            _records.emplace(id, std::move(entry));
        }
        else
        {
            if ((i->second) != entry)
            {
                _diff = true;
                i->second = entry;
            }
        }

        _devices[id].insert(device);
    }
}


} // namespace PVRHDHomeRun
