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

#include "Guide.h"
#include "Addon.h"
#include "Utils.h"

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

EPG_TAG GuideEntry::Epg_Tag(uint32_t number) const
{
    EPG_TAG tag = {0};

    tag.iUniqueChannelId   = number;

    tag.iUniqueBroadcastId = _id;
    tag.strTitle           = _title.c_str();
    tag.strEpisodeName     = _episodetitle.c_str();
    tag.startTime          = _starttime;
    tag.endTime            = _endtime;
    tag.firstAired         = _originalairdate;
    tag.strPlot            = _synopsis.c_str();
    tag.strIconPath        = _imageURL.c_str();
    tag.iGenreType         = _genre;
    tag.iSeriesNumber      = _season;
    tag.iEpisodeNumber     = _episode;

    return tag;
}

Guide::Guide(const Json::Value& v)
{
    _guidename = v["GuideName"].asString();
    _affiliate = v["Affiliate"].asString();
    _imageURL  = v["ImageURL"].asString();
}

bool Guide::AddEntry(GuideEntry& v, uint32_t number)
{
    bool newentry = false;
    auto it = _entries.find(v);
    if (it == _entries.end())
    {
        v._id = _nextidx ++;
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
    EPG_TAG tag = v.Epg_Tag(number);
    g.PVR->EpgEventStateChange(&tag, state);

    return newentry;
}

void Guide::_age_out(uint32_t number, time_t limit)
{
    auto it = _entries.begin();
    while (it != _entries.end())
    {
        auto& entry = *it;
        time_t end = entry._endtime;
        if (end < limit)
        {
            _times.Remove(entry);
            it = _entries.erase(it);
        }
        else
            it ++;
    }
    _requests.Remove({0, limit});
}


} // namespace PVRHDHomeRUn
