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

#include <hdhomerun.h>
#include <string>
#include <memory>
#include <vector>
#include <set>

namespace PVRHDHomeRun
{

class Tuner;

// The tuner box has an ID, lineup, guide, and one or more tuners.
class Device
{
public:
    Device(const hdhomerun_discover_device_t& d);
    ~Device();

    void Refresh(const hdhomerun_discover_device_t& d);

    unsigned int TunerCount() const
    {
        return _tunercount;
    }
    bool Legacy() const
    {
        return _legacy;
    }
    const uint32_t DeviceID() const
    {
        return _discover_device.device_id;
    }
    const char* Auth() const
    {
        return _discover_device.device_auth;
    }
    uint32_t IP() const
    {
        return _discover_device.ip_addr;
    }
    uint32_t LocalIP() const;
    const std::string& LineupURL()
    {
        return _lineupURL;
    }

private:
    // Called once
    void _get_api_data();
    // Called multiple times for legacy devices
    void _get_discover_data();

    hdhomerun_discover_device_t _discover_device;

    // Discover Data
    std::string                 _lineupURL;
    unsigned int                _tunercount;
    bool                        _legacy;

    std::vector<std::unique_ptr<Tuner>> _tuners;
public:
    bool operator<(const Device& rhs) const
    {
        return DeviceID() < rhs.DeviceID();
    }
    bool operator==(const Device& rhs) const
    {
    	return DeviceID() == rhs.DeviceID();
    }
    friend class Lineup;
    friend class Tuner;
};

class DeviceSet
{
public:
    size_t DeviceCount() const
    {
        return _devices.size();
    }
    std::string IDString() const;
    std::string AuthString() const;
protected:

    std::set<Device*> _devices;
};

class Tuner
{
public:
    Tuner(Device* box, unsigned int index=0);
    Tuner(const Tuner&) = delete;
    Tuner(Tuner&&) = default;
    ~Tuner();

    //hdhomerun_device_t* Device()
    //{
    //    return _device;
    //}

    std::string GetVar(const std::string& name)
    {
        std::string retval;
        _get_var(retval, name.c_str());
        return retval;
    }
    void SetVar(const std::string& name, const std::string& value)
    {
        _set_var(value.c_str(), name.c_str());
    }
    Device* GetDevice() const
    {
        return _box;
    }

private:
    void _get_var(std::string& value, const char* name);
    void _set_var(const char*value, const char* name);

    // The hdhomerun_... objects depend on the order listed here for proper instantiation.
    Device*             _box;
    unsigned int        _index;
    hdhomerun_debug_t*  _debug;
    hdhomerun_device_t* _device;

    friend class TunerLock;
};

class TunerLock
{
public:
    TunerLock(Tuner* t)
    : _tuner(t)
    {
        char* ret_error;
        if (hdhomerun_device_tuner_lockkey_request(_tuner->_device, &ret_error) > 0)
        {
            _success = true;
        }
    }
    ~TunerLock()
    {
        if (_success)
        {
            hdhomerun_device_tuner_lockkey_release(_tuner->_device);
        }
    }
    bool Success() const
    {
        return _success;
    }
private:
    bool   _success = false;
    Tuner* _tuner;
};


} // namespace PVRHDHomeRun
