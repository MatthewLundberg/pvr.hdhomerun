/*
 *      Copyright (C) 2017-2020 Matthew Lundberg <matthew.k.lundberg@gmail.com>
 *      https://github.com/MatthewLundberg/pvr.hdhomerun
 *
 *      Copyright (c) 2017 Michael G. Brehm
 *      https://github.com/djp952/pvr.hdhomerundvr
 *
 *      Copyright (C) 2015 Zoltan Csizmadia <zcsizmadia@gmail.com>
 *      https://github.com/zcsizmadia/pvr.hdhomerun
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

#include <cstring>
#include <string>
#include <p8-platform/threads/threads.h>
#include "PVR_HDHR.h"
#include "Utils.h"
#include "Lockable.h"
#include <iterator>
#include <sstream>
#include <algorithm>
#include <iostream>

namespace PVRHDHomeRun
{

GlobalsType g;

namespace {
template<typename T>
std::vector<T> split_vec(const std::string& s)
{
    std::stringstream ss(s);
    std::istream_iterator<T> begin(ss);
    std::istream_iterator<T> end;
    std::vector<T> svec(begin, end);
    return svec;
}
template<>
std::vector<uint32_t> split_vec(const std::string& s)
{
    std::vector<uint32_t> ivec;
    auto svec = split_vec<std::string>(s);
    std::transform(svec.begin(), svec.end(), std::back_inserter(ivec),
            [](const std::string& s) {return std::stoul(s, 0, 16);}
    );
    return ivec;
}

template<typename T>
std::set<T> split_set(const std::string& s)
{
    auto vec = split_vec<T>(s);
    std::set<T> sset;
    std::move(vec.begin(), vec.end(), std::inserter(sset, sset.end()));
    return sset;
}

void GetSetting(const std::string& name, int& i)
{
    i = kodi::GetSettingInt(name);
}
void GetSetting(const std::string& name, std::string& s)
{
    s = kodi::GetSettingString(name);
}
void GetSetting(const std::string& name, bool& b)
{
    b = kodi::GetSettingBoolean(name);
}
void GetSetting(const std::string& name, float& f)
{
    f = kodi::GetSettingFloat(name);
}


template<typename T>
void readvalue(const std::string& name, T& t)
{
    GetSetting(name, t);
}
template<>
void readvalue<std::vector<uint32_t>>(const std::string& name, std::vector<uint32_t>& t)
{
    auto value = kodi::GetSettingString(name);
    t = split_vec<uint32_t>(std::move(value));
}
template<>
void readvalue<std::vector<std::string>>(const std::string& name, std::vector<std::string>& t)
{
    auto value = kodi::GetSettingString(name);
    t = split_vec<std::string>(std::move(value));
}
template<>
void readvalue<std::set<uint32_t>>(const std::string& name, std::set<uint32_t>& t)
{
    auto value = kodi::GetSettingString(name);
    t = split_set<uint32_t>(std::move(value));
}
template<>
void readvalue<std::set<std::string>>(const std::string& name, std::set<std::string>& t)
{
    auto value = kodi::GetSettingString(name);
    t = split_set<std::string>(std::move(value));
}
} // namespace

void SettingsType::SetProtocol(const std::string& proto)
{
    if (proto == "TCP")
    {
        protocol = SettingsType::TCP;
    }
    else if (proto == "UDP")
    {
        protocol = SettingsType::UDP;
    }
}

bool SettingsType::ReadSettings(void)
{
    readvalue("hide_protected", hideProtectedChannels);
    readvalue("debug",          debugLog);
    readvalue("hide_unknown",   hideUnknownChannels);
    readvalue("use_legacy",     useLegacyDevices);
    readvalue("extended",       extendedGuide);
    readvalue("guidedays",      guideDays);
    readvalue("port",           udpPort);
    readvalue("record",         record);
    readvalue("recordforlive",  recordforlive);
    readvalue("preferred",      preferredDevice);
    readvalue("blacklist",      blacklistDevice);
    readvalue("hide_ch_no",     hiddenChannels);
    readvalue("use_stream_url", use_stream_url);

    auto cn = kodi::GetSettingInt("channel_name");
    channelName = static_cast<CHANNEL_NAME>(cn);
    auto protocol = kodi::GetSettingString("protocol");
    SetProtocol(protocol);
    return true;
}



namespace {

void castsetting(bool& b, const kodi::CSettingValue& value)
{
    b = value.GetBoolean();
}
void castsetting(int& t, const kodi::CSettingValue& value)
{
    t = value.GetInt();
}
void castsetting(std::string& s, const kodi::CSettingValue& value)
{
    s = value.GetString();
}

template<typename T>
bool setvalue(T& t, const std::string& text, const std::string& name, const kodi::CSettingValue& value)
{
    if (text == name)
    {
        castsetting(t, value);
        return true;
    }
    return false;
}
}

using namespace PVRHDHomeRun;

void SettingsType::SetChannelName(int name)
{
    switch(name)
    {
    case 1:
        channelName = SettingsType::TUNER_NAME;
        break;
    case 2:
        channelName = SettingsType::GUIDE_NAME;
        break;
    case 3:
        channelName = SettingsType::AFFILIATE;
        break;
    }
}

ADDON_STATUS SettingsType::SetSetting(const std::string& name, const kodi::CSettingValue& value)
{
    if (setvalue(hideProtectedChannels, "hide_protected", name, value))
        return ADDON_STATUS_NEED_RESTART;

    if (setvalue(debugLog, "debug", name, value))
        return ADDON_STATUS_OK;

    if (setvalue(useLegacyDevices, "use_legacy", name, value))
        return ADDON_STATUS_NEED_RESTART;

    if (setvalue(hideUnknownChannels, "hide_unknown", name, value))
        return ADDON_STATUS_NEED_RESTART;

    if (setvalue(udpPort, "port", name, value))
        return ADDON_STATUS_OK;

    if (setvalue(extendedGuide, "extended", name, value))
        return ADDON_STATUS_OK;

    if (setvalue(guideDays, "guidedays", name, value))
        return ADDON_STATUS_OK;

    if (setvalue(record, "record", name, value))
        return ADDON_STATUS_OK;

    if (setvalue(recordforlive, "recordforlive", name, value))
        return ADDON_STATUS_OK;

    if (setvalue(use_stream_url, "use_stream_url", name, value))
        return ADDON_STATUS_NEED_RESTART;

    if (name == "channel_name")
    {
        SetChannelName(value.GetInt());
        return ADDON_STATUS_NEED_RESTART;
    }
    else if (name == "protocol")
    {
        SetProtocol(value.GetString());
        return ADDON_STATUS_NEED_RESTART;
    }
    else if (name == "preferred")
    {
        preferredDevice = split_vec<uint32_t>(value.GetString());
    }
    else if (name == "blackist")
    {
        blacklistDevice = split_set<uint32_t>(value.GetString());
    }
    else if (name == "hide_ch_no")
    {
        hiddenChannels = split_set<std::string>(value.GetString());
    }

    return ADDON_STATUS_OK;
}




// Timer definition is copied from djp952
//
// duplicate_prevention
//
// Defines the identifiers for series duplicate prevention values
enum duplicate_prevention {

    none                    = 0,
    newonly                 = 1,
    recentonly              = 2,
};

// timer_type
//
// Defines the identifiers for the various timer types (1-based)
enum timer_type {

    seriesrule              = 1,
    datetimeonlyrule        = 2,
    epgseriesrule           = 3,
    epgdatetimeonlyrule     = 4,
    seriestimer             = 5,
    datetimeonlytimer       = 6,
};// g_timertypes (const)

//
// Array of PVR_TIMER_TYPE structures to pass to Kodi
static const PVR_TIMER_TYPE g_timertypes[] ={

    // timer_type::seriesrule
    //
    // Timer type for non-EPG series rules, requires a series name match operation to create. Also used when editing
    // an existing recording rule as the EPG/seriesid information will not be available
    {
        // iID
        timer_type::seriesrule,

        // iAttributes
        PVR_TIMER_TYPE_IS_REPEATING | PVR_TIMER_TYPE_SUPPORTS_CHANNELS | PVR_TIMER_TYPE_SUPPORTS_TITLE_EPG_MATCH | PVR_TIMER_TYPE_SUPPORTS_RECORD_ONLY_NEW_EPISODES |
            PVR_TIMER_TYPE_SUPPORTS_START_END_MARGIN | PVR_TIMER_TYPE_FORBIDS_EPG_TAG_ON_CREATE | PVR_TIMER_TYPE_SUPPORTS_ANY_CHANNEL,

        // strDescription
        "Record Series Rule",

        0, { { 0, "" } }, 0,        // priorities
        0, { { 0, "" } }, 0,        // lifetimes

        // preventDuplicateEpisodes
        3, {
            { duplicate_prevention::none, "Record all episodes" },
            { duplicate_prevention::newonly, "Record only new episodes" },
            { duplicate_prevention::recentonly, "Record only recent episodes" }
        }, 0,

        0, { { 0, "" } }, 0,        // recordingGroup
        0, { { 0, "" } }, 0,        // maxRecordings
    },

    // timer_type::datetimeonlyrule
    //
    // Timer type for non-EPG date time only rules, requires a series name match operation to create. Also used when editing
    // an existing recording rule as the EPG/seriesid information will not be available
    //
    // TODO: Made read-only since there is no way to get it to display the proper date selector.  Making it one-shot or manual
    // rather than repeating removes it from the Timer Rules area and causes other problems.  If Kodi allowed the date selector
    // to be displayed I think that would suffice, and wouldn't be that difficult or disruptive to the Kodi code.  For now, the
    // PVR_TIMER_TYPE_SUPPORTS_FIRST_DAY flag was added to show the date of the recording.  Unfortunately, this also means that
    // the timer rule cannot be deleted, which sucks.
    {
        // iID
        timer_type::datetimeonlyrule,

        // iAttributes
        PVR_TIMER_TYPE_IS_REPEATING | PVR_TIMER_TYPE_IS_READONLY | PVR_TIMER_TYPE_SUPPORTS_CHANNELS | PVR_TIMER_TYPE_SUPPORTS_TITLE_EPG_MATCH |
            PVR_TIMER_TYPE_SUPPORTS_FIRST_DAY | PVR_TIMER_TYPE_SUPPORTS_START_TIME | PVR_TIMER_TYPE_SUPPORTS_START_END_MARGIN |
            PVR_TIMER_TYPE_FORBIDS_EPG_TAG_ON_CREATE,

        // strDescription
        "Record Once Rule",

        0, { { 0, "" } }, 0,        // priorities
        0, { { 0, "" } }, 0,        // lifetimes
        0, { { 0, "" } }, 0,        // preventDuplicateEpisodes
        0, { { 0, "" } }, 0,        // recordingGroup
        0, { { 0, "" } }, 0,        // maxRecordings
    },

    // timer_type::epgseriesrule
    //
    // Timer type for EPG series rules
    {
        // iID
        timer_type::epgseriesrule,

        // iAttributes
        //
        // todo: PVR_TIMER_TYPE_REQUIRES_EPG_SERIESLINK_ON_CREATE can be set here, but seems to have bugs right now, after Kodi
        // is stopped and restarted, the cached EPG data prevents adding a new timer if this is set
        PVR_TIMER_TYPE_IS_REPEATING | PVR_TIMER_TYPE_SUPPORTS_CHANNELS | PVR_TIMER_TYPE_SUPPORTS_RECORD_ONLY_NEW_EPISODES |
            PVR_TIMER_TYPE_SUPPORTS_START_END_MARGIN | PVR_TIMER_TYPE_REQUIRES_EPG_SERIES_ON_CREATE | PVR_TIMER_TYPE_SUPPORTS_ANY_CHANNEL,

        // strDescription
        "Record Series",

        0, { { 0, "" } }, 0,        // priorities
        0, { { 0, "" } }, 0,        // lifetimes

        // preventDuplicateEpisodes
        3, {
            { duplicate_prevention::none, "Record all episodes" },
            { duplicate_prevention::newonly, "Record only new episodes" },
            { duplicate_prevention::recentonly, "Record only recent episodes" }
        }, 0,

        0, { { 0, "" } }, 0,        // recordingGroup
        0, { { 0, "" } }, 0,        // maxRecordings
    },

    // timer_type::epgdatetimeonlyrule
    //
    // Timer type for EPG date time only rules
    {
        // iID
        timer_type::epgdatetimeonlyrule,

        // iAttributes
        //
        // todo: PVR_TIMER_TYPE_REQUIRES_EPG_SERIESLINK_ON_CREATE can be set here, but seems to have bugs right now, after Kodi
        // is stopped and restarted, the cached EPG data prevents adding a new timer if this is set
        PVR_TIMER_TYPE_SUPPORTS_CHANNELS | PVR_TIMER_TYPE_SUPPORTS_START_END_MARGIN | PVR_TIMER_TYPE_REQUIRES_EPG_SERIES_ON_CREATE,

        // strDescription
        "Record Once",

        0, { { 0, "" } }, 0,        // priorities
        0, { { 0, "" } }, 0,        // lifetimes
        0, { { 0, "" } }, 0,        // preventDuplicateEpisodes
        0, { { 0, "" } }, 0,        // recordingGroup
        0, { { 0, "" } }, 0,        // maxRecordings
    },

    // timer_type::seriestimer
    //
    // used for existing episode timers; these cannot be edited or deleted by the end user
    {
        // iID
        timer_type::seriestimer,

        // iAttributes
        PVR_TIMER_TYPE_IS_READONLY | PVR_TIMER_TYPE_FORBIDS_NEW_INSTANCES | PVR_TIMER_TYPE_SUPPORTS_CHANNELS | PVR_TIMER_TYPE_SUPPORTS_START_TIME |
            PVR_TIMER_TYPE_SUPPORTS_END_TIME,

        // strDescription
        "Record Series Episode",

        0, { {0, "" } }, 0,         // priorities
        0, { {0, "" } }, 0,         // lifetimes
        0, { {0, "" } }, 0,         // preventDuplicateEpisodes
        0, { {0, "" } }, 0,         // recordingGroup
        0, { {0, "" } }, 0,         // maxRecordings
    },

    // timer_type::datetimeonlytimer
    //
    // used for existing date/time only episode timers; these cannot be edited by the user, but allows the
    // timer and it's associated parent rule to be deleted successfully via the live TV interface
    {
        // iID
        timer_type::datetimeonlytimer,

        // iAttributes
        PVR_TIMER_TYPE_FORBIDS_NEW_INSTANCES | PVR_TIMER_TYPE_SUPPORTS_CHANNELS | PVR_TIMER_TYPE_SUPPORTS_START_TIME | PVR_TIMER_TYPE_SUPPORTS_END_TIME,

        // strDescription
        "Record Once Episode",

        0, { {0, "" } }, 0,         // priorities
        0, { {0, "" } }, 0,         // lifetimes
        0, { {0, "" } }, 0,         // preventDuplicateEpisodes
        0, { {0, "" } }, 0,         // recordingGroup
        0, { {0, "" } }, 0,         // maxRecordings
    },
};

//---------------------------------------------------------------------------
// GetTimerTypes
//
// Retrieve the timer types supported by the backend
//
// Arguments:
//
//  types       - The function has to write the definition of the supported timer types into this array
//  count       - in: The maximum size of the list; out: the actual size of the list

PVR_ERROR GetTimerTypes(PVR_TIMER_TYPE types[], int* count)
{
    if(count == nullptr) return PVR_ERROR::PVR_ERROR_INVALID_PARAMETERS;
    if((*count) && (types == nullptr)) return PVR_ERROR::PVR_ERROR_INVALID_PARAMETERS;

    // Only copy up to the maximum size of the array provided by the caller
    *count = std::min(*count, static_cast<int>(std::extent<decltype(g_timertypes)>::value));
    for(int index = 0; index < *count; index++) types[index] = g_timertypes[index];

    return PVR_ERROR::PVR_ERROR_NO_ERROR;
}

} // namespace

