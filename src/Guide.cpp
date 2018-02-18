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
#include "client.h"
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

bool GuideNumber::operator<(const GuideNumber& rhs) const
{
    if (_channel < rhs._channel)
    {
        return true;
    }
    else if (_channel == rhs._channel)
    {
        if (_subchannel < rhs._subchannel)
        {
            return true;
        }
    }

    return false;
}
bool GuideNumber::operator==(const GuideNumber& rhs) const
{
    bool ret = (_channel == rhs._channel)
            && (_subchannel == rhs._subchannel)
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

template<typename T>
unsigned int GetGenreType(const T& arr)
{
    unsigned int nGenreType = 0;

    for (auto& i : arr)
    {
        auto str = i.asString();

        if (str == "News")
            nGenreType |= EPG_EVENT_CONTENTMASK_NEWSCURRENTAFFAIRS;
        else if (str == "Comedy")
            nGenreType |= EPG_EVENT_CONTENTMASK_SHOW;
        else if (str == "Movie" || str == "Drama")
            nGenreType |= EPG_EVENT_CONTENTMASK_MOVIEDRAMA;
        else if (str == "Food")
            nGenreType |= EPG_EVENT_CONTENTMASK_LEISUREHOBBIES;
        else if (str == "Talk Show")
            nGenreType |= EPG_EVENT_CONTENTMASK_SHOW;
        else if (str == "Game Show")
            nGenreType |= EPG_EVENT_CONTENTMASK_SHOW;
        else if (str == "Sport" || str == "Sports")
            nGenreType |= EPG_EVENT_CONTENTMASK_SPORTS;
    }

    return nGenreType;
}

GuideEntry::GuideEntry(const Json::Value& v)
{
    _starttime       = v["StartTime"].asUInt();
    _endtime         = v["EndTime"].asUInt();
    _originalairdate = v["OriginalAirdate"].asUInt();

    _episodenumber   = v["EpisodeNumber"].asString();
    _episodetitle    = v["EpisodeTitle"].asString();
    _synopsis        = v["Synopsis"].asString();
    _imageURL        = v["ImageURL"].asString();
    _seriesID        = v["SeriesID"].asString();
    _genre           = GetGenreType(v["Filter"]);
    bool recording = v.isMember("RecordingRule") && v["RecordingRule"].asBool();
    if (recording)
    {
        _title = std::string("* ") + v["Title"].asString();
    }
    else
    {
        _title = v["Title"].asString();
    }

    // TODO remove
    _synopsis = _seriesID + " " + _synopsis;
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

    if (_episodenumber[0] == 'S') {
        auto e = _episodenumber.find('E');
        if (e != std::string::npos) {
            auto season  = _episodenumber.substr(1, e);
            auto episode = _episodenumber.substr(e+1);
            tag.iSeriesNumber  = std::stoi(season);
            tag.iEpisodeNumber = std::stoi(episode);
        }
    }

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
