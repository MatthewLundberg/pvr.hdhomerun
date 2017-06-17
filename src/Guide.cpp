/*
 *      Copyright (C) 2017 Matthew Lundberg <matthew.k.lundberg@gmail.com>
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
    _title           = v["Title"].asString();
    _episodenumber   = v["EpisodeNumber"].asString();
    _episodetitle    = v["EpisodeTitle"].asString();
    _synopsis        = v["Synopsis"].asString();
    _imageURL        = v["ImageURL"].asString();
    _seriesID        = v["SeriesID"].asString();
    _genre           = GetGenreType(v["Filter"]);
}


EPG_TAG GuideEntry::Epg_Tag(uint32_t number) const
{
    EPG_TAG tag = {0};

    tag.iChannelNumber     = number;

    tag.iUniqueBroadcastId = _id;
    tag.strTitle           = _title.c_str();
    tag.strEpisodeName     = _episodetitle.c_str();
    tag.startTime          = _starttime;
    tag.endTime            = _endtime;
    tag.firstAired         = _originalairdate;
    tag.strPlot            = _synopsis.c_str();
    tag.strIconPath        = _imageURL.c_str();
    tag.iGenreType         = _genre;

    // SD doesn't provide integers for these, so we ignore them for now.
    //tag.iSeriesNumber
    //tag.iEpisodeNumber

    return tag;
}

Guide::Guide(const Json::Value& v)
{
    _guidename = v["GuideName"].asString();
    _affiliate = v["Affiliate"].asString();
    _imageURL  = v["ImageURL"].asString();
}

GuideEntryStatus Guide::InsertEntry(const GuideEntry& v)
{
    auto ins = _entries.insert(v);
    auto& entry = const_cast<GuideEntry&>(*(ins.first));
    if (ins.second) {
        std::cout << "New entry, index " << _nextidx << "\n";
        entry._id = _nextidx ++;

        _times.Add(entry);
    }

    return {ins.second, entry};
}

bool Guide::_age_out(uint32_t number)
{
    bool changed = false;

    uint32_t max_age = g.Settings.guideAgeOut;
    time_t   now = time(nullptr);

    for (auto& entry : _entries)
    {
        time_t end = entry._endtime;
        if ((now > end) && ((now - end) > max_age))
        {
            KODI_LOG(LOG_DEBUG, "Deleting guide entry for age %u: %s - %s", (now-end), entry._title.c_str(), entry._episodetitle.c_str());

            _times.Remove(entry);
            _requests.Remove(entry);

            _entries.erase(entry);
            changed = true;
        }
    }

    return changed;
}


} // namespace PVRHDHomeRUn
