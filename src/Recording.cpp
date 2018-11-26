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

bool operator<(const RecordingEntry& x, const RecordingEntry&y)
{
    return x._programid < y._programid;
}
