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


Entry::Entry(const Json::Value& v, bool recording)
{
    if (recording)
    {
        _starttime  = v["RecordStartTime"].asUInt64();
        _endtime    = v["RecordEndTime"].asUInt64();
    }
    else
    {
        _starttime       = v["StartTime"].asUInt();
        _endtime         = v["EndTime"].asUInt();
    }
    _originalairdate = v["OriginalAirdate"].asUInt64();
    _episodenumber   = v["EpisodeNumber"].asString();
    _episodetitle    = v["EpisodeTitle"].asString();


    if (_episodenumber[0] == 'S') {
        auto e = _episodenumber.find('E');
        if (e != std::string::npos) {
            auto season  = _episodenumber.substr(1, e);
            auto episode = _episodenumber.substr(e+1);
            _season  = std::stoi(season);
            _episode = std::stoi(episode);
        }
    }
}

bool operator==(const Entry& a, const Entry& b)
{
    return a._starttime == b._starttime &&
            a._endtime == b._endtime &&
            a._originalairdate == b._originalairdate &&
            a._episodenumber == b._episodenumber &&
            a._episodetitle == b._episodetitle;
}
bool operator<(const Entry& a, const Entry& b)
{
    return a._starttime < b._starttime;
}

}