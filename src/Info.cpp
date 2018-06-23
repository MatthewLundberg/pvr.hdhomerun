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

#include "Info.h"
#include "Device.h"
#include "client.h"
#include <sstream>
#include <cstdlib>

namespace PVRHDHomeRun
{

Info::Info(const Json::Value& v)
{
    _guidenumber = v["GuideNumber"].asString();
    _guidename   = v["GuideName"].asString();
    _drm         = v["DRM"].asBool();
    _hd          = v["HD"].asBool();
}

bool Info::AddDevice(TunerDevice* t, const std::string& url)
{
    if (HasDevice(t))
    {
        return false;
    }
    _tuner_devices.insert(t);
    _url[t] = url;
    return true;
}

bool Info::RemoveDevice(TunerDevice* t)
{
    if (!HasDevice(t))
    {
        return false;
    }
    _tuner_devices.erase(t);
    return true;
}

} // namespace PVRHDHomeRun
