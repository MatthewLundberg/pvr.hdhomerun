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

#include "Info.h"
#include "Tuner.h"

namespace PVRHDHomeRun
{

Info::Info(const Json::Value& v)
{
    _guidename = v["GuideName"].asString();
    _drm       = v["DRM"].asBool();
    _hd        = v["HD"].asBool();

     //KODI_LOG(LOG_DEBUG, "LineupEntry::LineupEntry %s", extendedName().c_str());
}

Tuner* Info::GetPreferredTuner()
{
    return nullptr;
}

Tuner* Info::GetNextTuner()
{
    if (_has_next)
    {
        _next ++;
        if (_next == _tuners.end())
        {
            _has_next = false;
            return nullptr;
        }
    }
    else
    {
        _has_next = true;
        _next = _tuners.begin();
    }
    return *_next;
}

void Info::ResetNextTuner()
{
    _has_next = false;
}

bool Info::AddTuner(Tuner* t, const std::string& url)
{
    if (HasTuner(t))
    {
        return false;
    }
    _tuners.insert(t);
    _url[t] = url;
    ResetNextTuner();

    return true;
}

bool Info::RemoveTuner(Tuner* t)
{
    if (!HasTuner(t))
    {
        return false;
    }
    _tuners.erase(t);
    ResetNextTuner();

    return true;
}

std::string Info::TunerListString() const
{
    std::string tuners;
    for (auto tuner : _tuners)
    {
        char id[10];
        sprintf(id, " %08x", tuner->DeviceID());
        tuners += id;
    }

    return tuners;
}


} // namespace PVRHDHomeRun
