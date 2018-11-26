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

class RecordingEntry
{
    RecordingEntry(const Json::Value&);

    std::string _category;
    std::string _affiliate;
    std::string _channelimg;
    std::string _channelname;
    std::string _channelnum;
    std::string _episodenum;
    std::string _episodetitle;
    std::string _image;
    std::string _programid;
    std::string _seriesid;
    std::string _synposis;
    std::string _title;
    std::string _groupid;
    std::string _grouptitle;
    std::string _playurl;
    std::string _cmdurl;

    time_t _aired;
    time_t _rstarttime;
    time_t _rendtime;
    time_t _starttime;
    time_t _endtime;

    friend bool operator<(const RecordingEntry&, const RecordingEntry&);
};

bool operator<(const RecordingEntry& x, const RecordingEntry&y);
