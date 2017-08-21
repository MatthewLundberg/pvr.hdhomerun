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

#include "Tuner.h"
#include "Utils.h"
#include "client.h"
#include <json/json.h>

namespace PVRHDHomeRun
{

Tuner::Tuner(const hdhomerun_discover_device_t& d, unsigned int tuner)
    : _debug(nullptr) // _debug(hdhomerun_debug_create())
    , _device(hdhomerun_device_create(d.device_id, d.ip_addr, tuner, _debug))
    , _discover_device(d) // copy
{
    _get_api_data();
    _get_discover_data();
}

Tuner::~Tuner()
{
    hdhomerun_device_destroy(_device);
    //hdhomerun_debug_destroy(_debug);
}


void Tuner::_get_var(std::string& value, const char* name)
{
    char *get_val;
    char *get_err;
    if (hdhomerun_device_get_var(_device, name, &get_val, &get_err) < 0)
    {
        KODI_LOG(LOG_ERROR,
                "communication error sending %s request to %08x",
                name, _discover_device.device_id
        );
    }
    else if (get_err)
    {
        KODI_LOG(LOG_ERROR, "error %s with %s request from %08x",
                get_err, name, _discover_device.device_id
        );
    }
    else
    {
        KODI_LOG(LOG_DEBUG, "Success getting value %s = %s from %08x",
                name, get_val,
                _discover_device.device_id
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
                "communication error sending set request %s = %s to %08x",
                name, value,
                _discover_device.device_id
        );
    }
    else if (set_err)
    {
        KODI_LOG(LOG_ERROR, "error %s with %s = %s request from %08x",
                set_err,
                name, value,
                _discover_device.device_id
        );
    }
    else
    {
        KODI_LOG(LOG_DEBUG, "Success setting value %s = %s from %08x",
                name, value,
                _discover_device.device_id
        );
    }
}
void Tuner::_get_api_data()
{
}

void Tuner::_get_discover_data()
{
    std::string discoverResults;

    // Ask the device for its lineup URL
    std::string discoverURL{ _discover_device.base_url };
    discoverURL.append("/discover.json");

    if (GetFileContents(discoverURL, discoverResults))
    {
        Json::Reader jsonReader;
        Json::Value discoverJson;
        if (jsonReader.parse(discoverResults, discoverJson))
        {
            auto& lineupURL  = discoverJson["LineupURL"];
            auto& tunercount = discoverJson["TunerCount"];
            auto& legacy     = discoverJson["Legacy"];

            _lineupURL  = std::move(lineupURL.asString());
            _tunercount = std::move(tunercount.asUInt());
            _legacy     = std::move(legacy.asBool());

            KODI_LOG(LOG_DEBUG, "HDR ID %08x LineupURL %s Tuner Count %d Legacy %d",
                    _discover_device.device_id,
                    _lineupURL.c_str(),
                    _tunercount,
                    _legacy
            );
        }
    }
    else
    {
        // Fall back to a pattern for "modern" devices
        KODI_LOG(LOG_DEBUG, "HDR ID %08x Fallback lineup URL %s/lineup.json",
                _discover_device.device_id,
                _discover_device.base_url
        );

        _lineupURL.assign(_discover_device.base_url);
        _lineupURL.append("/lineup.json");
    }
}

void Tuner::Refresh(const hdhomerun_discover_device_t& d)
{
    _discover_device = d;
    _get_discover_data();
}

uint32_t Tuner::LocalIP() const
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

} // namespace PVRHDHomeRun
