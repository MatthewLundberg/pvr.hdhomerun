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

namespace PVRHDHomeRun {

class PVR_HDHR : public HasTunerSet<PVR_HDHR>
{
public:
    PVR_HDHR() = default;
    virtual ~PVR_HDHR();

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

    PVR_ERROR GetChannels(ADDON_HANDLE handle, bool bRadio);
    int       GetChannelsAmount();
    PVR_ERROR GetEPGForChannel(ADDON_HANDLE handle,
                  const PVR_CHANNEL& channel,
                  time_t iStart,
                  time_t iEnd);
    int       GetChannelGroupsAmount(void);
    PVR_ERROR GetChannelGroups(ADDON_HANDLE handle, bool bRadio);
    PVR_ERROR GetChannelGroupMembers(ADDON_HANDLE handle, const PVR_CHANNEL_GROUP &group);

    bool OpenLiveStream(const PVR_CHANNEL& channel);
    void CloseLiveStream(void);
    int ReadLiveStream(unsigned char* buffer, unsigned int size);
    long long SeekLiveStream(long long position, int whence);
    long long LengthLiveStream();
    bool IsRealTimeStream();
    bool SeekTime(double time,bool backwards,double* startpts);
    PVR_ERROR GetStreamTimes(PVR_STREAM_TIMES *times);
    PVR_ERROR SignalStatus(PVR_SIGNAL_STATUS &signalStatus);
    bool CanPauseStream(void);
    bool CanSeekStream(void);
    PVR_ERROR GetChannelStreamProperties(const PVR_CHANNEL* channel, PVR_NAMED_VALUE* properties, unsigned int* iPropertiesCount);
    PVR_ERROR GetDriveSpace(long long *iTotal, long long *iUsed);
    PVR_ERROR GetStreamProperties(PVR_STREAM_PROPERTIES*);

    bool OpenRecordedStream(const PVR_RECORDING& rec);
    void CloseRecordedStream(void);
    int ReadRecordedStream(unsigned char* buf, unsigned int len);
    long long SeekRecordedStream(long long pos, int whence);
    long long LengthRecordedStream(void);
    PVR_ERROR GetRecordingStreamProperties(const PVR_RECORDING*, PVR_NAMED_VALUE*, unsigned int*);
    PVR_ERROR DeleteRecording(const PVR_RECORDING&);
    PVR_ERROR GetRecordings(ADDON_HANDLE, bool deleted);
    int GetRecordingsAmount(bool deleted);
    PVR_ERROR RenameRecording(const PVR_RECORDING&);
    PVR_ERROR GetRecordingEdl(const PVR_RECORDING&, PVR_EDL_ENTRY[], int* size);
    PVR_ERROR SetRecordingPlayCount(const PVR_RECORDING&, int count);
    int GetRecordingLastPlayedPosition(const PVR_RECORDING&);
    PVR_ERROR SetRecordingLastPlayedPosition(const PVR_RECORDING&, int);
    PVR_ERROR SetRecordingLifetime(const PVR_RECORDING*);
    PVR_ERROR DeleteAllRecordingsFromTrash();
    PVR_ERROR UndeleteRecording(const PVR_RECORDING&);

    PVR_ERROR AddTimer(const PVR_TIMER&);
    PVR_ERROR DeleteTimer(const PVR_TIMER&, bool);
    int GetTimersAmount(void);
    PVR_ERROR GetTimers(ADDON_HANDLE);
    PVR_ERROR UpdateTimer(const PVR_TIMER&);

    void PauseStream(bool bPaused);
    void SetSpeed(int);
    bool IsTimeshifting(void);

    PVR_ERROR SetEPGTimeFrame(int days) { return PVR_ERROR_NOT_IMPLEMENTED; }
    PVR_ERROR IsEPGTagPlayable(const EPG_TAG*, bool*) { return PVR_ERROR_NOT_IMPLEMENTED; }
    PVR_ERROR IsEPGTagRecordable(const EPG_TAG*, bool*) { return PVR_ERROR_NOT_IMPLEMENTED; }
    PVR_ERROR GetEPGTagStreamProperties(const EPG_TAG*, PVR_NAMED_VALUE*, unsigned int* count) { return PVR_ERROR_NOT_IMPLEMENTED; }
    PVR_ERROR GetEPGTagEdl(const EPG_TAG* epgTag, PVR_EDL_ENTRY edl[], int *size) { return PVR_ERROR_NOT_IMPLEMENTED; }
    PVR_ERROR GetStreamReadChunkSize(int* chunksize) { return PVR_ERROR_NOT_IMPLEMENTED; }

private:
    void  _age_out(time_t);
    bool  _guide_contains(time_t);
    void  _insert_json_guide_data(const Json::Value&, const char* idstr);
    void  _fetch_guide_data(const uint32_t* = nullptr, time_t start=0);

    virtual bool _open_stream(const PVR_CHANNEL& channel) { return false; };
    virtual bool _open_stream(const PVR_RECORDING& recording) { return false; };
    virtual int  _read_stream(unsigned char* buffer, unsigned int size) = 0;
    virtual void _close_stream() = 0;
    virtual int64_t _seek_stream(int64_t position, int whence);
    virtual int64_t _length_stream();
protected:
    bool  _open_tcp_stream(const std::string&);

protected:
    std::set<uint32_t>        _device_ids;
    std::set<std::string>     _storage_urls;
    std::set<GuideNumber>     _lineup;
    std::map<uint32_t, Info>  _info;
    std::map<uint32_t, Guide> _guide;
    Recording                 _recording;
    uint32_t                  _sessionid = 0;
    size_t                    _filesize = 0;

    bool                      _using_sd_record = false;
    time_t                    _starttime = 0;
    time_t                    _endtime   = 0;

#if 0
    size_t _length   = 0;
    size_t _duration = 0;
    size_t _bps      = 0;
#endif

public:
    std::set<TunerDevice*>    _tuner_devices;
    std::set<StorageDevice*>  _storage_devices;
    StorageDevice*            _current_storage = nullptr;
    const Entry*              _current_entry   = nullptr;
    bool                      _live_stream = false;
protected:
    Lockable _guide_lock;
    Lockable _pvr_lock;
    Lockable _stream_lock;
    void* _filehandle = nullptr;
};

class PVR_HDHR_TCP : public PVR_HDHR {
private:
    bool  _open_stream(const PVR_CHANNEL& channel) override;
    int   _read_stream(unsigned char* buffer, unsigned int size) override;
    void  _close_stream() override;
};
class PVR_HDHR_UDP : public PVR_HDHR {
private:
    bool  _open_stream(const PVR_CHANNEL& channel) override;
    int   _read_stream(unsigned char* buffer, unsigned int size) override;
    void  _close_stream() override;
};


PVR_HDHR* PVR_HDHR_Factory(int protocol);

}; // namespace PVRHDHomeRun
