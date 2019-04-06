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

#include "Entry.h"

namespace PVRHDHomeRun
{


Entry::Entry(const Json::Value& v)
{
    _starttime       = v["StartTime"].asUInt64();
    _endtime         = v["EndTime"].asUInt64();
    _originalairdate = v["OriginalAirdate"].asUInt64();
    _episodenumber   = v["EpisodeNumber"].asString();
    _episodetitle    = v["EpisodeTitle"].asString();

    _synopsis        = v["Synopsis"].asString();
    _imageURL        = v["ImageURL"].asString();
    _posterURL       = v["PosterURL"].asString();
    _seriesID        = v["SeriesID"].asString();

    // Remove this hack once the timer rules are sent to Kodi.
    bool recording = v.isMember("RecordingRule") && v["RecordingRule"].asBool();
    if (recording)
    {
        _title = std::string("* ") + v["Title"].asString();
    }
    else
    {
        _title = v["Title"].asString();
    }

    if (_episodenumber[0] == 'S') {
        auto e = _episodenumber.find('E');
        if (e != std::string::npos) {
            auto season  = _episodenumber.substr(1, e);
            auto episode = _episodenumber.substr(e+1);
            _season  = std::stoi(season);
            _episode = std::stoi(episode);
        }
    }

    _genre = GetGenreType(v["Filter"]);
}

time_t Entry::StartTime() const
{
    return _starttime;
}

time_t Entry::EndTime() const
{
    return _endtime;
}

size_t Entry::Length() const
{
    return 0;
}

bool operator==(const Entry& a, const Entry& b)
{
    return a._starttime        == b._starttime &&
            a._endtime         == b._endtime &&
            a._originalairdate == b._originalairdate &&
            a._imageURL        == b._imageURL &&
            a._posterURL       == b._posterURL &&
            a._seriesID        == b._seriesID &&
            a._synopsis        == b._synopsis &&
            a._title           == b._title &&
            a._episodenumber   == b._episodenumber &&
            a._episodetitle    == b._episodetitle;
}
bool operator<(const Entry& a, const Entry& b)
{
    return a._starttime < b._starttime;
}

}
