/*
 *      Copyright (C) 2017-2018 Matthew Lundberg <matthew.k.lundberg@gmail.com>
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

#include "Addon.h"
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

PVR_HDHR::~PVR_HDHR()
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

bool PVR_HDHR::DiscoverTunerDevices()
{
    struct hdhomerun_discover_device_t discover_devices[64];
    size_t device_count = hdhomerun_discover_find_devices_custom_v2(
            0,
            HDHOMERUN_DEVICE_TYPE_WILDCARD,
            HDHOMERUN_DEVICE_ID_WILDCARD,
            discover_devices,
            64
            );

    KODI_LOG(LOG_DEBUG, "PVR_HDHR::DiscoverTunerDevices Found %d devices", device_count);

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

    Lock guidelock(_guide_lock);
    Lock lock(this);
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
                KODI_LOG(LOG_DEBUG, "Adding storage %s", url);
                std::cout << "New Storage URL " << url << std::endl;

                _storage_urls.insert(url);
                _storage_devices.insert(New_StorageDevice(&dd));
            }
            else
            {
                KODI_LOG(LOG_DEBUG, "Known storage %s", url);
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
                KODI_LOG(LOG_INFO, "Ignoring blacklisted device %08x", id);
                continue;
            }

            if (dd.is_legacy && !g.Settings.UseLegacyDevices())
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
                KODI_LOG(LOG_DEBUG, "Known device %08x", id);

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
            KODI_LOG(LOG_DEBUG, "Removing device %08x", id);

            auto pdevice = const_cast<TunerDevice*>(device);

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
        if (hidden.size() && hidden[hidden.size()-1] == '*')
        {
            auto sz = hidden.size() - 1;
            if (hidden.substr(0, sz) == numberstr.substr(0, sz))
                return;
        }
        if (hidden == numberstr)
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

bool PVR_HDHR::UpdateRecordings()
{
    Lock lock(this);

    _recording.UpdateBegin();
    for (const auto dev:_storage_devices)
        dev->UpdateRecord(_recording);
    return _recording.UpdateEnd();
}

bool PVR_HDHR::UpdateLineup()
{
    KODI_LOG(LOG_DEBUG, "PVR_HDHR::UpdateLineup");

    Lock lock(this);
    std::set<GuideNumber> prior;
    std::copy(_lineup.begin(), _lineup.end(), std::inserter(prior, prior.begin()));

    _lineup.clear();

    for (auto device: _tuner_devices)
    {

        KODI_LOG(LOG_DEBUG, "Requesting channel lineup for %08x: %s",
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
        std::string devices = info.IDString();

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

void PVR_HDHR::_age_out(time_t now)
{
    Lock guidelock(_guide_lock);
    Lock lock(this);

    for (auto& mapentry : _guide)
    {
        uint32_t id = mapentry.first;
        auto& guide = mapentry.second;
        guide._age_out(id, now);
    }
}

void PVR_HDHR::_insert_json_guide_data(const Json::Value& jsondeviceguide, const char* idstr)
{
    Lock lock(this);

    if (jsondeviceguide.type() != Json::arrayValue)
    {
        KODI_LOG(LOG_ERROR, "Top-level JSON guide data is not an array for %s", idstr);
        return;
    }

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
            KODI_LOG(LOG_ERROR, "Guide entries is not an array for %s", idstr);
            continue;
        }

        bool new_channel_entries{false};

        for (auto& jsonentry: jsonguidenetries)
        {
            static uint32_t counter = 1;

            GuideEntry entry{jsonentry};
            bool n = channelguide.AddEntry(entry, number.ID());
        }

    }
}

void PVR_HDHR::_fetch_guide_data(const uint32_t* number, time_t start)
{
    TunerSet* ts;
    if (number)
        ts = &_info[*number];
    else
        ts = this;

    if (!ts->DeviceCount())
        return;

    std::string URL{"http://my.hdhomerun.com/api/guide.php?DeviceAuth="};
    auto authstring = ts->AuthString();
    auto idstring   = ts->IDString();
    auto auth = EncodeURL(authstring);
    URL.append(auth);

    if (number)
    {
        GuideNumber gn{*number};
        URL.append("&Channel=");
        URL.append(gn.toString());

        if (start)
        {
            auto start_s = std::to_string(start);
            URL.append("&Start=");
            URL.append(start_s);
        }
    }
    KODI_LOG(LOG_DEBUG, "Requesting guide for %s: %s %s",
            idstring.c_str(), start?FormatTime(start).c_str():"", URL.c_str());

    std::string guidedata;
    if (!GetFileContents(URL, guidedata))
    {
        KODI_LOG(LOG_ERROR, "Error requesting guide for %s from %s",
                idstring.c_str(), URL.c_str());
        return;
    }
    if (guidedata.substr(0,4) == "null")
        return;

    Json::Reader jsonreader;
    Json::Value  jsondeviceguide;
    if (!jsonreader.parse(guidedata, jsondeviceguide))
    {
        KODI_LOG(LOG_ERROR, "Error parsing JSON guide data for %s", idstring.c_str());
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
        std::cout << "Guide does not contain now - ";
        do_basic = true;
    }
    int guide_early = g.Settings.guideBasicBeforeHour + distribution(generator);
    if (now % g.Settings.guideBasicInterval >= g.Settings.guideBasicInterval - guide_early && now - basic_update_time > guide_early)
    {
        std::cout << guide_early << " seconds til the hour - ";
        do_basic = true;
    }
    if (basic_update_time + g.Settings.guideBasicInterval < now)
    {
        std::cout << "update based on interval - ";
        do_basic = true;
    }
    if (do_basic)
    {
        std::cout << "Update basic guide\n";
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
                for (auto& i : guide.Requests()._intervals)
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

int PVR_HDHR::GetChannelsAmount()
{
    return _lineup.size();
}
PVR_ERROR PVR_HDHR::GetChannels(ADDON_HANDLE handle, bool radio)
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
        pvr_strcpy(pvrChannel.strChannelName, name->c_str());
        pvr_strcpy(pvrChannel.strIconPath, guide.ImageURL().c_str());

        g.PVR->TransferChannelEntry(handle, &pvrChannel);
    }
    return PVR_ERROR_NO_ERROR;
}

PVR_ERROR PVR_HDHR::GetEPGForChannel(ADDON_HANDLE handle,
        const PVR_CHANNEL& channel, time_t start, time_t end
        )
{
    Lock guidelock(_guide_lock);
    Lock lock(this);

    auto& guide = _guide[channel.iUniqueId];

    for (auto& ge: guide.Entries())
    {
        if (ge._endtime < start)
            continue;
        if (ge._starttime > end)
            continue;
        EPG_TAG tag = ge.Epg_Tag(channel.iUniqueId);
        g.PVR->TransferEpgEntry(handle, &tag);
    }

   if (channel.iUniqueId == 130002)
   {
        auto& times    = guide.Times();
        std::cout << "PvrGetEPGForChannel " << channel.iUniqueId << " start " << FormatTime(start) << " end " << FormatTime(end) << " times " << times.toString() << " requests " << guide.Requests().toString() << "\n";
   }

    return PVR_ERROR_NO_ERROR;
}

int PVR_HDHR::GetChannelGroupsAmount()
{
    return 3;
}

static const std::string FavoriteChannels = "Favorite channels";
static const std::string HDChannels       = "HD channels";
static const std::string SDChannels       = "SD channels";


PVR_ERROR PVR_HDHR::GetChannelGroups(ADDON_HANDLE handle, bool bRadio)
{
    PVR_CHANNEL_GROUP channelGroup;

    if (bRadio)
        return PVR_ERROR_NO_ERROR;

    memset(&channelGroup, 0, sizeof(channelGroup));

    channelGroup.iPosition = 1;
    pvr_strcpy(channelGroup.strGroupName, FavoriteChannels.c_str());
    g.PVR->TransferChannelGroup(handle, &channelGroup);

    channelGroup.iPosition++;
    pvr_strcpy(channelGroup.strGroupName, HDChannels.c_str());
    g.PVR->TransferChannelGroup(handle, &channelGroup);

    channelGroup.iPosition++;
    pvr_strcpy(channelGroup.strGroupName, SDChannels.c_str());
    g.PVR->TransferChannelGroup(handle, &channelGroup);

    return PVR_ERROR_NO_ERROR;
}

PVR_ERROR PVR_HDHR::GetChannelGroupMembers(ADDON_HANDLE handle,
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
        pvr_strcpy(channelGroupMember.strGroupName, group.strGroupName);
        channelGroupMember.iChannelUniqueId = number.ID();

        g.PVR->TransferChannelGroupMember(handle, &channelGroupMember);
    }
    return PVR_ERROR_NO_ERROR;
}

bool PVR_HDHR::OpenLiveStream(const PVR_CHANNEL& channel)
{
    Lock lock(this);

    _close_stream();
    auto sts = _open_stream(channel);
    if (sts)
    {
        _live_stream = true;
    }
    return sts;
}

void PVR_HDHR::CloseLiveStream(void)
{
    _close_stream();
}

int PVR_HDHR::ReadLiveStream(unsigned char* buffer, unsigned int size)
{
    return _read_stream(buffer, size);
}

PVR_ERROR PVR_HDHR::GetStreamTimes(PVR_STREAM_TIMES *times)
{
    Lock lock(this);

    if (_current_entry && _filesize)
    {
        auto now = time(0);
        auto endtime = _current_entry->_endtime;
        if (endtime > now)
            endtime = now;
        auto len = endtime - _current_entry->_starttime;

        times->startTime = 0;
        times->ptsStart  = 0;
        times->ptsBegin  = 0;
        times->ptsEnd = len * 1000 * 1000;
    }
    else
    {
        return PVR_ERROR_NOT_IMPLEMENTED;
    }

    return PVR_ERROR_NO_ERROR;
}

long long PVR_HDHR::LengthLiveStream()
{
    return _length_stream();
}

bool PVR_HDHR::IsRealTimeStream()
{
    return _live_stream;
}
bool PVR_HDHR::SeekTime(double time,bool backwards,double* startpts)
{
    std::cout << __FUNCTION__ << "(" << time << "," << backwards << ",)" << std::endl;
    return false;
}
PVR_ERROR PVR_HDHR::SignalStatus(PVR_SIGNAL_STATUS &signalStatus)
{
    // TODO
    pvr_strcpy(signalStatus.strAdapterName, "otherkids PVR");
    pvr_strcpy(signalStatus.strAdapterStatus, "OK");

    return PVR_ERROR_NO_ERROR;
}
bool PVR_HDHR::CanPauseStream(void)
{
    return _filesize != 0;
}
bool PVR_HDHR::CanSeekStream(void)
{
    return _filesize != 0;
}
PVR_ERROR PVR_HDHR::GetChannelStreamProperties(const PVR_CHANNEL* channel, PVR_NAMED_VALUE* properties, unsigned int* iPropertiesCount)
{
    // TODO
    *iPropertiesCount = 0;

    return PVR_ERROR_NO_ERROR;
}
PVR_ERROR PVR_HDHR::GetDriveSpace(long long *iTotal, long long *iUsed)
{
    *iTotal = 0;
    *iUsed = 0;
    if (_current_storage)
    {
        *iTotal = _current_storage->FreeSpace();
        *iUsed = 0;
    }
    return PVR_ERROR_NO_ERROR;
}
PVR_ERROR PVR_HDHR::GetStreamProperties(PVR_STREAM_PROPERTIES*)
{
    return PVR_ERROR_NOT_IMPLEMENTED;
}

bool PVR_HDHR::OpenRecordedStream(const PVR_RECORDING& pvrrec)
{
    Lock strlock(_stream_lock);
    Lock lock(this);

    std::cout << __FUNCTION__ << std::endl;

    _close_stream();

    const auto& id = pvrrec.strRecordingId;
    const auto prec = _recording.Records().find(id);
    if (prec == _recording.Records().end())
    {
        KODI_LOG(LOG_ERROR, "Cannot find ID: %s", id);
        std::cout << "Cannot find ID: " << id << std::endl;
        return false;
    }

    const auto& rec = prec->second;

    const auto& ttl = rec._title;
    const auto& ep = rec._episodetitle;
    std::cout << id << " " << ttl << " " << ep << std::endl;
    std::cout << rec._playurl << std::endl;

    auto sts = _open_tcp_stream(rec._playurl);
    if (sts)
    {
        _current_entry = &rec;
        _live_stream = false;
    }

    return sts;
}
void PVR_HDHR::CloseRecordedStream(void)
{
    _close_stream();
    _current_entry = nullptr;
}
int PVR_HDHR::ReadRecordedStream(unsigned char* buf, unsigned int len)
{
    return _read_stream(buf, len);
}
long long PVR_HDHR::SeekRecordedStream(long long pos, int whence)
{
    return _seek_stream(pos, whence);
}
long long PVR_HDHR::LengthRecordedStream(void)
{
    return _length_stream();
}
PVR_ERROR PVR_HDHR::GetRecordingStreamProperties(const PVR_RECORDING*, PVR_NAMED_VALUE*, unsigned int*)
{
    // TODO
    return PVR_ERROR_NOT_IMPLEMENTED;
}
PVR_ERROR PVR_HDHR::DeleteRecording(const PVR_RECORDING&)
{
    // TODO
    return PVR_ERROR_NOT_IMPLEMENTED;
}
PVR_ERROR PVR_HDHR::GetRecordings(ADDON_HANDLE handle, bool deleted)
{
    if (!deleted)
    {
        Lock lock(this);
        std::cout << __FUNCTION__ << std::endl;

        for (auto& r: _recording.Records())
        {
            PVR_RECORDING prec = r.second;
            g.PVR->TransferRecordingEntry(handle, &prec);
            std::cout << "  " << r.second._title << std::endl;
        }
    }

    return PVR_ERROR_NO_ERROR;
}
int PVR_HDHR::GetRecordingsAmount(bool deleted)
{
    if (deleted)
        return 0;

    Lock lock(this);
    return static_cast<int>(_recording.size());
}
PVR_ERROR PVR_HDHR::RenameRecording(const PVR_RECORDING&)
{
    // TODO ?
    return PVR_ERROR_NOT_IMPLEMENTED;
}
PVR_ERROR PVR_HDHR::GetRecordingEdl(const PVR_RECORDING&, PVR_EDL_ENTRY[], int* size)
{
    // TODO ?
    return PVR_ERROR_NOT_IMPLEMENTED;
}
PVR_ERROR PVR_HDHR::SetRecordingPlayCount(const PVR_RECORDING&, int count)
{
    // TODO ?
    return PVR_ERROR_NOT_IMPLEMENTED;
}
int PVR_HDHR::GetRecordingLastPlayedPosition(const PVR_RECORDING&)
{
    // TODO ?
    return -1;
}
PVR_ERROR PVR_HDHR::SetRecordingLastPlayedPosition(const PVR_RECORDING&, int)
{
    // TODO ?
    return PVR_ERROR_NOT_IMPLEMENTED;
}
PVR_ERROR PVR_HDHR::SetRecordingLifetime(const PVR_RECORDING*)
{
    // TODO ?
    return PVR_ERROR_NOT_IMPLEMENTED;
}
PVR_ERROR PVR_HDHR::DeleteAllRecordingsFromTrash()
{
    // TODO ?
    return PVR_ERROR_NOT_IMPLEMENTED;
}
PVR_ERROR PVR_HDHR::UndeleteRecording(const PVR_RECORDING&)
{
    // TODO ?
    return PVR_ERROR_NOT_IMPLEMENTED;
}

PVR_ERROR PVR_HDHR::AddTimer(const PVR_TIMER&)
{
    // TODO
    return PVR_ERROR_NOT_IMPLEMENTED;
}
PVR_ERROR PVR_HDHR::DeleteTimer(const PVR_TIMER&, bool)
{
    // TODO
    return PVR_ERROR_NOT_IMPLEMENTED;
}
int PVR_HDHR::GetTimersAmount(void)
{
    // TODO
    return -1;
}
PVR_ERROR PVR_HDHR::GetTimers(ADDON_HANDLE)
{
    // TODO
    return PVR_ERROR_NOT_IMPLEMENTED;
}
PVR_ERROR PVR_HDHR::UpdateTimer(const PVR_TIMER&)
{
    // TODO ?
    return PVR_ERROR_NOT_IMPLEMENTED;
}

void PVR_HDHR::PauseStream(bool bPaused)
{
    // TODO
}
void PVR_HDHR::SetSpeed(int speed)
{
    // TODO
}
bool PVR_HDHR::IsTimeshifting(void)
{
    // TODO
    return false;
}

void PVR_HDHR_TCP::_close_stream()
{
    Lock strlock(_stream_lock);
    Lock lock(this);

    g.XBMC->CloseFile(_filehandle);
    _filehandle = nullptr;
    _filesize = 0;
}

int PVR_HDHR_TCP::_read_stream(unsigned char* buffer, unsigned int size)
{
    Lock strlock(_stream_lock);
    Lock lock(this);

    if (_filehandle)
    {
        return g.XBMC->ReadFile(_filehandle, buffer, size);
    }
    return 0;
}

long long PVR_HDHR::SeekLiveStream(long long position, int whence)
{
    //return _seek_stream(position, whence);
    return PVR_ERROR_NOT_IMPLEMENTED;
}

int64_t PVR_HDHR::_seek_stream(int64_t position, int whence)
{
    Lock strlock(_stream_lock);
    Lock lock(this);

    auto pos = g.XBMC->SeekFile(_filehandle, position, whence);
    std::cout << __FUNCTION__ << '(' << position << ',' << whence << ')' << " -> " << pos << std::endl;
    return pos;
}
int64_t PVR_HDHR::_length_stream()
{
    Lock strlock(_stream_lock);

    if (_filehandle)
    {
        auto len = g.XBMC->GetFileLength(_filehandle);
        std::cout << __FUNCTION__ << " " << len << std::endl;
        return len;
    }
    return -1;
}

bool PVR_HDHR::_open_tcp_stream(const std::string& url)
{
    Lock strlock(_stream_lock);
    Lock lock(this);

    _filehandle = nullptr;
    if (url.size())
    {
        _filehandle = g.XBMC->CURLCreate(url.c_str());
        if (!_filehandle)
        {
            KODI_LOG(LOG_ERROR, "Error creating CURL connection.");
        }
        else
        {
            bool sts = g.XBMC->CURLAddOption(_filehandle, XFILE::CURLOPTIONTYPE::CURL_OPTION_PROTOCOL, "seekable", "1");
            if (!sts)
            {
                KODI_LOG(LOG_ERROR, "Cannot add CURL seekable option.");
            }
            else
            {
                sts = g.XBMC->CURLOpen(_filehandle, XFILE::READ_AUDIO_VIDEO | XFILE::READ_MULTI_STREAM | XFILE::READ_REOPEN );
            }
            if (!sts)
            {
                g.XBMC->CloseFile(_filehandle);
                _filehandle = nullptr;
            }
        }
    }
    if (_filehandle)
    {
        auto len = g.XBMC->GetFileLength(_filehandle);
        std::cout << __FUNCTION__ << " file length: " << len << std::endl;
    }

    KODI_LOG(LOG_DEBUG, "Attempt to open TCP stream from url %s : %s",
            url.c_str(),
            _filehandle == nullptr ? "Fail":"Success");

    return _filehandle != nullptr;
}

bool PVR_HDHR_TCP::_open_stream(const PVR_CHANNEL& channel)
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

    if (g.Settings.recordforlive && _storage_devices.size())
    {
        for (auto device : _storage_devices)
        {
            auto sessionid = ++ _sessionid;
            std::stringstream ss;
            ss << device->BaseURL() << "/auto/v" + info._guidenumber;
            ss << "?SessionID=0x" << std::hex << std::setw(8) << std::setfill('0') << sessionid;
            auto url = ss.str();
            if (_open_tcp_stream(url))
            {
                _current_storage = device;
                std::cout << __FUNCTION__ << " Recorder used: " << url << std::endl;
                return true;
            }
        }
        KODI_LOG(LOG_INFO, "Failed to tune channel %s from storage, falling back to tuner device", info._guidenumber.c_str());
    }
    std::cout << "Failed to tune from recorder." << std::endl;

    for (auto id : g.Settings.preferredDevice)
    {
        for (auto device : info)
            if (device->DeviceID() == id && _open_tcp_stream(info.DlnaURL(device)))
                return true;
    }
    for (auto device : info)
    {
        if (_open_tcp_stream(info.DlnaURL(device)))
            return true;
    }

    return false;
}

bool PVR_HDHR_UDP::_open_stream(const PVR_CHANNEL& channel)
{
    Lock strlock(_stream_lock);
    Lock lock(this);
    return false;
}

int PVR_HDHR_UDP::_read_stream(unsigned char* buffer, unsigned int size)
{
    Lock strlock(_stream_lock);
    return 0;
}

void PVR_HDHR_UDP::_close_stream()
{
    Lock strlock(_stream_lock);
    Lock lock(this);
}

}; // namespace PVRHDHomeRun
