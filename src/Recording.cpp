/*
 *      Copyright (C) 2017-2020 Matthew Lundberg <matthew.k.lundberg@gmail.com>
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
#include "Settings.h"
#include "Filesystem.h"
//#include "IFileTypes.h"
#include <iostream>
#include <sstream>

namespace PVRHDHomeRun
{

RecordingEntry::RecordingEntry(const Json::Value& v)
: StringEntry(v)
{
    _category    = v["Category"].asString();
    _affiliate   = v["ChannelAffiliate"].asString();
    _channelimg  = v["ChannelImageURL"].asString();
    _channelname = v["ChannelName"].asString();
    _channelnum  = v["ChannelNumber"].asString();
    _programID   = v["ProgramID"].asString();
    _groupID     = v["DisplayGroupID"].asString();
    _grouptitle  = v["DisplayGroupTitle"].asString();
    _playurl     = v["PlayURL"].asString();
    _cmdurl      = v["CmdURL"].asString();
    _resume      = v["Resume"].asInt64();

    _recordstarttime = v["RecordStartTime"].asUInt64();
    _recordendtime   = v["RecordEndTime"].asUInt64();
}

kodi::addon::PVRRecording RecordingEntry::_pvr_recording() const
{
    kodi::addon::PVRRecording x;

    x.SetRecordingId(_programID);
    x.SetTitle(_title);
    x.SetEpisodeName(_episodetitle);
    x.SetSeriesNumber(_season);
    x.SetEpisodeNumber(_episode);
    x.SetPlot(_synopsis);
    x.SetChannelName(_channelnum + " " + _affiliate); // TODO - allow choice
    x.SetIconPath(_imageURL); // _channelimg
    x.SetDirectory(_grouptitle);
    x.SetRecordingTime(_starttime);
    x.SetDuration(_endtime - _starttime);

    return x;
}

time_t RecordingEntry::StartTime() const
{
    return _recordstarttime;
}

time_t RecordingEntry::EndTime() const
{
    return _recordendtime;
}

void RecordingEntry::Resume(int i)
{
    if (i < 0)
        return;

    _resume = i;
    std::stringstream url;
    url << _cmdurl << "&cmd=set&Resume=" << i;
    //std::cout << __FUNCTION__ << ' ' << i << ' ' << url.str() << std::endl;
    std::string result;
    GetFileContents(url.str(), result);
}

bool operator==(const RecordingEntry& a, const RecordingEntry& b)
{
    return static_cast<const Entry&>(a) == static_cast<const Entry&>(b) &&
            a._category     == b._category &&
            a._affiliate    == b._affiliate &&
            a._channelimg   == b._channelimg &&
            a._channelname  == b._channelname &&
            a._channelnum   == b._channelnum &&
            a._programID    == b._programID &&
            a._groupID      == b._groupID &&
            a._grouptitle   == b._grouptitle &&
            a._playurl      == b._playurl &&
            a._cmdurl       == b._cmdurl &&

            a._recordstarttime == b._recordstarttime &&
            a._recordendtime   == b._recordendtime;
}

bool operator<(const RecordingEntry& x, const RecordingEntry&y)
{
    return x._programID < y._programID;
}

bool operator==(const RecordingRule& a, const RecordingRule& b)
{
    return static_cast<const Entry&>(a) == static_cast<const Entry&>(b) &&
            a._recordingruleID == b._recordingruleID &&
            a._datetimeonly    == b._datetimeonly &&
            a._channelonly     == b._channelonly &&
            a._startpadding    == b._startpadding &&
            a._endpadding      == b._endpadding;
}

bool operator<(const RecordingRule& a, const RecordingRule& b)
{
    return a._recordingruleID < b._recordingruleID;
}

size_t Recording::size()
{
    return _records.size();
}

void Recording::UpdateBegin()
{
    _update_begin();
}

void Recording::UpdateEntry(const Json::Value& json)
{
    _update(_records, json);
}
bool Recording::UpdateEntryEnd()
{
    //std::cout << __FUNCTION__ << " #entries: " << _records.size() << std::endl;
    return _update_end(_records);
}

void Recording::UpdateRule(const Json::Value& json)
{
    _update(_rules, json);
}
bool Recording::UpdateRuleEnd()
{
    //std::cout << __FUNCTION__ << " #rules: " << _rules.size() << std::endl;
    return _update_end(_rules);
}

RecordingEntry* Recording::getEntry(const std::string& id)
{
    auto it = _records.find(id);
    if (it == _records.end())
    {
        KODI_LOG(ADDON_LOG_ERROR, "Cannot find recording ID: %s", id.c_str());
        return nullptr;
    }
    auto& rec = it->second;
    return &rec;
}

RecordingRule::RecordingRule(const Json::Value& v)
: StringEntry(v)
{
    _recordingruleID = v["RecordingRuleID"].asString();
    _datetimeonly    = v["DateTimeOnly"].asUInt64();
    _channelonly     = v["ChannelOnly"].asString();
    _startpadding    = v["StartPadding"].asInt();
    _endpadding      = v["EndPadding"].asInt();
}

} // namespace PVRHDHomeRun
