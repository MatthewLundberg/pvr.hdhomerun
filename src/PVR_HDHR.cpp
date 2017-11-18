/*
 *      Copyright (C) 2017 Matthew Lundberg <matthew.k.lundberg@gmail.com>
 *      https://github.com/MatthewLundberg/pvr.hdhomerun
 *
 *      Copyright (C) 2011 Pulse-Eight
 *      http://www.pulse-eight.com/
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

#include "client.h"
#include "Utils.h"
#include "PVR_HDHR.h"
#include <chrono>
#include <thread>
#include <functional>
#include <algorithm>
#include <iterator>
#include <string>
#include <sstream>
#include <iterator>
#include <numeric>

namespace PVRHDHomeRun
{
PVR_HDHR* PVR_HDHR_Factory(int protocol) {
    switch (protocol)
    {
    case SettingsType::TCP:
        return new PVR_HDHR_TCP();
    case SettingsType::UDP:
        return new PVR_HDHR_UDP();
    }
    return nullptr;
}

bool PVR_HDHR::DiscoverDevices()
{
    struct hdhomerun_discover_device_t discover_devices[64];
    size_t device_count = hdhomerun_discover_find_devices_custom_v2(
            0,
            HDHOMERUN_DEVICE_TYPE_TUNER,
            HDHOMERUN_DEVICE_ID_WILDCARD,
            discover_devices,
            64
            );

    KODI_LOG(LOG_DEBUG, "PVR_HDHR::DiscoverDevices Found %d devices", device_count);

    if (device_count == 0)
    {
        // Sometimes no devices are found when waking from sleep, causing a
        // lot of unnecessary network traffic as they are rediscovered.
        // TODO:  Handle the case where all devices go away?
        return true;
    }

    std::set<uint32_t> discovered_ids;

    bool device_added   = false;
    bool device_removed = false;

    Lock guidelock(_guide_lock);
    Lock lock(this);
    for (size_t i=0; i<device_count; i++)
    {
        auto& dd = discover_devices[i];
        auto  id = dd.device_id;

        if (g.Settings.blacklistDevice.find(id) != g.Settings.blacklistDevice.end())
        {
        	KODI_LOG(LOG_INFO, "Ignoring blacklisted device %08x", id);
        	continue;
        }

        if (dd.is_legacy && !g.Settings.useLegacyDevices)
        {
        	KODI_LOG(LOG_INFO, "Ignoring legacy device %08x", id);
            continue;
        }

        discovered_ids.insert(id);

        if (_device_ids.find(id) == _device_ids.end())
        {
            // New device
            device_added = true;
            KODI_LOG(LOG_DEBUG, "Adding device %08x", id);

            _devices.insert(new Device(dd));
            _device_ids.insert(id);
        }
        else
        {
            KODI_LOG(LOG_DEBUG, "Known device %08x", id);

            for (auto t: _devices)
            {
                if (t->DeviceID() == id)
                {
                    t->Refresh(dd);
                }
            }
        }
    }

    // Iterate through devices, Refresh and determine if there are stale entries.
    auto tit = _devices.begin();
    while (tit != _devices.end())
    {
        auto device = *tit;
        uint32_t id = device->DeviceID();
        if (discovered_ids.find(id) == discovered_ids.end())
        {
            // Device went away
            device_removed = true;
            KODI_LOG(LOG_DEBUG, "Removing device %08x", id);

            auto pdevice = const_cast<Device*>(device);

            auto nit = _lineup.begin();
            while (nit != _lineup.end())
            {
                auto& number = *nit;
                auto& info = _info[number];
                if (info.RemoveDevice(device))
                {
                    KODI_LOG(LOG_DEBUG, "Removed device from GuideNumber %s", number.extendedName().c_str());
                }
                if (info.DeviceCount() == 0)
                {
                    // No devices left for this lineup guide entry, remove it
                    KODI_LOG(LOG_DEBUG, "No devices left, removing GuideNumber %s", number.extendedName().c_str());
                    nit = _lineup.erase(nit);
                    _guide.erase(number);
                    _info.erase(number);
                }
                else
                    nit ++;
            }

            // Erase device from this
            tit = _devices.erase(tit);
            _device_ids.erase(id);
            delete(device);
        }
        else
        {
            tit ++;
        }
    }

    return device_added || device_removed;
}

void PVR_HDHR::AddLineupEntry(const Json::Value& v, Device* device)
{
    GuideNumber number = v;
    if ((g.Settings.hideUnknownChannels) && (number._guidename == "Unknown"))
    {
        return;
    }
    _lineup.insert(number);
    if (_info.find(number) == _info.end())
    {
        Info info = v;
        _info[number] = info;
    }
    _info[number].AddDevice(device, v["URL"].asString());
}

bool PVR_HDHR::UpdateLineup()
{
    KODI_LOG(LOG_DEBUG, "PVR_HDHR::UpdateLineup");

    Lock lock(this);
    std::set<GuideNumber> prior;
    std::copy(_lineup.begin(), _lineup.end(), std::inserter(prior, prior.begin()));

    _lineup.clear();

    for (auto device: _devices)
    {

        KODI_LOG(LOG_DEBUG, "Requesting HDHomeRun channel lineup for %08x: %s",
                device->DeviceID(), device->LineupURL().c_str()
        );

        std::string lineupStr;
        if (!GetFileContents(device->LineupURL(), lineupStr))
        {
            KODI_LOG(LOG_ERROR, "Cannot get lineup from %s", device->LineupURL().c_str());
            continue;
        }

        Json::Value lineupJson;
        Json::Reader jsonReader;
        if (!jsonReader.parse(lineupStr, lineupJson))
        {
            KODI_LOG(LOG_ERROR, "Cannot parse JSON value returned from %s", device->LineupURL().c_str());
            continue;
        }

        if (lineupJson.type() != Json::arrayValue)
        {
            KODI_LOG(LOG_ERROR, "Lineup is not a JSON array, returned from %s", device->LineupURL().c_str());
            continue;
        }

        for (auto& v : lineupJson)
        {
            AddLineupEntry(v, device);
        }
    }

    bool added = false;
    for (const auto& number: _lineup)
    {
        auto& info = _info[number];
        std::string devices = info.DeviceListString();

        if (prior.find(number) == prior.end())
        {
            added = true;

            KODI_LOG(LOG_DEBUG,
                    "New Lineup Entry: %d.%d - %s - %s - %s",
                    number._channel,
                    number._subchannel,
                    number._guidenumber.c_str(),
                    number._guidename.c_str(),
                    devices.c_str()
            );
        }
    }
    if (added)
    {
        return true;
    }

    bool removed = false;
    for (const auto& number: prior)
    {
        if (_lineup.find(number) == _lineup.end())
            return true;
    }
}


void PVR_HDHR::TriggerEpgUpdate()
{
    Lock guidelock(_guide_lock);
    Lock lock(this);

    for (auto& channel : _lineup)
    {
        auto& guide = _guide[channel];
        guide.ResetTransferred();

        g.PVR->TriggerEpgUpdate(channel);
    }
}

bool PVR_HDHR::_age_out()
{
    Lock guidelock(_guide_lock);
    Lock lock(this);

    bool any_changed = false;

    for (auto& mapentry : _guide)
    {
        uint32_t id = mapentry.first;
        auto& guide = mapentry.second;

        bool changed = guide._age_out(id);
        if (changed)
        {
            any_changed = true;
            g.PVR->TriggerEpgUpdate(id);
        }
    }

    return any_changed;
}

bool PVR_HDHR::_insert_json_guide_data(const Json::Value& jsondeviceguide, const Device* device)
{
    if (jsondeviceguide.type() != Json::arrayValue)
    {
        KODI_LOG(LOG_ERROR, "Top-level JSON guide data is not an array for %08x", device->DeviceID());
        return {};
    }

    bool new_guide_entries{false};

    for (auto& jsonchannelguide : jsondeviceguide)
    {
        GuideNumber number = jsonchannelguide;

        if (_guide.find(number) == _guide.end())
        {
            KODI_LOG(LOG_DEBUG, "Inserting guide for channel %u", number.ID());
            _guide[number] = jsonchannelguide;
        }

        Guide& channelguide = _guide[number];

        auto jsonguidenetries = jsonchannelguide["Guide"];
        if (jsonguidenetries.type() != Json::arrayValue)
        {
            KODI_LOG(LOG_ERROR, "Guide entries is not an array for %08x", device->DeviceID());
            continue;
        }

        bool new_channel_entries{false};

        for (auto& jsonentry: jsonguidenetries)
        {
            static uint32_t counter = 1;

            GuideEntry entry{jsonentry};
            bool n = channelguide.AddEntry(entry);

            if (n) {
                new_channel_entries = true;
            }
        }

        if (new_channel_entries)
        {
            new_guide_entries = true;
            g.PVR->TriggerEpgUpdate(number);
        }
    }
    return new_guide_entries;
}

bool PVR_HDHR::_insert_guide_data(const GuideNumber* number, const Device* device, time_t start)
{
    std::string URL{"http://my.hdhomerun.com/api/guide.php?DeviceAuth="};
    if (device)
    {
        URL.append(EncodeURL(device->Auth()));
    }
    else
    {
        for (const auto t : _devices)
        {
            URL.append(EncodeURL(t->Auth()));
        }
    }

    if (number)
    {
        URL.append("&Channel=");
        URL.append(number->toString());

        if (start)
        {
            std::string start_s = std::to_string(start);
            URL.append("&Start=");
            URL.append(start_s);
        }
    }
    KODI_LOG(LOG_DEBUG, "Requesting HDHomeRun guide for %08x: %s",
            device ? device->DeviceID() : 0xffffffff, URL.c_str());

    std::string guidedata;
    if (!GetFileContents(URL, guidedata))
    {
        KODI_LOG(LOG_ERROR, "Error requesting guide for %08x from %s",
                device ? device->DeviceID() : 0xffffffff, URL.c_str());
        return {};
    }
    if (guidedata.substr(0,4) == "null")
        return {};

    Json::Reader jsonreader;
    Json::Value  jsondeviceguide;
    if (!jsonreader.parse(guidedata, jsondeviceguide))
    {
        KODI_LOG(LOG_ERROR, "Error parsing JSON guide data for %08x", device ? device->DeviceID() : 0xffffffff);
        return {};
    }
    return _insert_json_guide_data(jsondeviceguide, device);
}

bool PVR_HDHR::_update_guide_basic()
{
    // Find a minimal covering of the lineup, to avoid duplicate guide requests.
    Lock lock(this);

    return _insert_guide_data();
}

bool PVR_HDHR::_update_guide_extended(const GuideNumber& gn, time_t start)
{
    Lock lock(this);

    const auto& info = _info[gn];
    auto device = info.GetFirstDevice();

    if (!device)
        return false;

    return _insert_guide_data(&gn, device, start);
}

bool PVR_HDHR::_guide_contains(time_t t)
{
    bool   contains = true;
    bool   haveany  = false;

    for (auto& ng: _guide)
    {
        uint32_t number = ng.first;
        auto&    guide = ng.second;

        if (guide.Times().Empty())
            continue;

        if (!guide.Times().Contains(t))
        {
            return false;
        }

        haveany = true;
    }

    return haveany;
}
void PVR_HDHR::UpdateGuide()
{
    Lock guidelock(_guide_lock);

    time_t now     = time(nullptr);
    if (!_guide_contains(now) || !_guide_contains(now + g.Settings.guideBasicInterval))
    {
        _update_guide_basic();
        return;
    }

    if (g.Settings.extendedGuide)
    {
        for (auto& ng : _guide)
        {
            auto  number = ng.first;
            auto& guide  = ng.second;

            time_t start;

            if (guide.Times().Empty())
            {
                time_t again = guide.LastCheck() + g.Settings.guideZeroCheckInterval;
                if (now < again)
                    continue;

                start = now;
            }
            else if (!guide.Times().Contains(now + g.Settings.guideExtendedLength))
            {
                start = guide.Times().End();
            }
            else if (!guide.Times().Contains(now - g.Settings.guideReverseLength))
            {
                start = guide.Times().Start() - g.Settings.guideExtendedEach;
            }
            else
            {
                continue;
            }

            //std::this_thread::sleep_for(std::chrono::milliseconds(100));

            KODI_LOG(LOG_DEBUG, "Extended Update: %d %s Times %s Requests %s",
                    number,
                    FormatTime(start).c_str(),
                    guide.Times().toString().c_str(),
                    guide.Requests().toString().c_str()
                    );

            _update_guide_extended(number, start);
            guide.LastCheck(now);
        }
    }

    _age_out();
}

int PVR_HDHR::PvrGetChannelsAmount()
{
    return _lineup.size();
}
PVR_ERROR PVR_HDHR::PvrGetChannels(ADDON_HANDLE handle, bool radio)
{
    if (radio)
        return PVR_ERROR_NO_ERROR;

    Lock lock(this);
    for (auto& number: _lineup)
    {
        PVR_CHANNEL pvrChannel = {0};
        auto& guide = _guide[number];
        auto& info  = _info[number];

        pvrChannel.iUniqueId         = number.ID();
        pvrChannel.iChannelNumber    = number._channel;
        pvrChannel.iSubChannelNumber = number._subchannel;

        const std::string* name;
        if (g.Settings.channelName == SettingsType::AFFILIATE) {
            name = &guide.Affiliate();
        }
        if (!name || !name->length() || (g.Settings.channelName == SettingsType::GUIDE_NAME))
        {
            // Lineup name from guide
            name = &guide.GuideName();
        }
        if (!name || !name->length() || (g.Settings.channelName == SettingsType::TUNER_NAME))
        {
            // Lineup name from device
            name = &info._guidename;
        }
        if (!name)
        {
            static const std::string empty{""};
            name = &empty;
        }
        PVR_STRCPY(pvrChannel.strChannelName, name->c_str());
        PVR_STRCPY(pvrChannel.strIconPath, guide.ImageURL().c_str());

        g.PVR->TransferChannelEntry(handle, &pvrChannel);
    }
    return PVR_ERROR_NO_ERROR;
}



PVR_ERROR PVR_HDHR::PvrGetEPGForChannel(ADDON_HANDLE handle,
        const PVR_CHANNEL& channel, time_t start, time_t end
        )
{
    KODI_LOG(LOG_DEBUG,
            "PvrGetEPCForChannel Handle:%p Channel ID: %d Number: %u Sub: %u Start: %s End: %s",
            handle,
            channel.iUniqueId,
            channel.iChannelNumber,
            channel.iSubChannelNumber,
            FormatTime(start).c_str(),
            FormatTime(end).c_str()
    );

    Lock guidelock(_guide_lock);
    Lock lock(this);

    auto& guide = _guide[channel.iUniqueId];
    auto& times    = guide.Times();

    if (!times.Contains(start) && !times.Contains(end))
    {
        guide.AddRequest({start, end});
    }
    else if (!times.Contains(start))
    {
        guide.AddRequest({start, times.Start()});
    }
    else if (!times.Contains(end))
    {
        guide.AddRequest({times.End(), end});
    }

    for (auto& ge: guide.Entries())
    {
        if (ge._endtime < start)
            continue;
        if (ge._starttime > end)
            break;
        if (ge._transferred)
            continue;

        EPG_TAG tag = ge.Epg_Tag(channel.iUniqueId);
        g.PVR->TransferEpgEntry(handle, &tag);

        guide.Transferred(ge);
    }

    return PVR_ERROR_NO_ERROR;
}

int PVR_HDHR::PvrGetChannelGroupsAmount()
{
    return 3;
}

static const std::string FavoriteChannels = "Favorite channels";
static const std::string HDChannels       = "HD channels";
static const std::string SDChannels       = "SD channels";


PVR_ERROR PVR_HDHR::PvrGetChannelGroups(ADDON_HANDLE handle, bool bRadio)
{
    PVR_CHANNEL_GROUP channelGroup;

    if (bRadio)
        return PVR_ERROR_NO_ERROR;

    memset(&channelGroup, 0, sizeof(channelGroup));

    channelGroup.iPosition = 1;
    PVR_STRCPY(channelGroup.strGroupName, FavoriteChannels.c_str());
    g.PVR->TransferChannelGroup(handle, &channelGroup);

    channelGroup.iPosition++;
    PVR_STRCPY(channelGroup.strGroupName, HDChannels.c_str());
    g.PVR->TransferChannelGroup(handle, &channelGroup);

    channelGroup.iPosition++;
    PVR_STRCPY(channelGroup.strGroupName, SDChannels.c_str());
    g.PVR->TransferChannelGroup(handle, &channelGroup);

    return PVR_ERROR_NO_ERROR;
}

PVR_ERROR PVR_HDHR::PvrGetChannelGroupMembers(ADDON_HANDLE handle,
        const PVR_CHANNEL_GROUP &group)
{
    Lock lock(this);

    for (const auto& number: _lineup)
    {
        auto& info  = _info[number];
        auto& guide = _guide[number];

        if ((FavoriteChannels != group.strGroupName) && !info._favorite)
            continue;
        if ((HDChannels != group.strGroupName) && !info._hd)
            continue;
        if ((SDChannels != group.strGroupName) && info._hd)
            continue;

        PVR_CHANNEL_GROUP_MEMBER channelGroupMember = {0};
        PVR_STRCPY(channelGroupMember.strGroupName, group.strGroupName);
        channelGroupMember.iChannelUniqueId = number.ID();

        g.PVR->TransferChannelGroupMember(handle, &channelGroupMember);
    }
    return PVR_ERROR_NO_ERROR;
}

bool PVR_HDHR::OpenLiveStream(const PVR_CHANNEL& channel)
{
    CloseLiveStream();
    return _open_live_stream(channel);
}

void PVR_HDHR::CloseLiveStream(void)
{
    Lock strlock(_stream_lock);
    Lock lock(this);

    g.XBMC->CloseFile(_filehandle);
    _filehandle = nullptr;
}

int PVR_HDHR::ReadLiveStream(unsigned char* buffer, unsigned int size)
{
    Lock strlock(_stream_lock);

    if (_filehandle)
    {
        return g.XBMC->ReadFile(_filehandle, buffer, size);
    }
    return 0;
}


bool PVR_HDHR_TCP::_open_tcp_stream(const std::string& url)
{
    Lock strlock(_stream_lock);
    Lock lock(this);

    if (url.size())
    {
        _filehandle = g.XBMC->OpenFile(url.c_str(), 0);
    }

    KODI_LOG(LOG_DEBUG, "Attempt to tune TCP stream from url %s : %s",
            url.c_str(),
            _filehandle == nullptr ? "Fail":"Success");

    return _filehandle != nullptr;
}

bool PVR_HDHR_TCP::_open_live_stream(const PVR_CHANNEL& channel)
{
    Lock strlock(_stream_lock);
    Lock lock(this);

    auto id = channel.iUniqueId;
    const auto& entry = _lineup.find(id);
    if (entry == _lineup.end())
    {
        KODI_LOG(LOG_ERROR, "Channel %d not found!", id);
        return false;
    }
    auto& info = _info[id];
    info.ResetNextDevice();

    Device* device;
    while ((device = info.GetPreferredDevice()) != nullptr)
    {
        if (_open_tcp_stream(info.DlnaURL(device)))
            return true;
    }

    info.ResetNextDevice();
    while ((device = info.GetNextDevice()) != nullptr)
    {
        if (_open_tcp_stream(info.DlnaURL(device)))
            return true;
    }

    return false;
}

bool PVR_HDHR_UDP::_open_live_stream(const PVR_CHANNEL& channel)
{
    std::stringstream url;
    url << "udp://@192.168.1.113:" << g.Settings.udpPort;
    _filehandle = g.XBMC->CURLCreate(url.str().c_str());

    std::cout << "Attempted  " << url.str() << "\n";
    if (_filehandle)
    {
        std::cout << "Success\n";

        if (!g.XBMC->CURLOpen(_filehandle, 0))
        {
            std::cout << "CURLOpen returns false\n";
        }
    }
    else
    {
        std::cout << "Can't create " << url.str() << "\n";
    }

    return true;
}



}; // namespace PVRHDHomeRun
