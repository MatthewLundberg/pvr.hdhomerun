/*
 *      Copyright (C) 2017-2019 Matthew Lundberg <matthew.k.lundberg@gmail.com>
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

#include "Guide.h"
#include "Settings.h"
#include "Utils.h"
#include "PVR_HDHR.h"

#include <iostream>

#include <kodi/addon-instance/pvr/EPG.h>

namespace PVRHDHomeRun
{

GuideNumber::GuideNumber(const Json::Value& v)
{
    _guidenumber = v["GuideNumber"].asString();
    _guidename   = v["GuideName"].asString();

     _channel = atoi(_guidenumber.c_str());
     auto dot = _guidenumber.find('.');
     if (std::string::npos != dot)
     {
         _subchannel = atoi(_guidenumber.c_str() + dot + 1);
     }
     else
     {
         _subchannel = 0;
     }
}

bool operator<(const GuideNumber& a, const GuideNumber& b)
{
    if (a._channel < b._channel)
    {
        return true;
    }
    else if (a._channel == b._channel)
    {
        if (a._subchannel < b._subchannel)
        {
            return true;
        }
    }

    return false;
}
bool operator==(const GuideNumber& a, const GuideNumber& b)
{
    bool ret = (a._channel    == b._channel)
            && (a._subchannel == b._subchannel)
            ;
    return ret;
}
std::string GuideNumber::toString() const
{
    char channel[64];
    if (_subchannel)
        sprintf(channel, "%d.%d", _channel, _subchannel);
    else
        sprintf(channel, "%d", _channel);

    return channel;
}
std::string GuideNumber::extendedName() const
{
    char channel[64];
    sprintf(channel, "%d.%d", _channel, _subchannel);
    return std::string("") + channel + " "
            + "_guidename("   + _guidename   + ") ";
}

GuideEntry::GuideEntry(const Json::Value& v)
: Entry(v)
{
}
bool operator<(const GuideEntry& a, const GuideEntry& b)
{
    return static_cast<const Entry&>(a) < static_cast<const Entry&>(b);
}
bool operator==(const GuideEntry& a, const GuideEntry& b)
{
    return static_cast<const Entry&>(a) == static_cast<const Entry&>(b)
            && a._id == b._id;
}

namespace {
std::string ParseAsW3CDateString(time_t time)
{
  std::tm* tm = std::gmtime(&time);
  char buffer[16];
  std::strftime(buffer, 16, "%Y-%m-%d", tm);

  return buffer;
}
}

kodi::addon::PVREPGTag GuideEntry::Epg_Tag(uint32_t number) const
{
    kodi::addon::PVREPGTag tag;

    tag.SetUniqueChannelId(number);

    tag.SetUniqueBroadcastId(_id);
    tag.SetTitle(_title.c_str());
    tag.SetEpisodeName(_episodetitle.c_str());
    tag.SetStartTime(_starttime);
    tag.SetEndTime(_endtime);
    tag.SetFirstAired(_originalairdate > 0 ? ParseAsW3CDateString(_originalairdate) : "");
    tag.SetPlot(_synopsis.c_str());
    tag.SetIconPath(_imageURL.c_str());
    tag.SetGenreType(_genre);
    tag.SetSeriesNumber(_season);
    tag.SetEpisodeNumber(_episode);

    return tag;
}

Guide::Guide(const Json::Value& v)
{
    _guidename = v["GuideName"].asString();
    _affiliate = v["Affiliate"].asString();
    _imageURL  = v["ImageURL"].asString();
}

bool Guide::AddEntry(PVR_HDHR* parent, GuideEntry& v, uint32_t number)
{
    Lock l(_lock);

    bool newentry = false;
    auto it = _entries.find(v);
    if (it == _entries.end())
    {
        v._id = _sequence.acquire();
        newentry = true;
    }
    else
    {
        v._id = it->_id;
    }

    Interval i(v);
    _times.Add(i);
    _requests.Remove(i);
    _entries.insert(v);

    EPG_EVENT_STATE state = newentry ? EPG_EVENT_CREATED : EPG_EVENT_UPDATED;
    auto tag = v.Epg_Tag(number);
    parent->EpgEventStateChange(tag, state);

    return newentry;
}

void Guide::_age_out(uint32_t number, time_t limit)
{
    Lock l(_lock);

    auto it = _entries.begin();
    while (it != _entries.end())
    {
        auto& entry = *it;
        time_t end = entry._endtime;
        if (end < limit)
        {
            _times.Remove(entry);
            it = _entries.erase(it);
            _sequence.release(entry._id);
        }
        else
            it ++;
    }
    _requests.Remove({0, limit});
}


} // namespace PVRHDHomeRUn
