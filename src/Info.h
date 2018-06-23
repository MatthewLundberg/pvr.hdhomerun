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

#include <json/json.h>
#include <set>
#include "Device.h"

namespace PVRHDHomeRun
{

class Device;

class Info : public HasTunerSet<Info>
{
public:
    Info(const Json::Value&);
    Info() = default;

    bool AddDevice(TunerDevice*, const std::string& url);
    bool RemoveDevice(TunerDevice*);
    bool HasDevice(TunerDevice* t) const
    {
        return _tuner_devices.find(t) != _tuner_devices.end();
    }
    std::string DlnaURL(TunerDevice* t) const
    {
        auto it = _url.find(t);
        if (it != _url.end())
            return it->second;
        return "";
    }

    std::string _guidenumber;
    std::string _guidename;
    bool        _hd       = false;
    bool        _drm      = false;
    bool        _favorite = false;
private:
    std::map<Device*, std::string> _url;
public:
    // Device pointers are owned by PVR_HDHR
    std::set<TunerDevice*>         _tuner_devices;
};

} // namespace PVRHDHomeRun
