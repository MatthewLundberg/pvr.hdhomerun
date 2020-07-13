#pragma once
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

#include <string>
#include <cstdint>
#include <json/json.h>
#include <kodi/addon-instance/pvr/EPG.h>

namespace PVRHDHomeRun
{

template<typename T>
class Optional
{
    bool _has_value{false};
    T    _t{};
public:
    Optional() = default;
    Optional(const T& t)
        : _t{t}
        , _has_value{true}
    {}
    operator bool() const { return _has_value; }
    operator T()    const { return _t; }
};
class Entry
{
public:
    Entry(const Json::Value&);
    virtual ~Entry() = default;

    time_t _starttime       {0};
    time_t _endtime         {0};
    time_t _originalairdate {0};
    std::string _episodenumber;
    std::string _episodetitle;

    std::string _title;
    std::string _synopsis;
    std::string _imageURL;
    std::string _posterURL;
    std::string _seriesID;
    uint32_t    _genre {0};
    Optional<int> _season;
    Optional<int> _episode;

    virtual time_t StartTime() const;
    virtual time_t EndTime() const;
    virtual size_t Length() const;
};

bool operator==(const Entry&, const Entry&);
bool operator<(const Entry&, const Entry&);

}
