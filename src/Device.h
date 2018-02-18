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

#include <hdhomerun.h>
#include <string>
#include <memory>
#include <vector>
#include <set>
#include <json/json.h>
#include <sstream>
#include <ios>
#include <iterator>
#include <set>
#include <algorithm>

namespace PVRHDHomeRun
{

class Device
{
public:
    virtual ~Device() = default;

    void Refresh(const hdhomerun_discover_device_t* d = nullptr);
    const char* BaseURL();
    uint32_t LocalIP() const;
    uint32_t IP() const
    {
        return _discover_device.ip_addr;
    }

protected:
    hdhomerun_discover_device_t _discover_device;
private:
    virtual void _parse_discover_data(const Json::Value&) = 0;
public:
    friend class Lineup;
};

class StorageDevice : public Device
{
private:
    void _parse_discover_data(const Json::Value&) override;

    std::string _storageID;
    std::string _storageURL;
    uint64_t    _freeSpace;
public:
    bool operator<(const StorageDevice& rhs)
    {
        return _storageID < rhs._storageID;
    }
    bool operator==(const StorageDevice& rhs)
    {
        return _storageID == rhs._storageID;
    }
};

// The tuner box has an ID, lineup, guide, and one or more tuners.
class Tuner;
class TunerDevice : public Device
{
public:
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
    const std::string& LineupURL()
    {
        return _lineupURL;
    }

private:
    void _parse_discover_data(const Json::Value&) override;


    // Discover Data
    std::string                 _lineupURL;
    unsigned int                _tunercount;
    bool                        _legacy;

    std::vector<std::unique_ptr<Tuner>> _tuners;
public:
    bool operator<(const TunerDevice& rhs) const
    {
        return DeviceID() < rhs.DeviceID();
    }
    bool operator==(const TunerDevice& rhs) const
    {
    	return DeviceID() == rhs.DeviceID();
    }
    friend class Tuner;
};

TunerDevice* New_TunerDevice(const hdhomerun_discover_device_t* d);

template<typename T>
class HasTunerSet
{
public:
    std::string IDString() const
    {
        auto t = static_cast<const T*>(this);
        std::stringstream devices;
        devices << std::hex;
        std::transform(t->_tuner_devices.begin(), t->_tuner_devices.end(),
                std::ostream_iterator<uint32_t>(devices, " "),
                [](const TunerDevice* t) -> uint32_t {return t->DeviceID();});
        return devices.str();
    }
    std::string AuthString() const
    {
        auto t = static_cast<const T*>(this);
        std::stringstream auth;
        std::transform(t->_tuner_devices.begin(), t->_tuner_devices.end(),
                std::ostream_iterator<std::string>(auth),
                [](const TunerDevice* t) -> std::string {return t->Auth();});
        return auth.str();
    }
    size_t DeviceCount() const
    {
        auto t = static_cast<const T*>(this);
        return t->_tuner_devices.size();
    }
    std::set<TunerDevice*>::iterator begin()
    {
        auto t = static_cast<T*>(this);
        return t->_tuner_devices.begin();
    }
    std::set<TunerDevice*>::iterator end()
    {
        auto t = static_cast<T*>(this);
        return t->_tuner_devices.end();
    }
};

class Tuner
{
public:
    Tuner(TunerDevice* box, unsigned int index=0);
    Tuner(const Tuner&) = delete;
    Tuner(Tuner&&) = default;
    ~Tuner();

    //hdhomerun_device_t* TunerDevice()
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
    TunerDevice* GetDevice() const
    {
        return _box;
    }

private:
    void _get_var(std::string& value, const char* name);
    void _set_var(const char*value, const char* name);

    // The hdhomerun_... objects depend on the order listed here for proper instantiation.
    TunerDevice*             _box;
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
