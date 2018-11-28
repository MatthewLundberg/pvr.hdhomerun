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

#include "Device.h"
#include "Utils.h"
#include "Addon.h"
#include <json/json.h>

#include <iostream>

namespace PVRHDHomeRun
{
TunerDevice* New_TunerDevice(const hdhomerun_discover_device_t* d)
{
    auto t = new TunerDevice();
    t->Refresh(d);
    return t;
}
StorageDevice* New_StorageDevice(const hdhomerun_discover_device_t* d)
{
    auto t = new StorageDevice();
    t->Refresh(d);
    return t;
}

Tuner::Tuner(TunerDevice* box, unsigned int index)
    : _box(box)
    , _index(index)
	, _debug(nullptr) // _debug(hdhomerun_debug_create())
    , _device(hdhomerun_device_create(box->DeviceID(), box->IP(), index, _debug))
{
}

Tuner::~Tuner()
{
    hdhomerun_device_destroy(_device);
    //hdhomerun_debug_destroy(_debug);
}

void Device::Refresh(const hdhomerun_discover_device_t* d)
{
    if (d)
        _discover_device = *d;

    std::string discoverResults;
    std::string baseURL{BaseURL()};
    if (GetFileContents(baseURL + "/discover.json", discoverResults))
    {
        Json::Reader jsonReader;
        Json::Value  discoverJson;
        if (jsonReader.parse(discoverResults, discoverJson))
        {
            _parse_discover_data(discoverJson);
        }
    }
}
const char* Device::BaseURL()
{
    return _discover_device.base_url;
}

void Tuner::_get_var(std::string& value, const char* name)
{
    char *get_val;
    char *get_err;
    if (hdhomerun_device_get_var(_device, name, &get_val, &get_err) < 0)
    {
        KODI_LOG(LOG_ERROR,
                "communication error sending %s request to %08x-%u",
                name, _box->DeviceID(), _index
        );
    }
    else if (get_err)
    {
        KODI_LOG(LOG_ERROR, "error %s with %s request from %08x-%u",
                get_err, name, _box->DeviceID(), _index
        );
    }
    else
    {
        KODI_LOG(LOG_DEBUG, "Success getting value %s = %s from %08x-%u",
                name, get_val,
                _box->DeviceID(), _index
        );

        value.assign(get_val);
    }
}
void Tuner::_set_var(const char* value, const char* name)
{
    char* set_err;
    if (hdhomerun_device_set_var(_device, name, value, NULL, &set_err) < 0)
    {
        KODI_LOG(LOG_ERROR,
                "communication error sending set request %s = %s to %08x-%u",
                name, value,
                _box->DeviceID(), _index
        );
    }
    else if (set_err)
    {
        KODI_LOG(LOG_ERROR, "error %s with %s = %s request from %08x-%u",
                set_err,
                name, value,
                _box->DeviceID(), _index
        );
    }
    else
    {
        KODI_LOG(LOG_DEBUG, "Success setting value %s = %s from %08x",
                name, value,
                _box->DeviceID(), _index
        );
    }
}

void StorageDevice::_parse_discover_data(const Json::Value& json)
{
    try
    {
        _storageID  = json["StorageID"].asString();
        _storageURL = json["StorageURL"].asString();
        _freeSpace  = json["FreeSpace"].asUInt64();
    }
    catch (...)
    {
        KODI_LOG(LOG_INFO, "Exception caught parsing JSON from device");
    }
}

void TunerDevice::_parse_discover_data(const Json::Value& json)
{
    _lineupURL  = json["LineupURL"].asString();
    _tunercount = json["TunerCount"].asUInt();
    _legacy     = json["Legacy"].asBool();

    // We only need the device-level info for TCP
    if (g.Settings.protocol == SettingsType::UDP)
    {
        _tuners.clear();
        for (unsigned int index=0; index<_tunercount; index++)
        {
            _tuners.push_back(std::unique_ptr<Tuner>(new Tuner(this, index)));
        }
    }
    KODI_LOG(LOG_DEBUG, "HDR ID %08x LineupURL %s Tuner Count %d Legacy %d",
            _discover_device.device_id,
            _lineupURL.c_str(),
            _tunercount,
            _legacy
    );
}

uint32_t Device::LocalIP() const
{
    uint32_t tunerip = IP();

    const size_t max = 64;
    struct hdhomerun_local_ip_info_t ip_info[max];
    int ip_info_count = hdhomerun_local_ip_info(ip_info, max);

    for (int i=0; i<ip_info_count; i++)
    {
        auto& info = ip_info[i];
        uint32_t localip = info.ip_addr;
        uint32_t mask    = info.subnet_mask;

        if (IPSubnetMatch(localip, tunerip, mask))
        {
            return localip;
        }
    }

    return 0;
}

bool StorageDevice::UpdateRecord()
{
    std::string contents;
    if (GetFileContents(_storageURL, contents))
    {
        Json::Reader jsonReader;
        Json::Value  contentsJson;
        if (jsonReader.parse(contents, contentsJson))
        {
            return _parse_record_data(contentsJson);
        }
    }
    return false;
}

namespace {
template<typename A, typename B> bool map_equal_oneway(const std::map<A,B>& a, const std::map<A,B>& b)
{
    for (const auto& p: a)
    {
        const auto& id = p.first;
        const auto& op = b.find(id);
        if (op == b.end())
            return false;
        const auto& aa = p.second;
        const auto& bb = op->second;
        if (aa != bb)
            return false;
    }
    return true;
}
template<typename A, typename B> bool operator==(const std::map<A,B>& a, const std::map<A,B>& b)
{
    return map_equal_oneway(a,b) && map_equal_oneway(b,a);
}
} // namespace

bool StorageDevice::_parse_record_data(const Json::Value& json)
{
    std::map<std::string, RecordingEntry> recordings;

    std::cout << __FUNCTION__ << std::endl;
    for (const auto& j : json)
    {
        RecordingEntry entry(j);
        auto id = entry._programid; // Copy _programid to allow a move for everything else.
        recordings.emplace(id, std::move(entry));
    }
    bool equal = (recordings == _records);
    _records = std::move(recordings);

    return !equal;
}

} // namespace PVRHDHomeRun
