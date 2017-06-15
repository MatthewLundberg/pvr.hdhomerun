#pragma once
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
#include <p8-platform/threads/mutex.h>
#include <hdhomerun.h>
#include <json/json.h>
#include <cstring>
#include <vector>
#include <set>
#include <memory>
#include <string>
#include <mutex>
#include <tuple>

namespace PVRHDHomeRun {

class Lockable {
public:
    virtual ~Lockable() {}
    void LockObject() {
        _lock.Lock();
    }
    void UnlockObject() {
        _lock.Unlock();
    }
private:
    P8PLATFORM::CMutex _lock;
};

class Lock {
public:
    Lock(Lockable* obj) : _obj(obj)
        {
            _obj->LockObject();
        }
    Lock(Lockable& obj) : Lock(&obj)
    {}
    ~Lock()
    {
        _obj->UnlockObject();
    }
private:
    Lockable* _obj;
};

class Interval
{
public:
    Interval(time_t s, time_t e)
        : _start(s)
        , _end(e)
    {}
    Interval(const Interval&) = default;

    bool Contains(time_t t) const
    {
        return (t >= _start) && (t < _end);
    }
    bool operator<(const Interval& rhs) const
    {
        return _start < rhs._start;
    }

    std::string toString() const;
    operator std::string() const
    {
        return toString();
    }

    time_t _start;
    time_t _end;
};
class IntervalSet
{
public:
    IntervalSet() = default;
    IntervalSet(const Interval& i)
    {
        _intervals.insert(i);
    }

    void Add(const Interval&, bool rebalance = true);
    void Add(const IntervalSet&);

    void Remove(const Interval&);
    void Remove(const IntervalSet&);

    std::string toString() const;
    operator std::string() const
    {
        return toString();
    }

    //bool Contains(const Interval&);

    void _rebalance();
    std::set<Interval> _intervals;
};

class GuideNumber
{
private:
    static const uint32_t SubchannelLimit = 10000;
public:
    GuideNumber(const Json::Value&);
    GuideNumber(const GuideNumber&) = default;
    GuideNumber(uint32_t id)
    {
        _channel = id / SubchannelLimit;
         id %= SubchannelLimit;
         _subchannel = id;
    }
    virtual ~GuideNumber() = default;

    std::string _guidenumber;
    std::string _guidename;

    uint32_t _channel;
    uint32_t _subchannel;

    std::string extendedName() const;
    std::string toString() const;

    uint32_t ID() const
    {
        // _subchannel < 1000, _nameidx < 100
        return (_channel * SubchannelLimit) + _subchannel;
    }
    operator uint32_t() const
    {
        return ID();
    }

    bool operator<(const GuideNumber&) const;
    bool operator==(const GuideNumber&) const;
};

class GuideEntry
{
public:
    GuideEntry(const Json::Value&);

    time_t      _starttime;
    time_t      _endtime;
    time_t      _originalairdate;
    std::string _title;
    std::string _episodenumber;
    std::string _episodetitle;
    std::string _synopsis;
    std::string _imageURL;
    std::string _seriesID;
    uint32_t    _genre;
    uint32_t    _id;

    mutable bool _transferred = false;

    bool operator<(const GuideEntry& rhs) const
    {
        return _starttime < rhs._starttime;
    }
    bool operator==(const GuideEntry& rhs) const
    {
        return
                _starttime         == rhs._starttime
               && _endtime         == rhs._endtime
               && _originalairdate == rhs._originalairdate
               && _title           == rhs._title
               && _episodenumber   == rhs._episodenumber
               && _synopsis        == rhs._synopsis
               && _imageURL        == rhs._imageURL
               && _seriesID        == rhs._seriesID
                ;
    }

public:
    operator Interval() const
    {
        return {_starttime, _endtime};
    }
    EPG_TAG Epg_Tag(uint32_t number) const;
};

class GuideEntryStatus
{
public:
    GuideEntryStatus(bool n, const Interval& e)
        : _new(n)
        , _times(e)
    {}
    GuideEntryStatus()
        : _new(false)
    {}

    // Widen time interval, check for any new values
    void Merge(const GuideEntryStatus& o)
    {
        if (o._new)
            _new = true;
        _times.Add(o._times);
    }
    bool NewEntry() {
        return _new;
    }
    const IntervalSet& Times() const
    {
        return _times;
    }

private:
    bool        _new;
    IntervalSet _times;
};

class Guide
{
public:
    Guide(const Json::Value&);
    Guide() = default;

    GuideEntryStatus InsertEntry(Json::Value& v);

    std::string          _guidename;
    std::string          _affiliate;
    std::string          _imageURL;
    std::set<GuideEntry> _entries;
    uint32_t             _nextidx = 1;

    IntervalSet          _times;
    IntervalSet          _requests;

    bool _age_out(uint32_t number);
};

class Tuner
{
public:
    Tuner(const hdhomerun_discover_device_t& d);
    Tuner(const Tuner&) = delete;
    Tuner(Tuner&&) = default;
    ~Tuner();
    void Refresh();

    // Accessors
    unsigned int TunerCount() const
    {
        return _tunercount;
    }
    bool Legacy() const
    {
        return _legacy;
    }
    const uint32_t DeviceID() const
    {
        return _discover_device.device_id;
    }
    const char* Auth() const
    {
        return _discover_device.device_auth;
    }
    const char* BaseURL() const
    {
        return _discover_device.base_url;
    }
    uint32_t IP() const
    {
        return _discover_device.ip_addr;
    }
    uint32_t LocalIP() const;
    std::string GetVar(const std::string& name)
    {
        std::string retval;
        _get_var(retval, name.c_str());
        return retval;
    }
    void SetVar(const std::string& name, const std::string& value)
    {
        _set_var(value.c_str(), name.c_str());
    }

private:
    void _get_var(std::string& value, const char* name);
    void _set_var(const char*value, const char* name);
    // Called once
    void _get_api_data();
    // Called multiple times for legacy devices
    void _get_discover_data();

    // The hdhomerun_... objects depend on the order listed here for proper instantiation.
    hdhomerun_debug_t*          _debug;
    hdhomerun_device_t*         _device;
    hdhomerun_discover_device_t _discover_device;
    // Discover Data
    std::string                 _lineupURL;
    unsigned int                _tunercount;
    bool                        _legacy;
public:
    bool operator<(const Tuner& rhs) const
    {
        return DeviceID() < rhs.DeviceID();
    }
    friend class Lineup;
    friend class TunerLock;
};

class TunerLock
{
public:
    TunerLock(Tuner* t)
    : _tuner(t)
    {
        char* ret_error;
        if (hdhomerun_device_tuner_lockkey_request(_tuner->_device, &ret_error) > 0)
        {
            _success = true;
        }
    }
    ~TunerLock()
    {
        if (_success)
        {
            hdhomerun_device_tuner_lockkey_release(_tuner->_device);
        }
    }
    bool Success() const
    {
        return _success;
    }
private:
    bool   _success = false;
    Tuner* _tuner;
};

class Info
{
public:
    Info(const Json::Value&);
    Info() = default;
    const Tuner* GetFirstTuner() const
    {
        auto it = _tuners.begin();
        if (it == _tuners.end())
            return nullptr;

        return *it;
    }
    Tuner* GetNextTuner();
    void ResetNextTuner();
    bool AddTuner(Tuner*, const std::string& url);
    bool RemoveTuner(Tuner*);
    bool HasTuner(Tuner* t) const
    {
        return _tuners.find(t) != _tuners.end();
    }
    std::string DlnaURL(Tuner* t) const
    {
        auto it = _url.find(t);
        if (it != _url.end())
            return it->second;
        return "";
    }
    size_t TunerCount() const
    {
        return _tuners.size();
    }
    std::string TunerListString() const;

    std::string _guidename;
    bool        _hd       = false;
    bool        _drm      = false;
    bool        _favorite = false;

private:
    // Tuners which can receive this channel.
    // Tuner pointers are owned by Lineup
    bool                          _has_next = false;
    std::set<Tuner*>              _tuners;
    std::set<Tuner*>::iterator    _next;
    std::map<Tuner*, std::string> _url;
};

class Lineup : public Lockable
{
public:
    Lineup() = default;
    ~Lineup()
    {
        for (auto tuner: _tuners)
        {
            delete tuner;
        }
    }

    bool DiscoverTuners();
    bool UpdateLineup();
    void UpdateGuide();

    bool Update()
    {
        bool newTuner  = DiscoverTuners();
        bool newLineup = UpdateLineup();
        UpdateGuide();

        return newTuner || newLineup;
    }
    void AddLineupEntry(const Json::Value&, Tuner*);

    PVR_ERROR PvrGetChannels(ADDON_HANDLE handle, bool bRadio);
    int PvrGetChannelsAmount();
    PVR_ERROR PvrGetEPGForChannel(ADDON_HANDLE handle,
            const PVR_CHANNEL& channel,
            time_t iStart,
            time_t iEnd);
    int PvrGetChannelGroupsAmount(void);
    PVR_ERROR PvrGetChannelGroups(ADDON_HANDLE handle, bool bRadio);
    PVR_ERROR PvrGetChannelGroupMembers(ADDON_HANDLE handle,
            const PVR_CHANNEL_GROUP &group);

    const char* GetLiveStreamURL(const PVR_CHANNEL& channel);

    std::string DlnaURL(const PVR_CHANNEL& channel);
    bool OpenLiveStream(const PVR_CHANNEL& channel);
    void CloseLiveStream(void);

    int ReadLiveStream(unsigned char* buffer, unsigned int size);


private:
    std::vector<Tuner*> _minimal_covering(void);
    bool                _age_out(void);
    GuideEntryStatus    _insert_json_guide_data(const Json::Value&, const Tuner*);
    GuideEntryStatus    _insert_guide_data(const GuideNumber*, const Tuner*, time_t start=0);
    bool                _update_guide_basic();
    bool                _update_guide_extended(const GuideNumber&, time_t, time_t);

    std::set<Tuner*>          _tuners;
    std::set<uint32_t>        _device_ids;
    std::set<GuideNumber>     _lineup;
    std::map<uint32_t, Info>  _info;
    std::map<uint32_t, Guide> _guide;

    Lockable _stream_lock;
    void* _filehandle;
};


}; // namespace PVRHDHomeRun
