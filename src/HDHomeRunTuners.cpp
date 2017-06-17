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
#include "HDHomeRunTuners.h"
#include <chrono>
#include <thread>
#include <functional>
#include <algorithm>
#include <iterator>
#include <string>
#include <sstream>
#include <iterator>
#include <numeric>

using namespace ADDON;


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

    std::set<uint32_t> discovered_ids;

    bool tuner_added   = false;
    bool tuner_removed = false;

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
    }

    // Iterate through tuners, Refresh and determine if there are stale entries.
    for (auto tuner : _tuners)
    {
        uint32_t id = tuner->DeviceID();
        if (discovered_ids.find(id) == discovered_ids.end())
        {
            // Tuner went away
            tuner_removed = true;
            KODI_LOG(LOG_DEBUG, "Removing tuner %08x", id);

            auto ptuner = const_cast<Tuner*>(tuner);

            for (auto number : _lineup)
            {
                auto info = _info[number];
                if (info.RemoveTuner(tuner))
                {
                    KODI_LOG(LOG_DEBUG, "Removed tuner from GuideNumber %s", number.extendedName().c_str());
                }
                if (info.TunerCount() == 0)
                {
                    // No tuners left for this lineup guide entry, remove it
                    KODI_LOG(LOG_DEBUG, "No tuners left, removing GuideNumber %s", number.extendedName().c_str());
                    _lineup.erase(number);
                    _info.erase(number);
                    _guide.erase(number);
                }
            }

            // Erase tuner from this
            _tuners.erase(tuner);
            _device_ids.erase(id);
            delete(tuner);
        }
        else
        {
            tuner->Refresh();
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

        auto ptuner = const_cast<Tuner*>(tuner);
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

        KODI_LOG(LOG_DEBUG,
                "Lineup Entry: %d.%d - %s - %s - %s",
                number._channel,
                number._subchannel,
                number._guidenumber.c_str(),
                number._guidename.c_str(),
                tuners.c_str()
        );

        if (prior.find(number) == prior.end())
            added = true;
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
    Lock lock(this);
    Lock guidelock(_guide_lock);

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

GuideEntryStatus Lineup::_insert_json_guide_data(const Json::Value& jsontunerguide, const Tuner* tuner)
{
    if (jsontunerguide.type() != Json::arrayValue)
    {
        KODI_LOG(LOG_ERROR, "Top-level JSON guide data is not an array for %08x", tuner->DeviceID());
        return {};
    }

    GuideEntryStatus status;

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
        for (auto& jsonentry: jsonguidenetries)
        {
            GuideEntry entry{jsonentry};
            auto s = channelguide.InsertEntry(entry);

            EPG_TAG tag = entry.Epg_Tag(number);

            std::cout << "Sending EpgEventStateChange for channel " << number
                    << " ID " << tag.iUniqueBroadcastId
                    << "\n";
            g.PVR->EpgEventStateChange(&tag, number, EPG_EVENT_CREATED);


            //if (s.Flag()) {
            //    g.PVR->TriggerEpgUpdate(number);
            //}
            status.Merge(s);
        }
    }
    return status;
}

GuideEntryStatus Lineup::_insert_guide_data(const GuideNumber* number, const Tuner* tuner, time_t start)
{
    std::string URL{"http://my.hdhomerun.com/api/guide.php?DeviceAuth="};
    URL.append(EncodeURL(tuner->Auth()));

    if (number)
    {
        URL.append("&Channel=");
        URL.append(number->toString());

        if (start)
        {
            char start_s[64];
            sprintf(start_s, "%d", start);
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

    bool added = false;
    GuideEntryStatus status;

    for (auto tuner: tuners) {
        auto s = _insert_guide_data(nullptr, tuner);
        status.Merge(s);
    }

    return status.Flag();
}

bool Lineup::_update_guide_extended(time_t start)
{
    GuideEntryStatus status;

    for (auto& ng: _guide)
    {
        uint32_t number = ng.first;
        auto&    guide = ng.second;
        GuideNumber gn{number};

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        Lock lock(this);

        const auto& info = _info[number];
        auto tuner = info.GetFirstTuner();

        if (!tuner)
            continue;

        auto s = _insert_guide_data(&gn, tuner, start);
        status.Merge(s);
    }

    return status.Flag();
}

bool Lineup::_guide_contains(time_t t)
{
    bool   contains = true;
    bool   haveany  = false;

    for (auto& ng: _guide)
    {
        uint32_t number = ng.first;
        auto&    guide = ng.second;

        if (guide._times.Empty())
            continue;

        if (!guide._times.Contains(t))
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

    _age_out();

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

        }
    }
    /*



    bool   haveany = bounds.Flag();
    time_t start = bounds.Times().Start();
    time_t end   = bounds.Times().End();

    time_t target = now + g.Settings.guideBasicInterval;

    bool stale = (!haveany) || (start > now) || (end < target);
    std::cout << "guide "
            << " now: "    << format_time(now)
            << " stale: "  << stale
            << " haveany: "<< haveany
            << " start: "  << format_time(start)
            << " end: "    << format_time(end)
            << " target: " << format_time(target)
            << "\n";

    if (stale)
    {
        _update_guide_basic();
        return;
    }

    if (g.Settings.extendedGuide)
    {
        if (end < now + g.Settings.guideExtendedTrigger)
        {
            std::cout << "Extended late from " << format_time(end) << "\n";
            _update_guide_extended(end);
        }

        if (start > now - g.Settings.guideExtendedTrigger)
        {
            time_t begin = start - g.Settings.guideExtendedEach;
            std::cout << "Extended early from " << format_time(begin) << "\n";
            _update_guide_extended(begin);
        }
    }
    */
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
            name = &guide._affiliate;
        }
        if (!name || !name->length() || (g.Settings.channelName == SettingsType::GUIDE_NAME))
        {
            // Lineup name from guide
            name = &guide._guidename;
        }
        if (!name || !name->length() || (g.Settings.channelName == SettingsType::TUNER_NAME))
        {
            // Lineup name from tuner
            name = &info._guidename;
        }
        PVR_STRCPY(pvrChannel.strChannelName, name->c_str());
        PVR_STRCPY(pvrChannel.strIconPath, guide._imageURL.c_str());

        g.PVR->TransferChannelEntry(handle, &pvrChannel);
    }
    return PVR_ERROR_NO_ERROR;
}



PVR_ERROR Lineup::PvrGetEPGForChannel(ADDON_HANDLE handle,
        const PVR_CHANNEL& channel, time_t start, time_t end
        )
{
    return PVR_ERROR_NOT_IMPLEMENTED;

    KODI_LOG(LOG_DEBUG,
            "PvrGetEPCForChannel Handle:%p Channel ID: %d Number: %u Sub: %u Start: %u End: %u",
            handle,
            channel.iUniqueId,
            channel.iChannelNumber,
            channel.iSubChannelNumber,
            start,
            end
    );

    Lock lock(this);
    Lock guidelock(_guide_lock);

    auto& guide = _guide[channel.iUniqueId];

    auto& requests = guide._requests;
    auto& times    = guide._times;
    if (!times.Contains(start) && !times.Contains(end))
    {
        requests.Add({start, end});
    }
    else if (!times.Contains(start))
    {
        requests.Add({start, times.Start()});
    }
    else if (!times.Contains(end))
    {
        requests.Add({times.End(), end});
    }

    for (auto& ge: guide._entries)
    {
        if (ge._endtime < start)
            continue;
        if (ge._starttime > end)
            break;
        if (ge._transferred)
            continue;

        EPG_TAG tag = ge.Epg_Tag(channel.iUniqueId);
        g.PVR->TransferEpgEntry(handle, &tag);

        ge._transferred = true;
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

std::string Lineup::DlnaURL(const PVR_CHANNEL& channel)
{
    Lock lock(this);

    auto  id    = channel.iUniqueId;
    const auto& entry = _lineup.find(id);
    if (entry == _lineup.end())
    {
        KODI_LOG(LOG_ERROR, "Channel %d not found!", id);
        return "";
    }
    auto& info = _info[id];
    Tuner* tuner =  info.GetNextTuner();
    return info.DlnaURL(tuner);
}

bool Lineup::OpenLiveStream(const PVR_CHANNEL& channel)
{
    Lock strlock(_stream_lock);
    Lock lock(this);

    if (_filehandle)
    {
        CloseLiveStream();
    }
    int pass = 0;
    do
    {
        std::string URL = DlnaURL(channel);
        if (URL.size())
        {
            _filehandle = g.XBMC->OpenFile(URL.c_str(), 0);
        }
        else
        {
            pass ++;
        }
        if (_filehandle)
            break;
    } while (pass < 2);

    return _filehandle != nullptr;
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
