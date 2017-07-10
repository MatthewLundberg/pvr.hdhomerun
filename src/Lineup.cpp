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
#include "Lineup.h"
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

bool Lineup::DiscoverTuners()
{
    struct hdhomerun_discover_device_t discover_devices[64];
    size_t tuner_count = hdhomerun_discover_find_devices_custom_v2(
            0,
            HDHOMERUN_DEVICE_TYPE_TUNER,
            HDHOMERUN_DEVICE_ID_WILDCARD,
            discover_devices,
            64
            );

    KODI_LOG(LOG_DEBUG, "Found %d tuners", tuner_count);

    std::set<uint32_t> discovered_ids;

    bool tuner_added   = false;
    bool tuner_removed = false;

    Lock guidelock(_guide_lock);
    Lock lock(this);
    for (size_t i=0; i<tuner_count; i++)
    {
        auto& dd = discover_devices[i];
        auto  id = dd.device_id;

        if (dd.is_legacy && !g.Settings.useLegacyTuners)
            continue;

        discovered_ids.insert(id);

        if (_device_ids.find(id) == _device_ids.end())
        {
            // New tuner
            tuner_added = true;
            KODI_LOG(LOG_DEBUG, "Adding tuner %08x", id);

            _tuners.insert(new Tuner(dd));
            _device_ids.insert(id);
        }
        else
        {
            KODI_LOG(LOG_DEBUG, "Known tuner %08x", id);

            for (auto t: _tuners)
            {
                if (t->DeviceID() == id)
                {
                    t->Refresh(dd);
                }
            }
        }
    }

    // Iterate through tuners, Refresh and determine if there are stale entries.
    auto tit = _tuners.begin();
    while (tit != _tuners.end())
    {
        auto tuner = *tit;
        uint32_t id = tuner->DeviceID();
        if (discovered_ids.find(id) == discovered_ids.end())
        {
            // Tuner went away
            tuner_removed = true;
            KODI_LOG(LOG_DEBUG, "Removing tuner %08x", id);

            auto ptuner = const_cast<Tuner*>(tuner);

            auto nit = _lineup.begin();
            while (nit != _lineup.end())
            {
                auto& number = *nit;
                auto& info = _info[number];
                if (info.RemoveTuner(tuner))
                {
                    KODI_LOG(LOG_DEBUG, "Removed tuner from GuideNumber %s", number.extendedName().c_str());
                }
                if (info.TunerCount() == 0)
                {
                    // No tuners left for this lineup guide entry, remove it
                    KODI_LOG(LOG_DEBUG, "No tuners left, removing GuideNumber %s", number.extendedName().c_str());
                    nit = _lineup.erase(nit);
                    _guide.erase(number);
                    _info.erase(number);
                }
                else
                    nit ++;
            }

            // Erase tuner from this
            tit = _tuners.erase(tit);
            _device_ids.erase(id);
            delete(tuner);
        }
        else
        {
            tit ++;
        }
    }

    return tuner_added || tuner_removed;
}

void Lineup::AddLineupEntry(const Json::Value& v, Tuner* tuner)
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
    _info[number].AddTuner(tuner, v["URL"].asString());
}

bool Lineup::UpdateLineup()
{
    KODI_LOG(LOG_DEBUG, "Lineup::UpdateLineup");

    Lock lock(this);
    std::set<GuideNumber> prior;
    std::copy(_lineup.begin(), _lineup.end(), std::inserter(prior, prior.begin()));

    _lineup.clear();

    for (auto tuner: _tuners)
    {

        KODI_LOG(LOG_DEBUG, "Requesting HDHomeRun channel lineup for %08x: %s",
                tuner->_discover_device.device_id, tuner->_lineupURL.c_str()
        );

        std::string lineupStr;
        if (!GetFileContents(tuner->_lineupURL, lineupStr))
        {
            KODI_LOG(LOG_ERROR, "Cannot get lineup from %s", tuner->_lineupURL.c_str());
            continue;
        }

        Json::Value lineupJson;
        Json::Reader jsonReader;
        if (!jsonReader.parse(lineupStr, lineupJson))
        {
            KODI_LOG(LOG_ERROR, "Cannot parse JSON value returned from %s", tuner->_lineupURL.c_str());
            continue;
        }

        if (lineupJson.type() != Json::arrayValue)
        {
            KODI_LOG(LOG_ERROR, "Lineup is not a JSON array, returned from %s", tuner->_lineupURL.c_str());
            continue;
        }

        for (auto& v : lineupJson)
        {
            AddLineupEntry(v, tuner);
        }
    }

    bool added = false;
    for (const auto& number: _lineup)
    {
        auto& info = _info[number];
        std::string tuners = info.TunerListString();

        if (prior.find(number) == prior.end())
        {
            added = true;

            KODI_LOG(LOG_DEBUG,
                    "New Lineup Entry: %d.%d - %s - %s - %s",
                    number._channel,
                    number._subchannel,
                    number._guidenumber.c_str(),
                    number._guidename.c_str(),
                    tuners.c_str()
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

namespace
{
// Increment the first element until max is reached, then increment further indices.
// Recursive function used in Lineup::UpdateGuide to find the minimal covering
bool increment_index(
        std::vector<size_t>::iterator index,
        std::vector<size_t>::iterator end,
        size_t max)
{
    if (index != end)
    {
        (*index) ++;
        if ((*index) >= max)
        {
            // Hit the max value, adjust the next slot
            if (!increment_index(index+1, end, max-1))
            {
                return false;
            }
            (*index) = (*(index+1)) + 1;
        }
        return true;
    }
    return false;
}
} // namespace

std::vector<Tuner*> Lineup::_minimal_covering()
{
    // Lock must be acquired before calling this method.
    // The _tuners std::set cannot be indexed by position, so copy to a vector.
    std::vector<Tuner*> tuners;
    std::copy(_tuners.begin(), _tuners.end(), std::back_inserter(tuners));

    std::vector<size_t> index;
    bool matched;
    for (int num_tuners = 1; num_tuners <= (int)tuners.size(); num_tuners ++)
    {
        // The index values will be incremented starting at begin().
        // Create index, reverse order
        index.clear();
        for (int i=0; i<num_tuners; i++)
        {
            index.insert(index.begin(), i);
        }

        matched = true;
        do {
            // index contains a combination of num_tuners entries.
            // This loop is entered for each unique combination of tuners,
            // until all channels are matched by at least one tuner in the list.
            matched = true;
            for (auto& number: _lineup)
            {
                auto& info = _info[number];

                bool tunermatch = false;
                for (auto idx: index) {
                    auto tuner = tuners[idx];

                    if (info.HasTuner(tuner))
                    {
                        tunermatch = true;
                        break;
                    }
                }
                if (!tunermatch)
                {
                    matched = false;
                    break;
                }
            }
        } while (!matched && increment_index(index.begin(), index.end(), tuners.size()));
        if (matched)
            break;
    }
    std::vector<Tuner*> retval;

    if (matched)
    {
        std::string idx;
        for (auto& i : index) {
            char buf[10];
            sprintf(buf, " %08x", tuners[i]->DeviceID());
            idx += buf;
            retval.push_back(tuners[i]);
        }
        KODI_LOG(LOG_DEBUG, "UpateGuide - Need to scan %u tuner(s) - %s", index.size(), idx.c_str());
    }
    else
    {
        KODI_LOG(LOG_INFO, "UpdateGuide - Found no tuners!");
    }
    return retval;
}

bool Lineup::_age_out()
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

bool Lineup::_insert_json_guide_data(const Json::Value& jsontunerguide, const Tuner* tuner)
{
    if (jsontunerguide.type() != Json::arrayValue)
    {
        KODI_LOG(LOG_ERROR, "Top-level JSON guide data is not an array for %08x", tuner->DeviceID());
        return {};
    }

    bool new_guide_entries{false};

    for (auto& jsonchannelguide : jsontunerguide)
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
            KODI_LOG(LOG_ERROR, "Guide entries is not an array for %08x", tuner->DeviceID());
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

bool Lineup::_insert_guide_data(const GuideNumber* number, const Tuner* tuner, time_t start)
{
    std::string URL{"http://my.hdhomerun.com/api/guide.php?DeviceAuth="};
    URL.append(EncodeURL(tuner->Auth()));

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
            tuner->DeviceID(), URL.c_str());

    std::string guidedata;
    if (!GetFileContents(URL, guidedata))
    {
        KODI_LOG(LOG_ERROR, "Error requesting guide for %08x from %s",
                tuner->DeviceID(), URL.c_str());
        return {};
    }
    if (guidedata.substr(0,4) == "null")
        return {};

    Json::Reader jsonreader;
    Json::Value  jsontunerguide;
    if (!jsonreader.parse(guidedata, jsontunerguide))
    {
        KODI_LOG(LOG_ERROR, "Error parsing JSON guide data for %08x", tuner->DeviceID());
        return {};
    }
    return _insert_json_guide_data(jsontunerguide, tuner);
}

bool Lineup::_update_guide_basic()
{
    // Find a minimal covering of the lineup, to avoid duplicate guide requests.
    Lock lock(this);

    std::vector<Tuner*> tuners = _minimal_covering();
    if (tuners.size() == 0)
        return false;

    bool added{false};

    for (auto tuner: tuners) {
        bool n = _insert_guide_data(nullptr, tuner);
        added |= n;
    }

    return added;
}

bool Lineup::_update_guide_extended(const GuideNumber& gn, time_t start)
{
    Lock lock(this);

    const auto& info = _info[gn];
    auto tuner = info.GetFirstTuner();

    if (!tuner)
        return false;

    return _insert_guide_data(&gn, tuner, start);
}

bool Lineup::_guide_contains(time_t t)
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
void Lineup::UpdateGuide()
{
    Lock guidelock(_guide_lock);

    time_t now     = time(nullptr);
    if (!_guide_contains(now) || !_guide_contains(now + g.Settings.guideBasicInterval))
    {
        std::cout << "Stale guide, basic update\n";
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

            std::this_thread::sleep_for(std::chrono::milliseconds(100));

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

int Lineup::PvrGetChannelsAmount()
{
    return _lineup.size();
}
PVR_ERROR Lineup::PvrGetChannels(ADDON_HANDLE handle, bool radio)
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
            // Lineup name from tuner
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



PVR_ERROR Lineup::PvrGetEPGForChannel(ADDON_HANDLE handle,
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

int Lineup::PvrGetChannelGroupsAmount()
{
    return 3;
}

static const std::string FavoriteChannels = "Favorite channels";
static const std::string HDChannels       = "HD channels";
static const std::string SDChannels       = "SD channels";


PVR_ERROR Lineup::PvrGetChannelGroups(ADDON_HANDLE handle, bool bRadio)
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

PVR_ERROR Lineup::PvrGetChannelGroupMembers(ADDON_HANDLE handle,
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

bool Lineup::_open_tcp_stream(const std::string& url)
{
    if (_filehandle)
    {
         CloseLiveStream();
         // sets _filehandle = nullptr
    }
    if (url.size())
    {
        _filehandle = g.XBMC->OpenFile(url.c_str(), 0);
    }

    KODI_LOG(LOG_DEBUG, "Attempt to tune TCP stream from url %s : %s",
            url.c_str(),
            _filehandle == nullptr ? "Fail":"Success");

    return _filehandle != nullptr;
}

bool Lineup::OpenLiveStream(const PVR_CHANNEL& channel)
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
    info.ResetNextTuner();

    Tuner* tuner;
    while ((tuner = info.GetPreferredTuner()) != nullptr)
    {
        if (_open_tcp_stream(info.DlnaURL(tuner)))
            return true;
    }

    info.ResetNextTuner();
    while ((tuner = info.GetNextTuner()) != nullptr)
    {
        if (_open_tcp_stream(info.DlnaURL(tuner)))
            return true;
    }

    return false;
}
void Lineup::CloseLiveStream(void)
{
    Lock strlock(_stream_lock);
    Lock lock(this);

    g.XBMC->CloseFile(_filehandle);
    _filehandle = nullptr;
}
int Lineup::ReadLiveStream(unsigned char* buffer, unsigned int size)
{
    Lock strlock(_stream_lock);

    if (_filehandle)
    {
        return g.XBMC->ReadFile(_filehandle, buffer, size);
    }
    return 0;
}

}; // namespace PVRHDHomeRun
