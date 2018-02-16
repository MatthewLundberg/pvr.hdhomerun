#pragma once
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

#include <json/json.h>
#include <set>
#include "Tuner.h"

namespace PVRHDHomeRun
{

class Device;

class Info : public DeviceSet // Device pointers are owned by PVR_HDHR
{
public:
    Info(const Json::Value&);
    Info() = default;

    bool AddDevice(Device*, const std::string& url);
    bool RemoveDevice(Device*);
    bool HasDevice(Device* t) const
    {
        return _devices.find(t) != _devices.end();
    }
    std::string DlnaURL(Device* t) const
    {
        auto it = _url.find(t);
        if (it != _url.end())
            return it->second;
        return "";
    }

    std::string _guidename;
    bool        _hd       = false;
    bool        _drm      = false;
    bool        _favorite = false;
private:
    std::map<Device*, std::string> _url;
};

} // namespace PVRHDHomeRun
