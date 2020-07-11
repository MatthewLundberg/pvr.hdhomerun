/*
 *      Copyright (C) 2017-2020 Matthew Lundberg <matthew.k.lundberg@gmail.com>
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

#include "Settings.h"
#include "Utils.h"
#include "PVR_HDHR.h"
#include <chrono>
#include <functional>
#include <algorithm>
#include <iterator>
#include <string>
#include <sstream>
#include <iomanip>
#include <iterator>
#include <numeric>
#include <iostream>
#include <random>
#include <kodi/c-api/filesystem.h>
#include <kodi/AddonBase.h>

namespace kodi {
ADDONCREATOR(PVRHDHomeRun::PVR_HDHR);
}

namespace PVRHDHomeRun
{
class UpdateThread: public P8PLATFORM::CThread, Lockable
{
public:
    UpdateThread(PVR_HDHR* _pvr)
        : pvr_hdhr(_pvr)
    {};
private:
    time_t _lastDiscover = 0;
    time_t _lastLineup   = 0;
    time_t _lastGuide    = 0;
    time_t _lastRecord   = 0;
    time_t _lastRules    = 0;

    bool   _running      = false;
    PVR_HDHR* pvr_hdhr;
    
public:
    void Wake()
    {
        Lock lock(this);

        _lastDiscover = 0;
        _lastLineup   = 0;
        _lastGuide    = 0;
        _lastRecord   = 0;
        _lastRules    = 0;
        _running      = false;
    }
    void *Process() override
    {
        int state{0};
        int prev_num_networks{0};

        for (;;)
        {
            P8PLATFORM::CThread::Sleep(1000);
            if (IsStopped())
            {
                break;
            }

            {
                int num_networks = 0;

                const uint32_t localhost = 127 << 24;
                const size_t max = 64;
                struct hdhomerun_local_ip_info_t ip_info[max];
                int ip_info_count = hdhomerun_local_ip_info(ip_info, max);
                for (int i=0; i<ip_info_count; i++)
                {
                    auto& info = ip_info[i];
                    //KODI_LOG(ADDON_LOG_DEBUG, "Local IP: %s %s", FormatIP(info.ip_addr).c_str(), FormatIP(info.subnet_mask).c_str());
                    if (!IPSubnetMatch(localhost, info.ip_addr, info.subnet_mask))
                    {
                        num_networks ++;
                    }
                }

                if (num_networks != prev_num_networks)
                {
                    if (num_networks == 0)
                    {
                        KODI_LOG(ADDON_LOG_DEBUG, "UpdateThread::Process No external networks found, waiting.");
                    }
                    else
                    {
                        for (int i=0; i<ip_info_count; i++)
                        {
                            KODI_LOG(ADDON_LOG_DEBUG, "UpdateThread::Process IP %s %s",
                                    FormatIP(ip_info[i].ip_addr).c_str(),
                                    FormatIP(ip_info[i].subnet_mask).c_str()
                            );
                        }
                    }
                    prev_num_networks = num_networks;
                }
                if (num_networks == 0)
                {
                    continue;
                }
            }

            time_t now = time(nullptr);
            bool updateDiscover = false;
            bool updateLineup   = false;
            bool updateGuide    = false;
            bool updateRecord   = false;
            bool updateRules    = false;

            time_t discover, lineup, guide, recordings, rules;
            {
                Lock lock(this);
                discover   = _lastDiscover;
                lineup     = _lastLineup;
                guide      = _lastGuide;
                recordings = _lastRecord;
                rules      = _lastRules;
            }

            if (state == 0)
            {
                // Tuner discover
                if (now >= discover + g.Settings.deviceDiscoverInterval)
                {
                    bool discovered = pvr_hdhr->DiscoverTunerDevices();
                    if (discovered)
                    {
                        KODI_LOG(ADDON_LOG_DEBUG, "PVR::DiscoverDevices returned true, try again");
                        now = 0;
                        state = 0;
                    }
                    else
                    {
                        state = 1;
                    }
                    updateDiscover = true;
                }
                else
                    state = 1;
            }

            if (state == 1)
            {
                if (now >= lineup + g.Settings.lineupUpdateInterval)
                {
                    if (pvr_hdhr->UpdateLineup())
                    {
                        pvr_hdhr->TriggerChannelUpdate();
                        pvr_hdhr->TriggerChannelGroupsUpdate();
                    }

                    updateLineup = true;
                }
                else if (now >= recordings + g.Settings.recordUpdateInterval)
                {
                    if (pvr_hdhr->UpdateRecordings())
                        pvr_hdhr->TriggerRecordingUpdate();

                    updateRecord = true;
                }
                else
                    state = 2;
            }

            if (state == 2)
            {
                if (now >= rules + g.Settings.ruleUpdateInterval)
                {
                     if (pvr_hdhr->UpdateRules()) {}
                        ; // pvr_hdhr->Trigger? TODO

                     updateRules = true;
                }
                else
                    state = 3;
            }

            if (state == 3)
            {
                if (now >= guide + g.Settings.guideUpdateInterval)
                {
                    pvr_hdhr->UpdateGuide();

                    updateGuide = true;
                }
                state = 0;
            }

            if (updateDiscover || updateLineup || updateGuide || updateRecord || updateRules)
            {
                Lock lock(this);

                if (updateDiscover)
                    _lastDiscover = now;
                if (updateLineup)
                    _lastLineup = now;
                if (updateGuide)
                    _lastGuide = now;
                if (updateRecord)
                    _lastRecord = now;
                if (updateRules)
                    _lastRules = now;
            }
        }
        return nullptr;
    }
};



PVR_HDHR_TUNER* PVR_HDHR_Factory(PVR_HDHR* pvr, int protocol) {
    switch (protocol)
    {
    case SettingsType::TCP:
        return new PVR_HDHR_TCP(pvr);
    case SettingsType::UDP:
        return new PVR_HDHR_UDP(pvr);
    }
    return nullptr;
}

PVR_HDHR_TUNER::~PVR_HDHR_TUNER()
{
    for (auto device: _tuner_devices)
    {
        delete device;
    }
    for (auto device: _storage_devices)
    {
        delete device;
    }
}

ADDON_STATUS PVR_HDHR::Create()
{
    KODI_LOG(ADDON_LOG_INFO, "%s - Creating the PVR HDHomeRun add-on", __FUNCTION__);

    g.Settings.ReadSettings();
    _tuner.reset(PVR_HDHR_Factory(this, g.Settings.protocol));
    Update();
    
    _updatethread.reset(new UpdateThread(this));
    if (!_updatethread->CreateThread(false))
        return ADDON_STATUS_PERMANENT_FAILURE;

    return ADDON_STATUS_OK;
}

PVR_ERROR PVR_HDHR::OnSystemWake()
{
    Update();
    TriggerChannelUpdate();

    return PVR_ERROR_NO_ERROR;
}

ADDON_STATUS PVR_HDHR::SetSetting(const std::string& settingName, const kodi::CSettingValue& settingValue)
{
    return g.Settings.SetSetting(settingName, settingValue);
}

PVR_ERROR PVR_HDHR::GetCapabilities(kodi::addon::PVRCapabilities& capabilities)
{
    capabilities.SetSupportsEPG(true);
    capabilities.SetSupportsEPGEdl(false);
    capabilities.SetSupportsTV(true);
    capabilities.SetSupportsRadio(false);
    capabilities.SetSupportsRecordings(g.Settings.record);
    capabilities.SetSupportsRecordingsUndelete(false);
    capabilities.SetSupportsTimers(g.Settings.record);
    capabilities.SetSupportsChannelGroups(g.Settings.usegroups);
    capabilities.SetSupportsChannelScan(false);
    capabilities.SetSupportsChannelSettings(false);
    capabilities.SetHandlesInputStream(!g.Settings.use_stream_url);
    capabilities.SetHandlesDemuxing(false);
    capabilities.SetSupportsRecordingPlayCount(false);
    capabilities.SetSupportsLastPlayedPosition(true);
    capabilities.SetSupportsRecordingEdl(false);
    capabilities.SetSupportsRecordingsRename(false);
    capabilities.SetSupportsRecordingsLifetimeChange(false);
    capabilities.SetSupportsDescrambleInfo(false);
    //capabilities.SetRecordingsLifetimesSize(0);

    std::cout << __FUNCTION__ << " handles input stream " << !g.Settings.use_stream_url << std::endl;
    return PVR_ERROR_NO_ERROR;
}

PVR_ERROR PVR_HDHR::GetBackendName(std::string& name)
{
    name = "otherkids PVR";
    return PVR_ERROR_NO_ERROR;
}

PVR_ERROR PVR_HDHR::GetBackendVersion(std::string& ver)
{
    ver = "5.0.0";
    return PVR_ERROR_NO_ERROR;
}

PVR_ERROR PVR_HDHR::GetConnectionString(std::string& con)
{
    con = "connected";
    return PVR_ERROR_NO_ERROR;
}
bool PVR_HDHR::DiscoverTunerDevices()
{
    Lock guidelock(_guide_lock);
    Lock pvrlock(_pvr_lock);
    return _tuner->discover_tuner_devices();
}


bool PVR_HDHR_TUNER::discover_tuner_devices()
{
    struct hdhomerun_discover_device_t discover_devices[64];
    size_t device_count = hdhomerun_discover_find_devices_custom_v2(
            0,
            HDHOMERUN_DEVICE_TYPE_WILDCARD,
            HDHOMERUN_DEVICE_ID_WILDCARD,
            discover_devices,
            64
            );

    KODI_LOG(ADDON_LOG_DEBUG, "PVR_HDHR::DiscoverTunerDevices Found %d devices", device_count);

    if (device_count == 0)
    {
        // Sometimes no devices are found when waking from sleep, causing a
        // lot of unnecessary network traffic as they are rediscovered.
        // TODO:  Handle the case where all devices go away?
        return true;
    }

    std::set<uint32_t>    discovered_ids;  // Tuner IDs
    std::set<std::string> discovered_urls; // Storage URLs

    bool device_added    = false;
    bool device_removed  = false;
    bool storage_added   = false;
    bool storage_removed = false;

    for (size_t i=0; i<device_count; i++)
    {
        auto& dd = discover_devices[i];

        if (dd.device_type == HDHOMERUN_DEVICE_TYPE_STORAGE)
        {

            const auto url = dd.base_url;
            discovered_urls.insert(url);
            if (!g.Settings.record)
            {
                std::cout << "Record: " << g.Settings.record << " Not using " << url << std::endl;
                continue;
            }

            if (_storage_urls.find(url) == _storage_urls.end())
            {
                storage_added = true;
                KODI_LOG(ADDON_LOG_DEBUG, "Adding storage %s", url);
                std::cout << "New Storage URL " << url << std::endl;

                _storage_urls.insert(url);
                _storage_devices.insert(New_StorageDevice(&dd));
            }
            else
            {
                KODI_LOG(ADDON_LOG_DEBUG, "Known storage %s", url);
                for (auto s: _storage_devices)
                {
                    if (!strcmp(s->BaseURL(), url))
                    {
                        s->Refresh(&dd);
                    }
                }
            }
        }
        else if (dd.device_type = HDHOMERUN_DEVICE_TYPE_TUNER)
        {
            auto  id = dd.device_id;


            if (g.Settings.blacklistDevice.find(id) != g.Settings.blacklistDevice.end())
            {
                KODI_LOG(ADDON_LOG_INFO, "Ignoring blacklisted device %08x", id);
                continue;
            }

            if (dd.is_legacy && !g.Settings.UseLegacyDevices())
            {
                KODI_LOG(ADDON_LOG_INFO, "Ignoring legacy device %08x", id);
                continue;
            }

            discovered_ids.insert(id);

            if (_device_ids.find(id) == _device_ids.end())
            {
                // New device
                device_added = true;
                KODI_LOG(ADDON_LOG_DEBUG, "Adding device %08x", id);
                std::cout << "New tuner "
                        << std::hex << dd.device_id << std::dec
                        << " auth " << EncodeURL(dd.device_auth)
                        << " URL " << dd.base_url
                        << "\n";

                _tuner_devices.insert(New_TunerDevice(&dd));
                _device_ids.insert(id);
            }
            else
            {
                KODI_LOG(ADDON_LOG_DEBUG, "Known device %08x", id);

                for (auto t: _tuner_devices)
                {
                    if (t->DeviceID() == id)
                    {
                        t->Refresh(&dd);
                    }
                }
            }
        }
    }

    // Iterate through devices, Refresh and determine if there are stale entries.
    auto sit = _storage_devices.begin();
    while (sit != _storage_devices.end())
    {
        auto storage = *sit;
        const auto url = storage->BaseURL();
        if (discovered_urls.find(url) == discovered_urls.end())
        {
            storage_removed = true;
            sit = _storage_devices.erase(sit);
            _storage_urls.erase(url);
            delete(storage);
        }
        else
            sit ++;
    }

    auto tit = _tuner_devices.begin();
    while (tit != _tuner_devices.end())
    {
        auto device = *tit;
        uint32_t id = device->DeviceID();
        if (discovered_ids.find(id) == discovered_ids.end())
        {
            // Device went away
            device_removed = true;
            KODI_LOG(ADDON_LOG_DEBUG, "Removing device %08x", id);

            auto pdevice = const_cast<TunerDevice*>(device);

            auto nit = _parent->_lineup.begin();
            while (nit != _parent->_lineup.end())
            {
                auto& number = *nit;
                auto& info = _parent->_info[number];
                if (info.RemoveDevice(device))
                {
                    KODI_LOG(ADDON_LOG_DEBUG, "Removed device from GuideNumber %s", number.extendedName().c_str());
                }
                if (info.DeviceCount() == 0)
                {
                    // No devices left for this lineup guide entry, remove it
                    KODI_LOG(ADDON_LOG_DEBUG, "No devices left, removing GuideNumber %s", number.extendedName().c_str());
                    nit = _parent->_lineup.erase(nit);
                    _parent->_guide.erase(number);
                    _parent->_info.erase(number);
                }
                else
                    nit ++;
            }

            // Erase device from this
            tit = _tuner_devices.erase(tit);
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

void PVR_HDHR::AddLineupEntry(const Json::Value& v, TunerDevice* device)
{
    GuideNumber number = v;
    if ((g.Settings.hideUnknownChannels) && (number._guidename == "Unknown"))
    {
        return;
    }
    auto numberstr = number.toString();

    for (const auto& hidden: g.Settings.hiddenChannels)
    {
        if (hidden == numberstr)
            return;

        if (hidden.find('.') == std::string::npos)
        {
            // No period, means the whole channel
            auto dot = numberstr.find('.');
            if (dot != std::string::npos)
            {
                auto base = numberstr.substr(0, dot);
                if (hidden == base)
                    return;
            }
        }

        if (hidden.size() && hidden[hidden.size()-1] == '*')
        {
            auto sz = hidden.size() - 1;
            if (hidden.substr(0, sz) == numberstr.substr(0, sz))
                return;
        }
    }

    _lineup.insert(number);
    if (_info.find(number) == _info.end())
    {
        Info info = v;
        _info[number] = info;
    }
    _info[number].AddDevice(device, v["URL"].asString());
}

bool PVR_HDHR::UpdateRecordings()
{
    Lock pvrlock(_pvr_lock);

    _recording.UpdateBegin();
    for (const auto dev: _tuner->_storage_devices)
    {
        std::string s;
        if (GetFileContents(dev->StorageURL(), s))
        {
            Json::Value json;
            std::string err;
            if (StringToJson(s, json, err))
            {
                _recording.UpdateEntry(json);
            }
        }
    }
    return _recording.UpdateEntryEnd();
}

bool PVR_HDHR::UpdateRules()
{
    Lock pvrlock(_pvr_lock);

    _recording.UpdateBegin();
    TunerSet* ts = _tuner.get();
    if (ts->DeviceCount())
    {
        std::string URL{"http://api.hdhomerun.com/api/recording_rules?DeviceAuth="};
        auto authstring = ts->AuthString();
        URL.append(EncodeURL(authstring));

        std::string rulestring;
        if (!GetFileContents(URL, rulestring))
        {
            KODI_LOG(ADDON_LOG_ERROR, "Error requesting recording rules from %s", URL.c_str());
        }
        else
        {
            Json::Value rulesjson;
            std::string err;
            if (!StringToJson(rulestring, rulesjson, err))
            {
                KODI_LOG(ADDON_LOG_ERROR, "Error parsing JSON guilde data for %s - %s", URL.c_str(), err.c_str());
            }
            else
            {
                 _recording.UpdateRule(rulesjson);
            }
        }
    }
    return _recording.UpdateRuleEnd();
}

bool PVR_HDHR::UpdateLineup()
{
    KODI_LOG(ADDON_LOG_DEBUG, "PVR_HDHR::UpdateLineup");

    Lock pvrlock(_pvr_lock);
    std::set<GuideNumber> prior;
    std::copy(_lineup.begin(), _lineup.end(), std::inserter(prior, prior.begin()));

    _lineup.clear();

    for (auto device: _tuner->_tuner_devices)
    {

        KODI_LOG(ADDON_LOG_DEBUG, "Requesting channel lineup for %08x: %s",
                device->DeviceID(), device->LineupURL().c_str()
        );

        std::string lineupStr;
        if (!GetFileContents(device->LineupURL(), lineupStr))
        {
            KODI_LOG(ADDON_LOG_ERROR, "Cannot get lineup from %s", device->LineupURL().c_str());
            continue;
        }

        Json::Value lineupJson;
        std::string err;
        if (!StringToJson(lineupStr, lineupJson, err))
        {
            KODI_LOG(ADDON_LOG_ERROR, "Cannot parse JSON value returned from %s - %s", device->LineupURL().c_str(), err.c_str());
            continue;
        }

        if (lineupJson.type() != Json::arrayValue)
        {
            KODI_LOG(ADDON_LOG_ERROR, "Lineup is not a JSON array, returned from %s", device->LineupURL().c_str());
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
        std::string devices = info.IDString();

        if (prior.find(number) == prior.end())
        {
            added = true;

            KODI_LOG(ADDON_LOG_DEBUG,
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

    for (const auto& number: prior)
    {
        if (_lineup.find(number) == _lineup.end())
            return true;
    }

    return false;
}

void PVR_HDHR::_age_out(time_t now)
{
    Lock guidelock(_guide_lock);
    Lock pvrlock(_pvr_lock);

    for (auto& mapentry : _guide)
    {
        uint32_t id = mapentry.first;
        auto& guide = mapentry.second;
        guide._age_out(id, now);
    }
}

void PVR_HDHR::_insert_json_guide_data(const Json::Value& jsondeviceguide, const char* idstr)
{
    Lock pvrlock(_pvr_lock);

    if (jsondeviceguide.type() != Json::arrayValue)
    {
        KODI_LOG(ADDON_LOG_ERROR, "Top-level JSON guide data is not an array for %s", idstr);
        return;
    }

    for (auto& jsonchannelguide : jsondeviceguide)
    {
        GuideNumber number = jsonchannelguide;

        if (_guide.find(number) == _guide.end())
        {
            KODI_LOG(ADDON_LOG_DEBUG, "Inserting guide for channel %u", number.ID());
            _guide.emplace(number, jsonchannelguide);
        }

        Guide& channelguide = _guide[number];

        auto jsonguidenetries = jsonchannelguide["Guide"];
        if (jsonguidenetries.type() != Json::arrayValue)
        {
            KODI_LOG(ADDON_LOG_ERROR, "Guide entries is not an array for %s", idstr);
            continue;
        }

        bool new_channel_entries{false};

        for (auto& jsonentry: jsonguidenetries)
        {
            static uint32_t counter = 1;

            GuideEntry entry{jsonentry};
            bool n = channelguide.AddEntry(this, entry, number.ID());
        }

    }
}

void PVR_HDHR::_fetch_guide_data(const uint32_t* number, time_t start)
{
    TunerSet* ts;
    if (number)
        ts = &_info[*number];
    else
        ts = _tuner.get();

    if (!ts->DeviceCount())
        return;

    std::string URL{"http://my.hdhomerun.com/api/guide.php?DeviceAuth="};
    auto authstring = ts->AuthString();
    auto idstring   = ts->IDString();
    URL.append(EncodeURL(authstring));

    if (number)
    {
        GuideNumber gn{*number};
        URL.append("&Channel=");
        URL.append(gn.toString());

        if (start)
        {
            URL.append("&Start=");
            URL.append(std::to_string(start));
        }
    }
    KODI_LOG(ADDON_LOG_DEBUG, "Requesting guide for %s: %s %s",
            idstring.c_str(), start?FormatTime(start).c_str():"", URL.c_str());

    std::string guidedata;
    if (!GetFileContents(URL, guidedata))
    {
        KODI_LOG(ADDON_LOG_ERROR, "Error requesting guide for %s from %s",
                idstring.c_str(), URL.c_str());
        return;
    }
    if (guidedata.substr(0,4) == "null")
        return;

    Json::Value jsondeviceguide;
    std::string err;
    if (!StringToJson(guidedata, jsondeviceguide, err))
    {
        KODI_LOG(ADDON_LOG_ERROR, "Error parsing JSON guide data for %s - %s", idstring.c_str(), err.c_str());
        return;
    }
    _insert_json_guide_data(jsondeviceguide, idstring.c_str());
}

bool PVR_HDHR::_guide_contains(time_t t)
{
    // guidelock held.
    for (auto& ng: _guide)
    {
        uint32_t number = ng.first;
        auto&    guide = ng.second;

        if (guide.Times().Contains(t))
        {
            return true;
        }
    }
    return false;
}

void PVR_HDHR::UpdateGuide()
{
    // Offset by a random value to stagger load on the upstream servers.
    static std::default_random_engine generator;
    static std::uniform_int_distribution<int> distribution(0, g.Settings.guideRandom);
    static time_t basic_update_time = 0;

    time_t now = time(nullptr);

    // First remove stale entries
    _age_out(now);

    Lock guidelock(_guide_lock);

    bool do_basic = false;

    if (!_guide_contains(now))
    {
        do_basic = true;
    }
    int guide_early = g.Settings.guideBasicBeforeHour + distribution(generator);
    if (now % g.Settings.guideBasicInterval >= g.Settings.guideBasicInterval - guide_early && now - basic_update_time > guide_early)
    {
        do_basic = true;
    }
    if (basic_update_time + g.Settings.guideBasicInterval < now)
    {
        do_basic = true;
    }
    if (do_basic)
    {
        _fetch_guide_data();
        basic_update_time = now;
        return;
    }

    if (g.Settings.extendedGuide)
    {
        for (auto& ng : _guide)
        {
            auto  number = ng.first;
            auto& guide  = ng.second;

            if (guide.Times().Empty())
            {
                // Nothing retrieved for this channel with the basic guide, skip it.
                continue;
            }

            time_t tail = guide.Times().End();
            time_t end  = now + g.Settings.guideDays*24*3600;

            guide.RemoveRequest({0, now});
            if (end > tail)
            {
                guide.AddRequest({tail, end});
            }

            if (guide.Requests().Empty())
            {
                continue;                 
            }

            // First try the last interval in requests
            auto& last = guide.Requests().Last();
            auto limit = g.Settings.guideExtendedHysteresis - distribution(generator);
            if (last.Length() > limit)
            {
                _fetch_guide_data(&number, last.Start());
            }
            else if (guide.Requests().Count() > 1)
            {
                // Next attept to fill holes
                // TODO - get iterator from IntervalSet
                // Keep a separate set of start times.  Inserting into the guide modifies Requests.
                std::set<time_t> starts;
                for (const auto& i : guide.Requests().Intervals())
                {
                    auto start = i.Start();
                    if (start == last.Start())
                        break;
                    starts.insert(start);
                }
                for (auto start : starts)
                {
                    if (!guide.Requests().Contains(start))
                    {
                        _fetch_guide_data(&number, start);
                    }
                }
            }
        }
    }
}

PVR_ERROR PVR_HDHR::GetChannelsAmount(int& amount)
{
    amount =  _lineup.size();
    return PVR_ERROR_NO_ERROR;
}
PVR_ERROR PVR_HDHR::GetChannels(bool bRadio, kodi::addon::PVRChannelsResultSet& results)
{
    if (bRadio)
        return PVR_ERROR_NO_ERROR;

    Lock guidelock(_guide_lock);
    Lock pvrlock(_pvr_lock);

    for (auto& number: _lineup)
    {
        kodi::addon::PVRChannel pvrChannel;
        auto& guide = _guide[number];
        auto& info  = _info[number];

        pvrChannel.SetUniqueId(number.ID());
        pvrChannel.SetChannelNumber(number._channel);
        pvrChannel.SetSubChannelNumber(number._subchannel);

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
        pvrChannel.SetChannelName(*name);
        pvrChannel.SetIconPath(guide.ImageURL());

        results.Add(pvrChannel);
    }
    return PVR_ERROR_NO_ERROR;
}

PVR_ERROR PVR_HDHR::GetEPGForChannel(int channel, time_t start, time_t end, kodi::addon::PVREPGTagsResultSet& results)
{
    Lock guidelock(_guide_lock);
    Lock pvrlock(_pvr_lock);

    auto& guide = _guide[channel];

    for (auto& ge: guide.Entries())
    {
        if (ge._endtime < start)
            continue;
        if (ge._starttime > end)
            continue;
        auto tag = ge.Epg_Tag(channel);
        results.Add(tag);
    }

    return PVR_ERROR_NO_ERROR;
}

PVR_ERROR PVR_HDHR::GetChannelGroupsAmount(int& c)
{
    c = 3;
    return PVR_ERROR_NO_ERROR;
}

static const std::string FavoriteChannels = "Favorite channels";
static const std::string HDChannels       = "HD channels";
static const std::string SDChannels       = "SD channels";


PVR_ERROR PVR_HDHR::GetChannelGroups(bool bRadio, kodi::addon::PVRChannelGroupsResultSet& results)
{

    if (bRadio)
        return PVR_ERROR_NO_ERROR;

    kodi::addon::PVRChannelGroup channelGroup;

    channelGroup.SetPosition(1);
    channelGroup.SetGroupName(FavoriteChannels);
    results.Add(channelGroup);

    channelGroup.SetPosition(2);
    channelGroup.SetGroupName(HDChannels);
    results.Add(channelGroup);

    channelGroup.SetPosition(3);
    channelGroup.SetGroupName(SDChannels);
    results.Add(channelGroup);

    return PVR_ERROR_NO_ERROR;
}

PVR_ERROR PVR_HDHR::GetChannelGroupMembers(const kodi::addon::PVRChannelGroup& group, kodi::addon::PVRChannelGroupMembersResultSet& results)
{
    Lock guidelock(_guide_lock);
    Lock pvrlock(_pvr_lock);

    for (const auto& number: _lineup)
    {
        auto& info  = _info[number];
        auto& guide = _guide[number];

        if ((FavoriteChannels != group.GetGroupName()) && !info._favorite)
            continue;
        if ((HDChannels != group.GetGroupName()) && !info._hd)
            continue;
        if ((SDChannels != group.GetGroupName()) && info._hd)
            continue;

        kodi::addon::PVRChannelGroupMember channelGroupMember;
        
        channelGroupMember.SetGroupName(group.GetGroupName());
        channelGroupMember.SetChannelUniqueId(number.ID());
        channelGroupMember.SetChannelNumber(number._channel);
        channelGroupMember.SetSubChannelNumber(number._subchannel);

        results.Add(channelGroupMember);
    }
    return PVR_ERROR_NO_ERROR;
}

bool PVR_HDHR::OpenLiveStream(const kodi::addon::PVRChannel& channel)
{
    Lock pvrlock(_pvr_lock);

    _tuner->close_stream();

    if (g.Settings.use_stream_url)
        return false;

    auto sts = _tuner->open_stream(channel);
    return sts;
}

void PVR_HDHR::CloseLiveStream(void)
{
    Lock pvrlock(_pvr_lock);
    _tuner->close_stream();
}

int PVR_HDHR::ReadLiveStream(unsigned char* buffer, unsigned int size)
{
    return _tuner->read_stream(buffer, size);
}

PVR_ERROR PVR_HDHR::GetStreamTimes(kodi::addon::PVRStreamTimes& times)
{
    Lock pvrlock(_pvr_lock);
    return _tuner->get_stream_times(times);
}

PVR_ERROR PVR_HDHR_TUNER::get_stream_times(kodi::addon::PVRStreamTimes& times)
{

    if (_using_sd_record && _filesize) // no filesize && _starttime && _endtime)
    {
        auto now = time(0);
        auto end = std::min(_endtime, now);
        auto len = end - _starttime;

        times.SetStartTime(0);
        times.SetPTSStart(0);
        times.SetPTSBegin(0);
        times.SetPTSEnd(len * 1000 * 1000);

        //std::cout << __FUNCTION__ << " len: " << len << std::endl;
    }
    else
    {
        //std::cout << __FUNCTION__ << " not impl" << std::endl;
        return PVR_ERROR_NOT_IMPLEMENTED;
    }

    return PVR_ERROR_NO_ERROR;
}

int64_t PVR_HDHR::LengthLiveStream()
{
    return _tuner->length_stream();
}

bool PVR_HDHR::IsRealTimeStream()
{
    //std::cout << __FUNCTION__ << " " << _live_stream << " " << _filesize << std::endl;
    Lock pvrlock(_pvr_lock);
    return _tuner->live_stream();
}
bool PVR_HDHR::SeekTime(double time,bool backwards,double& startpts)
{
    std::cout << __FUNCTION__ << "(" << time << "," << backwards << ",)" << std::endl;
    return false;
}
PVR_ERROR PVR_HDHR::GetSignalStatus(int channelUid, kodi::addon::PVRSignalStatus& signalStatus)
{
    signalStatus.SetAdapterName("otherkids PVR");
    signalStatus.SetAdapterStatus("OK");

    return PVR_ERROR_NO_ERROR;
}
bool PVR_HDHR::CanPauseStream()
{
    return _tuner->can_pause_stream();
}
bool PVR_HDHR_TUNER::can_pause_stream()
{
    Lock strlock(_stream_lock);
    return g.Settings.use_stream_url || _filesize != 0;
}

bool PVR_HDHR::CanSeekStream()
{
    return _tuner->can_seek_stream();
}
bool PVR_HDHR_TUNER::can_seek_stream()
{
    Lock strlock(_stream_lock);
    return g.Settings.use_stream_url || _filesize != 0;
}
PVR_ERROR PVR_HDHR::GetChannelStreamProperties(const kodi::addon::PVRChannel& channel, std::vector<kodi::addon::PVRStreamProperty>& properties)
{
    Lock pvrlock(_pvr_lock);
    return _tuner->get_channel_stream_properties(channel, properties);
}

PVR_ERROR PVR_HDHR_TUNER::get_channel_stream_properties(const kodi::addon::PVRChannel& channel, std::vector<kodi::addon::PVRStreamProperty>& properties)
{

    if (g.Settings.use_stream_url)
    {
        auto id = channel.GetUniqueId();
        auto& info = _parent->_info[id];

        for (auto device : _storage_devices)
        {
            auto sessionid = ++ _sessionid;
            std::stringstream ss;
            ss << device->BaseURL() << "/auto/v" + info._guidenumber;
            ss << "?SessionID=0x" << std::hex << std::setw(8) << std::setfill('0') << sessionid;
            auto url = ss.str();

            properties.emplace_back(PVR_STREAM_PROPERTY_STREAMURL, url);
            std::cout << "Passing URL : " << url << std::endl;
            break;
        }
    }
    properties.emplace_back(PVR_STREAM_PROPERTY_ISREALTIMESTREAM, "true");

    return PVR_ERROR_NO_ERROR;
}
PVR_ERROR PVR_HDHR::GetDriveSpace(uint64_t& total, uint64_t& used)
{
    total = 0;
    used = 0;
    if (_tuner->_current_storage)
    {
        total = _tuner->_current_storage->FreeSpace();
        used = 0;
    }
    return PVR_ERROR_NO_ERROR;
}
PVR_ERROR PVR_HDHR::GetStreamProperties(std::vector<kodi::addon::PVRStreamProperties>&)
{
    std::cout << __FUNCTION__ << std::endl;
    return PVR_ERROR_NOT_IMPLEMENTED;
}

bool PVR_HDHR::OpenRecordedStream(const kodi::addon::PVRRecording& recording)
{
    Lock pvrlock(_pvr_lock);
    return _tuner->open_stream(recording);
}

bool PVR_HDHR_TUNER::open_stream(const kodi::addon::PVRRecording& recording)
{
    Lock strlock(_stream_lock);

    std::cout << __FUNCTION__ << std::endl;

    close_stream();

    if (g.Settings.use_stream_url)
        return false;
    
    auto id = recording.GetRecordingId();

    const auto rec = _parent->_recording.getEntry(id);
    if (!rec)
    {
        KODI_LOG(ADDON_LOG_ERROR, "Cannot find ID: %s", id);
        std::cout << "Cannot find ID: " << id << std::endl;
        return false;
    }

    const auto& ttl = rec->_title;
    const auto& ep = rec->_episodetitle;
    std::cout << id << " " << ttl << " " << ep << std::endl;
    std::cout << rec->_playurl << std::endl;

    auto sts = open_tcp_stream(rec->_playurl, false);
    if (sts)
    {
        _current_entry = rec;

        auto len = length_stream();
        _live_stream = len == 0;

        _using_sd_record = true;
        _starttime = rec->StartTime();
        _endtime   = rec->EndTime();
    }

    return sts;
}
void PVR_HDHR::CloseRecordedStream(void)
{
    std::cout << __FUNCTION__ << std::endl;
    Lock pvrlock(_pvr_lock);
    _tuner->close_stream();
    _tuner->_current_entry = nullptr;
}
int PVR_HDHR::ReadRecordedStream(unsigned char* buf, unsigned int len)
{
    return _tuner->read_stream(buf, len);
}
int64_t PVR_HDHR::SeekRecordedStream(int64_t pos, int whence)
{
    return _tuner->seek_stream(pos, whence);
}
int64_t PVR_HDHR::LengthRecordedStream(void)
{
    return _tuner->length_stream();
}
PVR_ERROR PVR_HDHR::GetRecordingStreamProperties(const kodi::addon::PVRRecording& recording, std::vector<kodi::addon::PVRStreamProperty>& properties)
{
    Lock pvrlock(_pvr_lock);

    auto id = recording.GetRecordingId();
    const auto rec = _recording.getEntry(id);
    if (!rec)
    {
        KODI_LOG(ADDON_LOG_ERROR, "Cannot find ID: %s", id);
        std::cout << "Cannot find ID: " << id << std::endl;
        return PVR_ERROR_SERVER_ERROR;
    }
    properties.emplace_back(PVR_STREAM_PROPERTY_STREAMURL, rec->_playurl);
    std::cout << "Record URL: " << rec->_playurl << std::endl;

    return PVR_ERROR_NO_ERROR;
}
PVR_ERROR PVR_HDHR::DeleteRecording(const kodi::addon::PVRRecording& recording)
{
    // TODO
    return PVR_ERROR_NOT_IMPLEMENTED;
}
PVR_ERROR PVR_HDHR::GetRecordings(bool deleted, kodi::addon::PVRRecordingsResultSet& results)
{
    if (!deleted)
    {
        Lock pvrlock(_pvr_lock);

        for (auto& r: _recording.Records())
        {
            results.Add(r.second);
        }
    }

    return PVR_ERROR_NO_ERROR;
}
PVR_ERROR PVR_HDHR::GetRecordingsAmount(bool deleted, int& amount)
{
    amount = 0;
    if (deleted)
        return PVR_ERROR_NO_ERROR;
    Lock pvrlock(_pvr_lock);
    amount = static_cast<int>(_recording.size());
    return PVR_ERROR_NO_ERROR;
}
PVR_ERROR PVR_HDHR::RenameRecording(const kodi::addon::PVRRecording& recording)
{
    // TODO ?
    return PVR_ERROR_NOT_IMPLEMENTED;
}
PVR_ERROR PVR_HDHR::GetRecordingEdl(const kodi::addon::PVRRecording& recording, std::vector<kodi::addon::PVREDLEntry>& edl)
{
    // TODO ?
    return PVR_ERROR_NOT_IMPLEMENTED;
}
PVR_ERROR PVR_HDHR::SetRecordingPlayCount(const kodi::addon::PVRRecording& recording, int count)
{
    // TODO ?
    return PVR_ERROR_NOT_IMPLEMENTED;
}
PVR_ERROR PVR_HDHR::GetRecordingLastPlayedPosition(const kodi::addon::PVRRecording& recording, int& position)
{
    //std::cout << __FUNCTION__ << " " << pvrrec.strTitle << std::endl;
    Lock pvrlock(_pvr_lock);
    auto rec = _recording.getEntry(recording.GetRecordingId());
    position = rec ? rec->Resume() : 0;
    return PVR_ERROR_NO_ERROR;
}
PVR_ERROR PVR_HDHR::SetRecordingLastPlayedPosition(const kodi::addon::PVRRecording& recording, int i)
{
    //std::cout << __FUNCTION__ << " " << pvrrec.strTitle << " " << i << std::endl;
    Lock pvrlock(_pvr_lock);
    auto rec = _recording.getEntry(recording.GetRecordingId());
    if (rec)
    {
        rec->Resume(i);
    }
    return PVR_ERROR_NO_ERROR;
}
PVR_ERROR PVR_HDHR::SetRecordingLifetime(const kodi::addon::PVRRecording& recording)
{
    // TODO ?
    return PVR_ERROR_NOT_IMPLEMENTED;
}
PVR_ERROR PVR_HDHR::DeleteAllRecordingsFromTrash()
{
    // TODO ?
    return PVR_ERROR_NOT_IMPLEMENTED;
}
PVR_ERROR PVR_HDHR::UndeleteRecording(const kodi::addon::PVRRecording& recording)
{
    // TODO ?
    return PVR_ERROR_NOT_IMPLEMENTED;
}

#define TP(x) "  " << #x << " " << t.x << std::endl
PVR_ERROR PVR_HDHR::AddTimer(const kodi::addon::PVRTimer& timer)
{
/*
     std::cout << __FUNCTION__ << std::endl <<
            TP(iParentClientIndex) <<
            TP(startTime) <<
            TP(endTime) <<
            TP(bStartAnyTime) <<
            TP(bEndAnyTime) <<
            TP(state) <<
            TP(iTimerType) <<
            TP(strTitle) <<
            TP(strEpgSearchString) <<
            TP(bFullTextEpgSearch) <<
            TP(strDirectory) <<
            TP(strSummary) <<
            TP(iPriority) <<
            TP(iLifetime) <<
            TP(iMaxRecordings) <<
            TP(iRecordingGroup) <<
            TP(firstDay) <<
            TP(iWeekdays) <<
            TP(iPreventDuplicateEpisodes) <<
            TP(iEpgUid) <<
            TP(iMarginStart) <<
            TP(iMarginEnd) <<
            TP(iGenreType) <<
            TP(iGenreSubType) <<
            TP(strSeriesLink);
*/
    return PVR_ERROR_NOT_IMPLEMENTED;
}
PVR_ERROR PVR_HDHR::DeleteTimer(const kodi::addon::PVRTimer& timer, bool forceDelete)
{
    // TODO
    return PVR_ERROR_NOT_IMPLEMENTED;
}
PVR_ERROR PVR_HDHR::GetTimersAmount(int& amount)
{
    // TODO
    return PVR_ERROR_NOT_IMPLEMENTED;
}
PVR_ERROR PVR_HDHR::GetTimers(kodi::addon::PVRTimersResultSet& results)
{
    // TODO
    return PVR_ERROR_NOT_IMPLEMENTED;
}
PVR_ERROR PVR_HDHR::UpdateTimer(const kodi::addon::PVRTimer& timer)
{
    // TODO ?
    return PVR_ERROR_NOT_IMPLEMENTED;
}

void PVR_HDHR::PauseStream(bool bPaused)
{
    std::cout << __FUNCTION__ << " " << bPaused << std::endl;
}
void PVR_HDHR::SetSpeed(int speed)
{
    // TODO
}

void PVR_HDHR_TCP::_close_stream()
{
    //Lock pvrlock(_pvr_lock);
    Lock strlock(_stream_lock);

    if (_filehandle)
        _filehandle->Close();
    _filehandle.reset();
    
    _using_sd_record = false;
    _starttime = 0;
    _endtime = 0;
    _filesize = 0;
}

int PVR_HDHR_TCP::_read_stream(unsigned char* buffer, unsigned int size)
{
    Lock strlock(_stream_lock);

    if (_filehandle)
    {
        return _filehandle->Read(buffer, size);
    }
    return 0;
}

int64_t PVR_HDHR::SeekLiveStream(int64_t position, int whence)
{
    return _tuner->seek_stream(position, whence);
}

int64_t PVR_HDHR_TUNER::seek_stream(int64_t position, int whence)
{
    Lock strlock(_stream_lock);

    if (_filehandle)
    {
        //std::cout << __FUNCTION__ << '(' << position << ',' << whence << ')';
        auto pos = _filehandle->Seek(position, whence);
        //std::cout  << " -> " << pos << std::endl;
        return pos;
    }
    return -1;
}
int64_t PVR_HDHR_TUNER::length_stream()
{
    Lock strlock(_stream_lock);

    //if (_current_entry)
    //{
    //    return _current_entry->Length();
    //}
    //else
    if (_filehandle)
    {
        auto len = _filehandle->GetLength();
        //std::cout << __FUNCTION__ << " " << len << std::endl;
        //auto pvs = g.XBMC->GetFilePropertyValues(_filehandle, )
        //std::cout << "  pos: " << _filehandle->GetPosition() << std::endl;
        return len ? len : -1;
    }
    return -1;
}

bool PVR_HDHR_TUNER::open_tcp_stream(const std::string& url, bool live)
{
    Lock strlock(_stream_lock);
    
    _live_stream = false;
    _starttime = 0;
    _endtime = 0;

#   define COMMON_OPTIONS (ADDON_READ_CHUNKED | ADDON_READ_AUDIO_VIDEO | ADDON_READ_REOPEN | ADDON_READ_TRUNCATED)
//#   define COMMON_OPTIONS (ADDON_READ_AUDIO_VIDEO | ADDON_READ_MULTI_STREAM | ADDON_READ_REOPEN | ADDON_READ_TRUNCATED)
//#   define COMMON_OPTIONS (ADDON_READ_CHUNKED | ADDON_READ_TRUNCATED)
#   if NO_FILE_CACHE
#       define OPEN_OPTIONS (COMMON_OPTIONS | ADDON_READ_NO_CACHE)
#   else
#       define OPEN_OPTIONS (COMMON_OPTIONS | ADDON_READ_CACHED)
#   endif

    unsigned int flags = OPEN_OPTIONS;
    if (live)
    {
//        flags |= ADDON_READ_BITRATE;
    }

    _filehandle.reset();
    if (url.size())
    {
        _filehandle.reset(new kodi::vfs::CFile());
        _filehandle->CURLCreate(url.c_str());
        if (!_filehandle)
        {
            KODI_LOG(ADDON_LOG_ERROR, "Error creating CURL connection.");
        }
        else
        {
            bool sts = _filehandle->CURLAddOption(ADDON_CURL_OPTION_PROTOCOL, "seekable", "1");
            if (!sts)
            {
                KODI_LOG(ADDON_LOG_ERROR, "Cannot add CURL seekable option.");
            }
            else
            {
                sts = _filehandle->CURLOpen(flags);
            }
            if (!sts)
            {
                _filehandle->Close();
                _filehandle.reset();
            }

#if NO_FILE_CACHE
            auto dur_s = _filehandle->GetPropertyValue(ADDON_FILE_PROPERTY_RESPONSE_HEADER, "X-Content-Duration");
            auto bps_s = _filehandle->GetPropertyValue(ADDON_FILE_PROPERTY_RESPONSE_HEADER, "X-Content-BitsPerSecond");
            auto cr_s  = _filehandle->GetPropertyValue(ADDON_FILE_PROPERTY_RESPONSE_HEADER, "Content-Range");
            auto ar_s  = _filehandle->GetPropertyValue(ADDON_FILE_PROPERTY_RESPONSE_HEADER, "Accept-Ranges");

            _duration = 0;
            if (dur_s != "")
            {
                _duration = std::atoi(dur_s.c_str());
            }
            _bps = 0;
            if (bps_s != "")
            {
                _bps = std::atoi(bps_s.c_str());
            }


            std::cout << " Len: " << _length << " dur: " << _duration << " bps: " << _bps /* << " Len: " << Length() */ << std::endl;
            if (cr_s != "")
            {
                std::cout << "CR: " << cr_s << std::endl;
            }
            if (ar_s != "")
            {
                std::cout << "AR: " << ar_s << std::endl;
            }
#endif
        }
    }
    if (_filehandle)
    {
        if (live)
        {
            _live_stream = true;
            _starttime = time(0);
            _endtime = std::numeric_limits<time_t>::max();
        }
        _filesize = _filehandle->GetLength();
    }

    KODI_LOG(ADDON_LOG_DEBUG, "Attempt to open TCP stream from url %s : %s",
            url.c_str(),
            _filehandle ? "Fail":"Success");

    return _filehandle.get();
}

bool PVR_HDHR_TCP::_open_stream(const kodi::addon::PVRChannel& channel)
{
    Lock strlock(_stream_lock);

    auto id = channel.GetUniqueId();
    const auto& entry = _parent->_lineup.find(id);
    if (entry == _parent->_lineup.end())
    {
        KODI_LOG(ADDON_LOG_ERROR, "Channel %d not found!", id);
        return false;
    }
    auto& info = _parent->_info[id];

    if (g.Settings.recordforlive && _storage_devices.size())
    {
        for (auto device : _storage_devices)
        {
            auto sessionid = ++ _sessionid;
            std::stringstream ss;
            ss << device->BaseURL() << "/auto/v" + info._guidenumber;
            ss << "?SessionID=0x" << std::hex << std::setw(8) << std::setfill('0') << sessionid;
            auto url = ss.str();
            if (open_tcp_stream(url, true))
            {
                _current_storage = device;
                _using_sd_record = true;
                return true;
            }
        }
        KODI_LOG(ADDON_LOG_INFO, "Failed to tune channel %s from storage, falling back to tuner device", info._guidenumber.c_str());
    }
    std::cout << "Using direct tuning" << std::endl;
    _using_sd_record = false;

    for (auto id : g.Settings.preferredDevice)
    {
        for (auto device : info)
            if (device->DeviceID() == id && open_tcp_stream(info.DlnaURL(device), true))
                return true;
    }
    for (auto device : info)
    {
        if (open_tcp_stream(info.DlnaURL(device), true))
            return true;
    }

    return false;
}

bool PVR_HDHR_UDP::_open_stream(const kodi::addon::PVRChannel& channel)
{
    //Lock pvrlock(_pvr_lock);
    Lock strlock(_stream_lock);
    _live_stream = true;
    _starttime = time(0);
    _endtime   = std::numeric_limits<time_t>::max();
    return false;
}

int PVR_HDHR_UDP::_read_stream(unsigned char* buffer, unsigned int size)
{
    Lock strlock(_stream_lock);
    return 0;
}

void PVR_HDHR_UDP::_close_stream()
{
    //Lock pvrlock(_pvr_lock);
    Lock strlock(_stream_lock);
}

}; // namespace PVRHDHomeRun

