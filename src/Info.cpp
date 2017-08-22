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
#include "client.h"
#include <sstream>
#include <cstdlib>

namespace PVRHDHomeRun
{

Info::Info(const Json::Value& v)
{
    _guidename = v["GuideName"].asString();
    _drm       = v["DRM"].asBool();
    _hd        = v["HD"].asBool();
}

Device* Info::GetPreferredDevice()
{
    for (const auto id: g.Settings.preferredDevice)
    {
        for (const auto device: _devices)
        {
            if (id == device->DeviceID())
                return device;
        }
    }
    return nullptr;
}

Device* Info::GetNextDevice()
{
    if (_has_next)
    {
        _next ++;
        if (_next == _devices.end())
        {
            _has_next = false;
            return nullptr;
        }
    }
    else
    {
        _has_next = true;
        _next = _devices.begin();
    }
    return *_next;
}

void Info::ResetNextDevice()
{
    _has_next = false;
}

bool Info::AddDevice(Device* t, const std::string& url)
{
    if (HasDevice(t))
    {
        return false;
    }
    _devices.insert(t);
    _url[t] = url;
    ResetNextDevice();

    return true;
}

bool Info::RemoveDevice(Device* t)
{
    if (!HasDevice(t))
    {
        return false;
    }
    _devices.erase(t);
    ResetNextDevice();

    return true;
}

std::string Info::DeviceListString() const
{
    std::string devices;
    for (auto device : _devices)
    {
        char id[10];
        sprintf(id, " %08x", device->DeviceID());
        devices += id;
    }

    return devices;
}


} // namespace PVRHDHomeRun
