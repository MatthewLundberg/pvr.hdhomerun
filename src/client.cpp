/*
 *      Copyright (C) 2017 Matthew Lundberg <matthew.k.lundberg@gmail.com>
 *      https://github.com/MatthewLundberg/pvr.hdhomerun
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

#include "client.h"

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

    bool   _running      = false;

public:
    void Wake()
    {
        Lock lock(this);

        _lastDiscover = 0;
        _lastLineup   = 0;
        _lastGuide    = 0;
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

            if (g.pvr_hdhr)
            {
                bool changed = false;

                if (now >= _lastDiscover + g.Settings.deviceDiscoverInterval)
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
                else if (state == 1 || now >= _lastLineup + g.Settings.lineupUpdateInterval)
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
                else if (state == 2 || now >= _lastGuide + g.Settings.guideUpdateInterval)
                {
                    state = 0;
                    g.pvr_hdhr->UpdateGuide();
                    updateGuide = true;
                }
                else
                {
                    state = 0;
                }
            }

            if (updateDiscover || updateLineup || updateGuide)
            {
                Lock lock(this);

                if (updateDiscover)
                    _lastDiscover = now;
                if (updateLineup)
                    _lastLineup = now;
                if (updateGuide)
                    _lastGuide = now;
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

std::vector<uint32_t> split_vec(const std::string& s) {
	std::stringstream ss(s);
	std::istream_iterator<std::string> begin(ss);
	std::istream_iterator<std::string> end;
	std::vector<std::string> svec(begin, end);
	std::vector<uint32_t> ivec;
	std::transform(svec.begin(), svec.end(), std::back_inserter(ivec),
			[](const std::string& s) {return std::stoul(s, 0, 16);}
	);
	return ivec;
}
std::set<uint32_t> split_set(const std::string& s) {
	auto vec = split_vec(s);
	std::set<uint32_t> sset(vec.begin(), vec.end());
	return sset;
}

}

extern "C"
{

void ADDON_ReadSettings(void)
{
    if (g.XBMC == nullptr)
        return;

    g.XBMC->GetSetting("hide_protected", &g.Settings.hideProtectedChannels);
    g.XBMC->GetSetting("mark_new",       &g.Settings.markNewProgram);
    g.XBMC->GetSetting("debug",          &g.Settings.debugLog);
    g.XBMC->GetSetting("hide_unknown",   &g.Settings.hideUnknownChannels);
    g.XBMC->GetSetting("use_legacy",     &g.Settings.useLegacyDevices);
    g.XBMC->GetSetting("extended",       &g.Settings.extendedGuide);
    g.XBMC->GetSetting("guidedays",      &g.Settings.guideDays);
    g.XBMC->GetSetting("channel_name",   &g.Settings.channelName);
    g.XBMC->GetSetting("port",           &g.Settings.udpPort);
    g.XBMC->GetSetting("record",         &g.Settings.record);
    g.XBMC->GetSetting("recordforlive",  &g.Settings.recordforlive);

    char preferred[1024] = "";
    g.XBMC->GetSetting("preferred",      preferred);
    g.Settings.preferredDevice = split_vec(preferred);

    char blacklist[1024] = "";
    g.XBMC->GetSetting("blacklist",      blacklist);
    g.Settings.blacklistDevice = split_set(blacklist);

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
        g.Settings.preferredDevice = split_vec((char*) value);
    }
    else if (strcmp(name, "blackist") == 0)
    {
        g.Settings.blacklistDevice = split_set((char*) value);
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
    return "4.0.0";
}

const char *GetConnectionString(void)
{
    return "connected";
}

const char *GetBackendHostname(void)
{
    return "";
}

PVR_ERROR GetDriveSpace(long long *iTotal, long long *iUsed)
{
    // TODO - fix this
    *iTotal = 1024 * 1024 * 1024;
    *iUsed = 0;
    return PVR_ERROR_NO_ERROR;
}

PVR_ERROR GetEPGForChannel(ADDON_HANDLE handle, const PVR_CHANNEL& channel,
        time_t iStart, time_t iEnd)
{
    return g.pvr_hdhr ?
            g.pvr_hdhr->PvrGetEPGForChannel(handle, channel, iStart, iEnd) :
            PVR_ERROR_SERVER_ERROR;
}

int GetChannelsAmount(void)
{
    return g.pvr_hdhr ? g.pvr_hdhr->PvrGetChannelsAmount() : PVR_ERROR_SERVER_ERROR;
}

PVR_ERROR GetChannels(ADDON_HANDLE handle, bool bRadio)
{
    return g.pvr_hdhr ?
            g.pvr_hdhr->PvrGetChannels(handle, bRadio) : PVR_ERROR_SERVER_ERROR;
}

int GetChannelGroupsAmount(void)
{
    return g.pvr_hdhr ?
            g.pvr_hdhr->PvrGetChannelGroupsAmount() : PVR_ERROR_SERVER_ERROR;
}

PVR_ERROR GetChannelGroups(ADDON_HANDLE handle, bool bRadio)
{
    return g.pvr_hdhr ?
            g.pvr_hdhr->PvrGetChannelGroups(handle, bRadio) :
            PVR_ERROR_SERVER_ERROR;
}

PVR_ERROR GetChannelGroupMembers(ADDON_HANDLE handle,
        const PVR_CHANNEL_GROUP &group)
{
    return g.pvr_hdhr ?
            g.pvr_hdhr->PvrGetChannelGroupMembers(handle, group) :
            PVR_ERROR_SERVER_ERROR;
}

bool OpenLiveStream(const PVR_CHANNEL &channel)
{
    return g.pvr_hdhr ?
            g.pvr_hdhr->OpenLiveStream(channel) :
            false;
}

void CloseLiveStream(void)
{
    if (g.pvr_hdhr)
        g.pvr_hdhr->CloseLiveStream();
}

PVR_ERROR SignalStatus(PVR_SIGNAL_STATUS &signalStatus)
{
    PVR_STRCPY(signalStatus.strAdapterName, "otherkids PVR");
    PVR_STRCPY(signalStatus.strAdapterStatus, "OK");

    return PVR_ERROR_NO_ERROR;
}

bool CanPauseStream(void)
{
    return true;
}

bool CanSeekStream(void)
{
    return true;
}

int ReadLiveStream(unsigned char *pBuffer, unsigned int iBufferSize)
{
    return g.pvr_hdhr ? g.pvr_hdhr->ReadLiveStream(pBuffer, iBufferSize) : 0;
}

PVR_ERROR GetChannelStreamProperties(const PVR_CHANNEL* channel, PVR_NAMED_VALUE* properties, unsigned int* iPropertiesCount)
{
  *iPropertiesCount = 0;

  return PVR_ERROR_NO_ERROR;
}

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
// LiveStream
long long LengthLiveStream(void) { return -1; }
long long SeekLiveStream(long long, int) { return -1; }
bool SeekTime(double,bool,double*) { return false; }
bool IsRealTimeStream(void) { return true; }
PVR_ERROR GetStreamProperties(PVR_STREAM_PROPERTIES*) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR GetStreamTimes(PVR_STREAM_TIMES *times) { return PVR_ERROR_NOT_IMPLEMENTED; }
// Recording
bool OpenRecordedStream(const PVR_RECORDING&) { return false; }
void CloseRecordedStream(void) {}
int ReadRecordedStream(unsigned char*, unsigned int) { return 0; }
long long SeekRecordedStream(long long, int) { return 0; }
long long LengthRecordedStream(void) { return 0; }
PVR_ERROR GetRecordingStreamProperties(const PVR_RECORDING*, PVR_NAMED_VALUE*, unsigned int*) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR DeleteRecording(const PVR_RECORDING&) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR GetRecordings(ADDON_HANDLE, bool) { return PVR_ERROR_NOT_IMPLEMENTED; }
int GetRecordingsAmount(bool) { return -1; }
PVR_ERROR RenameRecording(const PVR_RECORDING&) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR GetRecordingEdl(const PVR_RECORDING&, PVR_EDL_ENTRY[], int*) { return PVR_ERROR_NOT_IMPLEMENTED; };
PVR_ERROR SetRecordingPlayCount(const PVR_RECORDING&, int) { return PVR_ERROR_NOT_IMPLEMENTED; }
int GetRecordingLastPlayedPosition(const PVR_RECORDING&) { return -1; }
PVR_ERROR SetRecordingLastPlayedPosition(const PVR_RECORDING&, int) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR SetRecordingLifetime(const PVR_RECORDING*) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR DeleteAllRecordingsFromTrash() { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR UndeleteRecording(const PVR_RECORDING&) { return PVR_ERROR_NOT_IMPLEMENTED; }
// Timers
PVR_ERROR AddTimer(const PVR_TIMER&) { return PVR_ERROR_NO_ERROR; }
PVR_ERROR DeleteTimer(const PVR_TIMER&, bool) { return PVR_ERROR_NOT_IMPLEMENTED; }
int GetTimersAmount(void) { return -1; }
PVR_ERROR GetTimers(ADDON_HANDLE) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR GetTimerTypes(PVR_TIMER_TYPE types[], int*) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR UpdateTimer(const PVR_TIMER&) { return PVR_ERROR_NOT_IMPLEMENTED; }
// Timeshift
void PauseStream(bool bPaused) {}
void SetSpeed(int) {};
bool IsTimeshifting(void) { return false; }
// EPG
PVR_ERROR SetEPGTimeFrame(int) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR IsEPGTagPlayable(const EPG_TAG*, bool*) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR IsEPGTagRecordable(const EPG_TAG*, bool*) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR GetEPGTagStreamProperties(const EPG_TAG*, PVR_NAMED_VALUE*, unsigned int*) { return PVR_ERROR_NOT_IMPLEMENTED; }
} // extern "C"

