/*
 *      Copyright (C) 2017-2018 Matthew Lundberg <matthew.k.lundberg@gmail.com>
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

#include "Addon.h"

#include <cstring>
#include <string>
#include <p8-platform/threads/threads.h>
#include <xbmc_pvr_dll.h>
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

class UpdateThread: public P8PLATFORM::CThread, Lockable
{
    time_t _lastDiscover = 0;
    time_t _lastLineup   = 0;
    time_t _lastGuide    = 0;
    time_t _lastRecord   = 0;

    bool   _running      = false;

public:
    void Wake()
    {
        Lock lock(this);

        _lastDiscover = 0;
        _lastLineup   = 0;
        _lastGuide    = 0;
        _lastRecord   = 0;
        _running      = false;
    }
    void *Process()
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
                    //KODI_LOG(LOG_DEBUG, "Local IP: %s %s", FormatIP(info.ip_addr).c_str(), FormatIP(info.subnet_mask).c_str());
                    if (!IPSubnetMatch(localhost, info.ip_addr, info.subnet_mask))
                    {
                        num_networks ++;
                    }
                }

                if (num_networks != prev_num_networks)
                {
                    if (num_networks == 0)
                    {
                        KODI_LOG(LOG_DEBUG, "UpdateThread::Process No external networks found, waiting.");
                    }
                    else
                    {
                        for (int i=0; i<ip_info_count; i++)
                        {
                            KODI_LOG(LOG_DEBUG, "UpdateThread::Process IP %s %s",
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

            time_t discover, lineup, guide, recordings;
            {
                Lock lock(this);
                discover   = _lastDiscover;
                lineup     = _lastLineup;
                guide      = _lastGuide;
                recordings = _lastRecord;
            }

            if (g.pvr_hdhr)
            {
                if (now >= discover + g.Settings.deviceDiscoverInterval)
                {
                    bool discovered = g.pvr_hdhr->DiscoverTunerDevices();
                    if (discovered)
                    {
                        KODI_LOG(LOG_DEBUG, "PVR::DiscoverDevices returned true, try again");
                        now = 0;
                        state = 0;
                    }
                    else
                    {
                        state = 1;
                    }
                    updateDiscover = true;
                }
                else if (state == 1 || now >= recordings + g.Settings.recordUpdateInterval)
                {
                    state = 0;

                    if (now >= recordings + g.Settings.recordUpdateInterval)
                    {
                        if (g.pvr_hdhr->UpdateRecordings())
                            g.PVR->TriggerRecordingUpdate();

                        updateRecord = true;
                    }
                }
                else if (state == 1 || now >= lineup + g.Settings.lineupUpdateInterval)
                {
                    if (g.pvr_hdhr->UpdateLineup())
                    {
                        state = 2;
                        g.PVR->TriggerChannelUpdate();
                        g.PVR->TriggerChannelGroupsUpdate();
                    }
                    else
                    {
                        state = 0;
                    }
                    updateLineup = true;
                }
                else if (state == 2 || now >= guide + g.Settings.guideUpdateInterval)
                {
                    state = 0;

                    g.pvr_hdhr->UpdateGuide();
                    updateGuide = true;
                }
            }

            if (updateDiscover || updateLineup || updateGuide || updateRecord)
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
            }
        }
        return nullptr;
    }
};

UpdateThread g_UpdateThread;
}; // namespace

using namespace PVRHDHomeRun;

void SetChannelName(int name)
{
    if (g.XBMC == nullptr)
        return;

    switch(name)
    {
    case 1:
        g.Settings.channelName = SettingsType::TUNER_NAME;
        break;
    case 2:
        g.Settings.channelName = SettingsType::GUIDE_NAME;
        break;
    case 3:
        g.Settings.channelName = SettingsType::AFFILIATE;
        break;
    }
}
void SetProtocol(const char* proto)
{
    if (strcmp(proto, "TCP") == 0)
    {
        g.Settings.protocol = SettingsType::TCP;
    }
    else if (strcmp(proto, "UDP") == 0)
    {
        g.Settings.protocol = SettingsType::UDP;
    }
}

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

// g.XBMC is non-null when readvalue is called.
template<typename T>
void readvalue(const char* name, T& t)
{
    g.XBMC->GetSetting(name, &t);
}
template<>
void readvalue<std::vector<uint32_t>>(const char*name, std::vector<uint32_t>& t)
{
    char value[10240];
    g.XBMC->GetSetting(name, value);
    t = split_vec<uint32_t>(value);
}
template<>
void readvalue<std::vector<std::string>>(const char* name, std::vector<std::string>& t)
{
    char value[10240];
    g.XBMC->GetSetting(name, value);
    t = split_vec<std::string>(value);
}
template<>
void readvalue<std::set<uint32_t>>(const char* name, std::set<uint32_t>& t)
{
    char value[10240];
    g.XBMC->GetSetting(name, value);
    t = split_set<uint32_t>(value);
}
template<>
void readvalue<std::set<std::string>>(const char* name, std::set<std::string>& t)
{
    char value[10240];
    g.XBMC->GetSetting(name, value);
    t = split_set<std::string>(value);
}
}

extern "C"
{

void ADDON_ReadSettings(void)
{
    if (g.XBMC == nullptr)
        return;

    readvalue("hide_protected", g.Settings.hideProtectedChannels);
    readvalue("mark_new",       g.Settings.markNewProgram);
    readvalue("debug",          g.Settings.debugLog);
    readvalue("hide_unknown",   g.Settings.hideUnknownChannels);
    readvalue("use_legacy",     g.Settings.useLegacyDevices);
    readvalue("extended",       g.Settings.extendedGuide);
    readvalue("guidedays",      g.Settings.guideDays);
    readvalue("channel_name",   g.Settings.channelName);
    readvalue("port",           g.Settings.udpPort);
    readvalue("record",         g.Settings.record);
    readvalue("recordforlive",  g.Settings.recordforlive);
    readvalue("preferred",      g.Settings.preferredDevice);
    readvalue("blacklist",      g.Settings.blacklistDevice);
    readvalue("hide_ch_no",     g.Settings.hiddenChannels);

    char protocol[64] = "TCP";
    g.XBMC->GetSetting("protocol", protocol);
    SetProtocol(protocol);
}

ADDON_STATUS ADDON_Create(void* hdl, void* props)
{
    if (!hdl || !props)
        return ADDON_STATUS_UNKNOWN;

    PVR_PROPERTIES* pvrprops = (PVR_PROPERTIES*) props;

    g.XBMC = new ADDON::CHelper_libXBMC_addon;
    if (!g.XBMC->RegisterMe(hdl))
    {
        delete(g.XBMC); g.XBMC = nullptr;
        return ADDON_STATUS_PERMANENT_FAILURE;
    }

    g.PVR = new CHelper_libXBMC_pvr;
    if (!g.PVR->RegisterMe(hdl))
    {
        delete(g.PVR);  g.PVR = nullptr;
        delete(g.XBMC); g.XBMC = nullptr;
        return ADDON_STATUS_PERMANENT_FAILURE;
    }

    KODI_LOG(LOG_NOTICE, "%s - Creating the PVR HDHomeRun add-on",
            __FUNCTION__);

    g.currentStatus = ADDON_STATUS_UNKNOWN;
    g.userPath = pvrprops->strUserPath;
    g.clientPath = pvrprops->strClientPath;

    ADDON_ReadSettings();

    KODI_LOG(LOG_DEBUG, "Creating new-style Lineup");
    g.pvr_hdhr = PVR_HDHR_Factory(g.Settings.protocol);

    if (g.pvr_hdhr == nullptr)
    {
        return ADDON_STATUS_PERMANENT_FAILURE;
    }
    KODI_LOG(LOG_DEBUG, "Done with new-style Lineup");

    if (g.pvr_hdhr)
    {
        g.pvr_hdhr->Update();
        g_UpdateThread.CreateThread(false);
    }

    g.currentStatus = ADDON_STATUS_OK;
    g.isCreated = true;

    return ADDON_STATUS_OK;
}

ADDON_STATUS ADDON_GetStatus()
{
    return g.currentStatus;
}

void ADDON_Destroy()
{
    g_UpdateThread.StopThread();

    delete(g.pvr_hdhr); g.pvr_hdhr = nullptr;
    delete(g.PVR);      g.PVR = nullptr;
    delete(g.XBMC);     g.XBMC = nullptr;

    g.isCreated = false;
    g.currentStatus = ADDON_STATUS_UNKNOWN;
}

bool ADDON_HasSettings()
{
    return true;
}
} // extern "C"

namespace {
template<typename T>
bool setvalue(T& t, const char* text, const char* name, const void* value)
{
    if (strcmp(text, name) == 0)
    {
        t = *(T*) value;
        return true;
    }
    return false;
}
} // namespace

extern "C" {
ADDON_STATUS ADDON_SetSetting(const char *name, const void *value)
{
    if (g.pvr_hdhr == nullptr)
        return ADDON_STATUS_OK;

    if (setvalue(g.Settings.hideProtectedChannels, "hide_protected", name, value))
        return ADDON_STATUS_NEED_RESTART;

    if (setvalue(g.Settings.markNewProgram, "mark_new", name, value))
        return ADDON_STATUS_OK;

    if (setvalue(g.Settings.debugLog, "debug", name, value))
        return ADDON_STATUS_OK;

    if (setvalue(g.Settings.useLegacyDevices, "use_legacy", name, value))
        return ADDON_STATUS_NEED_RESTART;

    if (setvalue(g.Settings.hideUnknownChannels, "hide_unknown", name, value))
        return ADDON_STATUS_NEED_RESTART;

    if (setvalue(g.Settings.udpPort, "port", name, value))
        return ADDON_STATUS_OK;

    if (setvalue(g.Settings.extendedGuide, "extended", name, value))
        return ADDON_STATUS_OK;

    if (setvalue(g.Settings.guideDays, "guidedays", name, value))
        return ADDON_STATUS_OK;

    if (setvalue(g.Settings.record, "record", name, value))
        return ADDON_STATUS_OK;

    if (setvalue(g.Settings.recordforlive, "recordforlive", name, value))
        return ADDON_STATUS_OK;

    if (strcmp(name, "channel_name") == 0)
    {
        SetChannelName(*(int*) value);
        return ADDON_STATUS_NEED_RESTART;
    }
    else if (strcmp(name, "protocol") == 0)
    {
        SetProtocol((char*) value);
        return ADDON_STATUS_NEED_RESTART;
    }
    else if (strcmp(name, "preferred") == 0)
    {
        g.Settings.preferredDevice = split_vec<uint32_t>((char*) value);
    }
    else if (strcmp(name, "blackist") == 0)
    {
        g.Settings.blacklistDevice = split_set<uint32_t>((char*) value);
    }
    else if (strcmp(name, "hide_ch_no") == 0)
    {
        g.Settings.hiddenChannels = split_set<std::string>((char*) value);
    }

    return ADDON_STATUS_OK;
}

/***********************************************************
 * PVR Client AddOn specific public library functions
 ***********************************************************/

void OnSystemSleep()
{
}

void OnSystemWake()
{
    g_UpdateThread.Wake();

    if (g.pvr_hdhr && g.PVR)
    {
        g.pvr_hdhr->Update();
        g.PVR->TriggerChannelUpdate();
    }
}

void OnPowerSavingActivated()
{
}

void OnPowerSavingDeactivated()
{
}

PVR_ERROR GetAddonCapabilities(PVR_ADDON_CAPABILITIES* pCapabilities)
{
    pCapabilities->bSupportsEPG                      = true;
    pCapabilities->bSupportsEPGEdl                   = false;
    pCapabilities->bSupportsTV                       = true;
    pCapabilities->bSupportsRadio                    = false;
    pCapabilities->bSupportsRecordings               = g.Settings.record;
    pCapabilities->bSupportsRecordingsUndelete       = false;
    pCapabilities->bSupportsTimers                   = g.Settings.record;
    pCapabilities->bSupportsChannelGroups            = g.Settings.usegroups;
    pCapabilities->bSupportsChannelScan              = false;
    pCapabilities->bSupportsChannelSettings          = false;
    pCapabilities->bHandlesInputStream               = true;
    pCapabilities->bHandlesDemuxing                  = false;
    pCapabilities->bSupportsRecordingPlayCount       = false;
    pCapabilities->bSupportsLastPlayedPosition       = true;
    pCapabilities->bSupportsRecordingEdl             = false;
    pCapabilities->bSupportsRecordingsRename         = false;
    pCapabilities->bSupportsRecordingsLifetimeChange = false;
    pCapabilities->bSupportsDescrambleInfo           = false;
    pCapabilities->iRecordingsLifetimesSize          = 0;

    return PVR_ERROR_NO_ERROR;
}

const char *GetBackendName(void)
{
    return "otherkids PVR";
}

const char *GetBackendVersion(void)
{
    return "4.0.3";
}

const char *GetConnectionString(void)
{
    return "connected";
}

const char *GetBackendHostname(void)
{
    return "";
}

// Pass-through to PVR object
#define PVR_VOID_0(name)         void name(void)         { if (g.pvr_hdhr) g.pvr_hdhr->name(); }
#define PVR_VOID_1(name, t0)     void name(t0 v0)        { if (g.pvr_hdhr) g.pvr_hdhr->name(v0); }
#define PVR_VOID_2(name, t0, t1) void name(t0 v0, t1 v1) { if (g.pvr_hdhr) g.pvr_hdhr->name(v0, v1); }

#define PVR_RETR_0(name, typ, def)                 typ name(void)                       { return g.pvr_hdhr ? g.pvr_hdhr->name()               : def; }
#define PVR_RETR_1(name, typ, def, t0)             typ name(t0 v0)                      { return g.pvr_hdhr ? g.pvr_hdhr->name(v0)             : def; }
#define PVR_RETR_2(name, typ, def, t0, t1)         typ name(t0 v0, t1 v1)               { return g.pvr_hdhr ? g.pvr_hdhr->name(v0, v1)         : def; }
#define PVR_RETR_3(name, typ, def, t0, t1, t2)     typ name(t0 v0, t1 v1, t2 v2)        { return g.pvr_hdhr ? g.pvr_hdhr->name(v0, v1, v2)     : def; }
#define PVR_RETR_4(name, typ, def, t0, t1, t2, t3) typ name(t0 v0, t1 v1, t2 v2, t3 v3) { return g.pvr_hdhr ? g.pvr_hdhr->name(v0, v1, v2, v3) : def; }

#define PVR_ERR_0(name)                 PVR_RETR_0(name, PVR_ERROR, PVR_ERROR_SERVER_ERROR)
#define PVR_ERR_1(name, t0)             PVR_RETR_1(name, PVR_ERROR, PVR_ERROR_SERVER_ERROR, t0)
#define PVR_ERR_2(name, t0, t1)         PVR_RETR_2(name, PVR_ERROR, PVR_ERROR_SERVER_ERROR, t0, t1)
#define PVR_ERR_3(name, t0, t1, t2)     PVR_RETR_3(name, PVR_ERROR, PVR_ERROR_SERVER_ERROR, t0, t1, t2)
#define PVR_ERR_4(name, t0, t1, t2, t3) PVR_RETR_4(name, PVR_ERROR, PVR_ERROR_SERVER_ERROR, t0, t1, t2, t3)


PVR_ERR_2(GetDriveSpace,           long long *, long long *)
PVR_ERR_4(GetEPGForChannel,        ADDON_HANDLE, const PVR_CHANNEL&, time_t, time_t)
PVR_RETR_0(GetChannelsAmount,      int,           -1)
PVR_ERR_2(GetChannels,             ADDON_HANDLE, bool)
PVR_RETR_0(GetChannelGroupsAmount, int,           -1)
PVR_ERR_2(GetChannelGroups,        ADDON_HANDLE, bool)
PVR_ERR_2(GetChannelGroupMembers,  ADDON_HANDLE, const PVR_CHANNEL_GROUP &)
PVR_RETR_1(OpenLiveStream,         bool,          false,                  const PVR_CHANNEL &)
PVR_VOID_0(CloseLiveStream)
PVR_RETR_2(ReadLiveStream,         int,           0,                      unsigned char *, unsigned int)
PVR_ERR_1(SignalStatus,            PVR_SIGNAL_STATUS&)
PVR_RETR_0(CanPauseStream,         bool,          false)
PVR_RETR_0(CanSeekStream,          bool,          false)
PVR_ERR_3(GetChannelStreamProperties, const PVR_CHANNEL*, PVR_NAMED_VALUE*, unsigned int*)

// LiveStream
PVR_RETR_0(LengthLiveStream,    long long, -1)
PVR_RETR_2(SeekLiveStream,      long long, v0, long long, int)
PVR_RETR_3(SeekTime,            bool,      false, double, bool, double*)
PVR_RETR_0(IsRealTimeStream,    bool,      false)
PVR_ERR_1(GetStreamProperties,  PVR_STREAM_PROPERTIES*)
PVR_ERR_1(GetStreamTimes,       PVR_STREAM_TIMES*)

// Recording
PVR_RETR_1(OpenRecordedStream,             bool,      false, const PVR_RECORDING&);
PVR_VOID_0(CloseRecordedStream)
PVR_RETR_2(ReadRecordedStream,             int,       0,     unsigned char*, unsigned int);
PVR_RETR_2(SeekRecordedStream,             long long, v0,    long long, int)
PVR_RETR_0(LengthRecordedStream,           long long, 0)
PVR_ERR_3(GetRecordingStreamProperties,    const PVR_RECORDING*, PVR_NAMED_VALUE*, unsigned int*)
PVR_ERR_1(DeleteRecording,                 const PVR_RECORDING&)
PVR_ERR_2(GetRecordings,                   ADDON_HANDLE, bool)
PVR_RETR_1(GetRecordingsAmount,            int,       -1, bool)
PVR_ERR_1(RenameRecording,                 const PVR_RECORDING&)
PVR_ERR_3(GetRecordingEdl,                 const PVR_RECORDING&, PVR_EDL_ENTRY*, int*)
PVR_ERR_2(SetRecordingPlayCount,           const PVR_RECORDING&, int)
PVR_RETR_1(GetRecordingLastPlayedPosition, int,       -1, const PVR_RECORDING&)
PVR_ERR_2(SetRecordingLastPlayedPosition,  const PVR_RECORDING&, int)
PVR_ERR_1(SetRecordingLifetime,            const PVR_RECORDING*)
PVR_ERR_0(DeleteAllRecordingsFromTrash)
PVR_ERR_1(UndeleteRecording,               const PVR_RECORDING&)

// Timers
PVR_ERR_1(AddTimer,                        const PVR_TIMER&)
PVR_ERR_2(DeleteTimer,                     const PVR_TIMER&, bool)
PVR_RETR_0(GetTimersAmount,                int, -1);
PVR_ERR_1(GetTimers,                       ADDON_HANDLE);
PVR_ERR_1(UpdateTimer,                     const PVR_TIMER&)

// Timeshift
PVR_VOID_1(PauseStream,    bool)
PVR_VOID_1(SetSpeed,       int)
PVR_RETR_0(IsTimeshifting, bool, false)

// EPG
PVR_ERR_1(SetEPGTimeFrame,            int)
PVR_ERR_2(IsEPGTagPlayable,           const EPG_TAG*, bool*)
PVR_ERR_2(IsEPGTagRecordable,         const EPG_TAG*, bool*)
PVR_ERR_3(GetEPGTagStreamProperties,  const EPG_TAG*, PVR_NAMED_VALUE*, unsigned int*)
PVR_ERR_3(GetEPGTagEdl,               const EPG_TAG*, PVR_EDL_ENTRY*, int*)
PVR_ERR_1(GetStreamReadChunkSize,     int*)



/* UNUSED API FUNCTIONS */
PVR_ERROR CallMenuHook(const PVR_MENUHOOK&, const PVR_MENUHOOK_DATA&) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR GetDescrambleInfo(PVR_DESCRAMBLE_INFO*) { return PVR_ERROR_NOT_IMPLEMENTED; }
// Channel
PVR_ERROR OpenDialogChannelScan(void) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR OpenDialogChannelSettings(const PVR_CHANNEL&) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR OpenDialogChannelAdd(const PVR_CHANNEL&) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR DeleteChannel(const PVR_CHANNEL&) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR RenameChannel(const PVR_CHANNEL&) { return PVR_ERROR_NOT_IMPLEMENTED; }
// Demux
void DemuxAbort(void) {}
void DemuxFlush(void) {}
DemuxPacket* DemuxRead(void) { return NULL; }
void DemuxReset(void) {}





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


} // extern "C"

