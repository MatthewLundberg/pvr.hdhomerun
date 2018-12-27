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
#include "Utils.h"
#include <iostream>

namespace PVRHDHomeRun
{

RecordingEntry::RecordingEntry(const Json::Value& v)
: Entry(v)
{
    _category    = v["Category"].asString();
    _affiliate   = v["ChannelAffiliate"].asString();
    _channelimg  = v["ChannelImageURL"].asString();
    _channelname = v["ChannelName"].asString();
    _channelnum  = v["ChannelNumber"].asString();
    _poster      = v["PosterURL"].asString();
    _programid   = v["ProgramID"].asString();
    _seriesid    = v["SeriesID"].asString();
    _groupid     = v["DisplayGroupID"].asString();
    _grouptitle  = v["DisplayGroupTitle"].asString();
    _playurl     = v["PlayURL"].asString();
    _cmdurl      = v["CmdURL"].asString();

    _recordstarttime = v["RecordStartTime"].asUInt64();
    _recordendtime   = v["RecordEndTime"].asUInt64();
}

namespace {
template<size_t N>
void scp(char (&x)[N], const std::string& s)
{
    strcpy(x, s.substr(0,N-1).c_str());
}
}

time_t RecordingEntry::rstarttime() const
{
    return (static_cast<const Entry*>(this))->_starttime;
}
time_t RecordingEntry::rendtime() const
{
    return (static_cast<const Entry*>(this))->_endtime;
}

PVR_RECORDING RecordingEntry::_pvr_recording() const
{
    PVR_RECORDING x = {0};

    pvr_strcpy(x.strRecordingId, _programid);
    pvr_strcpy(x.strTitle,       _title);
    pvr_strcpy(x.strEpisodeName, _episodetitle);
    x.iSeriesNumber = _season;
    x.iEpisodeNumber = _episode;
    pvr_strcpy(x.strPlot,        _synopsis);
    pvr_strcpy(x.strChannelName, _channelname + " " + _channelnum);
    pvr_strcpy(x.strIconPath,    _imageURL); // _channelimg
    pvr_strcpy(x.strDirectory,   _grouptitle);
    x.recordingTime = _starttime;
    x.iDuration = _endtime - _starttime;

    return x;
}

bool operator==(const RecordingEntry& a, const RecordingEntry& b)
{
    return static_cast<const Entry&>(a) == static_cast<const Entry&>(b) &&
            a._category     == b._category &&
            a._affiliate    == b._affiliate &&
            a._channelimg   == b._channelimg &&
            a._channelname  == b._channelname &&
            a._channelnum   == b._channelnum &&
            a._poster       == b._poster &&
            a._programid    == b._programid &&
            a._seriesid     == b._seriesid &&
            a._groupid      == b._groupid &&
            a._grouptitle   == b._grouptitle &&
            a._playurl      == b._playurl &&
            a._cmdurl       == b._cmdurl &&

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

size_t Recording::size()
{
    return _records.size();
}

void Recording::UpdateBegin()
{
    _devices.clear();
    _diff = false;
}
bool Recording::UpdateEnd()
{
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
        _devices[id].insert(device);

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
    }
}


} // namespace PVRHDHomeRun
