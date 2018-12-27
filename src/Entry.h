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

#include <string>
#include <cstdint>
#include <json/json.h>
#include <xbmc_epg_types.h>

namespace PVRHDHomeRun
{

class Entry
{
public:
    Entry(const Json::Value&);

    time_t _starttime       = 0;
    time_t _endtime         = 0;
    time_t _originalairdate = 0;
    int         _season     = 0;
    int         _episode    = 0;
    std::string _episodenumber;
    std::string _episodetitle;

    std::string _title;
    std::string _synopsis;
    std::string _imageURL;

};

bool operator==(const Entry&, const Entry&);
bool operator<(const Entry&, const Entry&);

class ChannelNumber
{
private:
    static const uint32_t SubchannelLimit = 10000;
public:
    ChannelNumber(const Json::Value&);
    ChannelNumber(const ChannelNumber&) = default;
    ChannelNumber(uint32_t id)
    {
        _channel = id / SubchannelLimit;
         id %= SubchannelLimit;
         _subchannel = id;
    }
    virtual ~ChannelNumber() = default;

    std::string _channelnumber;
    std::string _channelname;

    uint32_t _channel;
    uint32_t _subchannel;

    std::string extendedName() const;
    std::string toString() const;

    uint32_t ID() const
    {
        // _subchannel < 1000, _nameidx < 100
        return (_channel * SubchannelLimit) + _subchannel;
    }
    operator uint32_t() const
    {
        return ID();
    }
};

bool operator<(const ChannelNumber&, const ChannelNumber&);
bool operator==(const ChannelNumber&, const ChannelNumber&);

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


}
