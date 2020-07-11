#pragma once
/*
 *      Copyright (C) 2017-2019 Matthew Lundberg <matthew.k.lundberg@gmail.com>
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

#include <json/json.h>
#include <cstring>
#include <vector>
#include <set>
#include <memory>
#include <string>
#include <mutex>
#include <tuple>
#include "Lockable.h"
#include "IntervalSet.h"
#include "Guide.h"
#include "Utils.h"
#include "Info.h"
#include "Recording.h"

#include <kodi/addon-instance/PVR.h>
#include <kodi/Filesystem.h>
#include <p8-platform/threads/mutex.h>
#include <p8-platform/threads/threads.h>

#define NO_FILE_CACHE 1

namespace PVRHDHomeRun {

class ATTRIBUTE_HIDDEN PVR_HDHR_TUNER;
class ATTRIBUTE_HIDDEN UpdateThread;

class ATTRIBUTE_HIDDEN PVR_HDHR
    : public kodi::addon::CAddonBase
    , public kodi::addon::CInstancePVRClient
{
public:
    PVR_HDHR() = default;

    ADDON_STATUS Create() override;
    ADDON_STATUS SetSetting(const std::string& settingName, const kodi::CSettingValue& settingValue) override;
    PVR_ERROR OnSystemWake() override;

    PVR_ERROR GetCapabilities(kodi::addon::PVRCapabilities& capabilities) override;
    PVR_ERROR GetBackendName(std::string& name) override;
    PVR_ERROR GetBackendVersion(std::string& version) override;
    PVR_ERROR GetConnectionString(std::string& connection) override;
    PVR_ERROR GetDriveSpace(uint64_t& total, uint64_t& used) override;

    bool DiscoverTunerDevices();
    bool UpdateLineup();
    bool UpdateRecordings();
    void UpdateGuide();
    bool UpdateRules();

    bool Update()
    {
        bool newDevice  = DiscoverTunerDevices();
        bool newLineup = UpdateLineup();
        UpdateGuide();
        bool newRules = UpdateRules();

        return newDevice || newLineup || newRules;
    }
    void AddLineupEntry(const Json::Value&, TunerDevice*);

    PVR_ERROR GetChannels(bool bRadio, kodi::addon::PVRChannelsResultSet& results) override;
    PVR_ERROR GetChannelsAmount(int& amount) override;
    PVR_ERROR GetEPGForChannel(int channel, time_t iStart, time_t iEnd, kodi::addon::PVREPGTagsResultSet& results) override;
    PVR_ERROR GetChannelGroupsAmount(int& amount) override;
    PVR_ERROR GetChannelGroups(bool bRadio, kodi::addon::PVRChannelGroupsResultSet& groups) override;
    PVR_ERROR GetChannelGroupMembers(const kodi::addon::PVRChannelGroup& group, kodi::addon::PVRChannelGroupMembersResultSet& results) override;

    bool OpenLiveStream(const kodi::addon::PVRChannel& channel) override;
    void CloseLiveStream(void) override;
    int ReadLiveStream(unsigned char* buffer, unsigned int size) override;
    int64_t SeekLiveStream(int64_t position, int whence) override;
    int64_t LengthLiveStream() override;
    bool IsRealTimeStream() override;
    bool SeekTime(double time,bool backwards,double& startpts) override;
    PVR_ERROR GetStreamTimes(kodi::addon::PVRStreamTimes& times) override;
    PVR_ERROR GetSignalStatus(int channelUid, kodi::addon::PVRSignalStatus& signalStatus) override;
    bool CanPauseStream(void) override;
    bool CanSeekStream(void) override;
    PVR_ERROR GetChannelStreamProperties(const kodi::addon::PVRChannel& channel, std::vector<kodi::addon::PVRStreamProperty>& properties) override;
    PVR_ERROR GetStreamProperties(std::vector<kodi::addon::PVRStreamProperties>& properties) override;

    bool OpenRecordedStream(const kodi::addon::PVRRecording& recording) override;
    void CloseRecordedStream(void) override;
    int ReadRecordedStream(unsigned char* buf, unsigned int len) override;
    int64_t SeekRecordedStream(int64_t position, int whence) override;
    int64_t LengthRecordedStream(void) override;
    PVR_ERROR GetRecordingStreamProperties(const kodi::addon::PVRRecording& recording, std::vector<kodi::addon::PVRStreamProperty>& properties) override;
    PVR_ERROR DeleteRecording(const kodi::addon::PVRRecording& recording) override;
    PVR_ERROR GetRecordings(bool deleted, kodi::addon::PVRRecordingsResultSet& results) override;
    PVR_ERROR GetRecordingsAmount(bool deleted, int& amount) override;
    PVR_ERROR RenameRecording(const kodi::addon::PVRRecording& recording) override;
    PVR_ERROR GetRecordingEdl(const kodi::addon::PVRRecording& recording, std::vector<kodi::addon::PVREDLEntry>& edl) override;
    PVR_ERROR SetRecordingPlayCount(const kodi::addon::PVRRecording& recording, int count) override;
    PVR_ERROR GetRecordingLastPlayedPosition(const kodi::addon::PVRRecording& recording, int& position) override;
    PVR_ERROR SetRecordingLastPlayedPosition(const kodi::addon::PVRRecording& recording, int lastplayedposition) override;
    PVR_ERROR SetRecordingLifetime(const kodi::addon::PVRRecording& recording) override;
    PVR_ERROR DeleteAllRecordingsFromTrash() override;
    PVR_ERROR UndeleteRecording(const kodi::addon::PVRRecording& recording) override;

    PVR_ERROR AddTimer(const kodi::addon::PVRTimer& timer) override;
    PVR_ERROR DeleteTimer(const kodi::addon::PVRTimer& timer, bool forceDelete) override;
    PVR_ERROR GetTimersAmount(int& amount) override;
    PVR_ERROR GetTimers(kodi::addon::PVRTimersResultSet& results) override;
    PVR_ERROR UpdateTimer(const kodi::addon::PVRTimer& timer) override;

    void PauseStream(bool bPaused) override;
    void SetSpeed(int) override;

    //PVR_ERROR SetEPGTimeFrame(int days) override { return PVR_ERROR_NOT_IMPLEMENTED; }
    //PVR_ERROR IsEPGTagPlayable(const kodi::addon::PVREPGTag& tag, bool& isPlayable) override { return PVR_ERROR_NOT_IMPLEMENTED; }
    //PVR_ERROR IsEPGTagRecordable(const kodi::addon::PVREPGTag& tag, bool& isRecordable) override { return PVR_ERROR_NOT_IMPLEMENTED; }
    //PVR_ERROR GetEPGTagStreamProperties(const kodi::addon::PVREPGTag& tag, std::vector<kodi::addon::PVRStreamProperty>& properties) override { return PVR_ERROR_NOT_IMPLEMENTED; }
    //PVR_ERROR GetEPGTagEdl(const kodi::addon::PVREPGTag& tag, std::vector<kodi::addon::PVREDLEntry>& edl) override { return PVR_ERROR_NOT_IMPLEMENTED; }
    //PVR_ERROR GetStreamReadChunkSize(int& chunksize) override { return PVR_ERROR_NOT_IMPLEMENTED; }

private:
    std::unique_ptr<UpdateThread> _updatethread;
    void  _age_out(time_t);
    bool  _guide_contains(time_t);
    void  _insert_json_guide_data(const Json::Value&, const char* idstr);
    void  _fetch_guide_data(const uint32_t* = nullptr, time_t start=0);

    int64_t _seek_stream(int64_t position, int whence);
    int64_t _length_stream();

    std::unique_ptr<PVR_HDHR_TUNER> _tuner;

protected:
    std::set<GuideNumber>     _lineup;
    std::map<uint32_t, Info>  _info;
    std::map<uint32_t, Guide> _guide;
    Recording                 _recording;

protected:
    Lockable _guide_lock;
    Lockable _pvr_lock;

friend class PVR_HDHR_TUNER;
friend class PVR_HDHR_TCP;
};

class ATTRIBUTE_HIDDEN PVR_HDHR_TUNER : public HasTunerSet<PVR_HDHR_TUNER>
{
public:
    PVR_HDHR_TUNER(PVR_HDHR* p)
        : _parent(p)
    {}
    virtual ~PVR_HDHR_TUNER();
    bool    open_stream(const kodi::addon::PVRChannel& channel) { return _open_stream(channel); }
    bool    open_stream(const kodi::addon::PVRRecording& recording);
    int     read_stream(unsigned char* buffer, unsigned int size) { return _read_stream(buffer,size); }
    void    close_stream() { _close_stream(); }
    bool    open_tcp_stream(const std::string&, bool live);
    int64_t seek_stream(int64_t position, int whence);
    int64_t length_stream();
    PVR_ERROR get_stream_times(kodi::addon::PVRStreamTimes& times);
    bool    can_pause_stream();
    bool    can_seek_stream();
    bool    discover_tuner_devices();
    PVR_ERROR get_channel_stream_properties(const kodi::addon::PVRChannel& channel, std::vector<kodi::addon::PVRStreamProperty>& properties);
    
    size_t filesize() { return _filesize; }
    bool   using_sd_record() { return _using_sd_record; }
    time_t starttime() { return _starttime; }
    time_t endtime() { return _endtime; }
    bool   live_stream() { return _live_stream; }

private:
    virtual bool  _open_stream(const kodi::addon::PVRChannel& channel) = 0;
    virtual int   _read_stream(unsigned char* buffer, unsigned int size) = 0;
    virtual void  _close_stream() = 0;

public:
    std::set<TunerDevice*>    _tuner_devices;
    std::set<StorageDevice*>  _storage_devices;
    StorageDevice*            _current_storage = nullptr;
    const Entry*              _current_entry   = nullptr;

protected:
    size_t   _filesize = 0;
    bool     _using_sd_record = false;
    time_t   _starttime = 0;
    time_t   _endtime   = 0;
    bool     _live_stream = false;
    uint32_t _sessionid = 0;

#if NO_FILE_CACHE
    size_t _length   = 0;
    size_t _duration = 0;
    size_t _bps      = 0;
#endif

    std::set<uint32_t>        _device_ids;
    std::set<std::string>     _storage_urls;

    Lockable _stream_lock;

    std::unique_ptr<kodi::vfs::CFile> _filehandle;
    PVR_HDHR* _parent;
};

class ATTRIBUTE_HIDDEN PVR_HDHR_TCP : public PVR_HDHR_TUNER
{
public:
    PVR_HDHR_TCP(PVR_HDHR* p)
        : PVR_HDHR_TUNER(p)
    {}
private:
    bool  _open_stream(const kodi::addon::PVRChannel& channel) override;
    int   _read_stream(unsigned char* buffer, unsigned int size) override;
    void  _close_stream() override;
};
class ATTRIBUTE_HIDDEN PVR_HDHR_UDP : public PVR_HDHR_TUNER
{
public:
    PVR_HDHR_UDP(PVR_HDHR* p)
        : PVR_HDHR_TUNER(p)
    {}
private:
    bool  _open_stream(const kodi::addon::PVRChannel& channel) override;
    int   _read_stream(unsigned char* buffer, unsigned int size) override;
    void  _close_stream() override;
};


PVR_HDHR_TUNER* PVR_HDHR_Factory(int protocol);

}; // namespace PVRHDHomeRun
