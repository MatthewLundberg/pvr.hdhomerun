#pragma once
/*
 *      Copyright (C) 2017 Matthew Lundberg <matthew.k.lundberg@gmail.com>
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

#include "Lockable.h"
#include "IntervalSet.h"
#include "Guide.h"
#include "client.h"
#include "Utils.h"
#include "Tuner.h"
#include "Info.h"
#include <json/json.h>
#include <cstring>
#include <vector>
#include <set>
#include <memory>
#include <string>
#include <mutex>
#include <tuple>

namespace PVRHDHomeRun {

class PVR_HDHR : public Lockable, public DeviceSet
{
public:
    PVR_HDHR() = default;
    virtual ~PVR_HDHR()
    {
        for (auto device: _devices)
        {
            delete device;
        }
    }

    bool DiscoverDevices();
    bool UpdateLineup();
    void UpdateGuide();

    bool Update()
    {
        bool newDevice  = DiscoverDevices();
        bool newLineup = UpdateLineup();
        UpdateGuide();

        return newDevice || newLineup;
    }
    void AddLineupEntry(const Json::Value&, Device*);

    PVR_ERROR PvrGetChannels(ADDON_HANDLE handle, bool bRadio);
    int PvrGetChannelsAmount();
    int PvrGetChannelGroupsAmount(void);
    PVR_ERROR PvrGetChannelGroups(ADDON_HANDLE handle, bool bRadio);
    PVR_ERROR PvrGetChannelGroupMembers(ADDON_HANDLE handle,
            const PVR_CHANNEL_GROUP &group);

    bool OpenLiveStream(const PVR_CHANNEL& channel);
    void CloseLiveStream(void);
    int ReadLiveStream(unsigned char* buffer, unsigned int size);


private:
    bool  _age_out(void);
    bool  _guide_contains(time_t);
    void  _insert_json_guide_data(const Json::Value&, const char* idstr);
    void  _fetch_guide_data(const GuideNumber* = nullptr, time_t start=0);
    void  _update_guide_basic();
    void  _update_guide_extended(const GuideNumber&, time_t start);

    virtual bool _open_live_stream(const PVR_CHANNEL& channel) = 0;
    virtual int  _read_live_stream(unsigned char* buffer, unsigned int size) = 0;
    virtual void _close_live_stream() = 0;
public:

protected:
    std::set<uint32_t>        _device_ids;
    std::set<GuideNumber>     _lineup;
    std::map<uint32_t, Info>  _info;
    std::map<uint32_t, Guide> _guide;

    Lockable _guide_lock;
    Lockable _stream_lock;
};

class PVR_HDHR_TCP : public PVR_HDHR {
private:
    bool  _open_live_stream(const PVR_CHANNEL& channel) override;
    bool  _open_tcp_stream(const std::string&);
    int   _read_live_stream(unsigned char* buffer, unsigned int size) override;
    void  _close_live_stream() override;
    void* _filehandle = nullptr;
};
class PVR_HDHR_UDP : public PVR_HDHR {
private:
    bool  _open_live_stream(const PVR_CHANNEL& channel) override;
    int   _read_live_stream(unsigned char* buffer, unsigned int size) override;
    void  _close_live_stream() override;
    int   _fd = -1;
};

PVR_HDHR* PVR_HDHR_Factory(int protocol);

}; // namespace PVRHDHomeRun
