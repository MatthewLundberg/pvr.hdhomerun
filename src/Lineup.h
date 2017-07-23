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
    void TriggerEpgUpdate();

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

    bool OpenLiveStream(const PVR_CHANNEL& channel);
    void CloseLiveStream(void);

    int ReadLiveStream(unsigned char* buffer, unsigned int size);


private:
    bool                _age_out(void);
    bool                _guide_contains(time_t);
    bool                _insert_json_guide_data(const Json::Value&, const Tuner*);
    bool                _insert_guide_data(const GuideNumber* = nullptr, const Tuner* = nullptr, time_t start=0);
    bool                _update_guide_basic();
    bool                _update_guide_extended(const GuideNumber&, time_t start);
    bool                _open_tcp_stream(const std::string&);

    std::set<Tuner*>          _tuners;
    std::set<uint32_t>        _device_ids;
    std::set<GuideNumber>     _lineup;
    std::map<uint32_t, Info>  _info;
    std::map<uint32_t, Guide> _guide;

    Lockable _guide_lock;
    Lockable _stream_lock;
    void* _filehandle;
};


}; // namespace PVRHDHomeRun
