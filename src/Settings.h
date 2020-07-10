#pragma once
/*
 *      Copyright (C) 2017-2018 Matthew Lundberg <matthew.k.lundberg@gmail.com>
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

#include <kodi/AddonBase.h>
#include <vector>
#include <set>

#if defined(_WIN32)
#define DLL_EXPORT __declspec(dllexport)
#else
#define DLL_EXPORT
#endif

namespace PVRHDHomeRun
{

class PVR_HDHR;


class ATTRIBUTE_HIDDEN SettingsType
{
public:
    bool ReadSettings();
    ADDON_STATUS SetSetting(const std::string& settingName, const kodi::CSettingValue& settingValue);
    
    bool UseLegacyDevices()
    {
        return useLegacyDevices && protocol == UDP;
    }
    
    enum CHANNEL_NAME {
        TUNER_NAME,
        GUIDE_NAME,
        AFFILIATE
    };
    enum PROTOCOL {
        TCP,
        UDP
    };

    bool hideProtectedChannels  = true;
    bool debugLog               = false;
    //bool markNewProgram         = false;
    bool useLegacyDevices       = false;
    bool hideUnknownChannels    = true;
    CHANNEL_NAME channelName    = AFFILIATE;
    PROTOCOL     protocol       = TCP;
    bool extendedGuide          = false;
    int  guideDays              = 1;
    std::set<std::string> hiddenChannels;
    std::vector<uint32_t> preferredDevice;
    std::set<uint32_t>    blacklistDevice;
    int udpPort                 = 5000;
    bool record                 = false;
    bool recordforlive          = true;
    bool use_stream_url         = false;

    bool usegroups              = false;
    int deviceDiscoverInterval  = 300;
    int lineupUpdateInterval    = 300;       // 5 min   Refresh lineup (local traffic for DLNA devices, remote fetch for legacy)
    int recordUpdateInterval    = 30;        // 30 sec  Refresh list from RECORD engine (local)
    int ruleUpdateInterval      = 3600;      // 1 hour  Refresh recording rules (remote)
    int guideUpdateInterval     = 60;        // 1 min   Check guide for the need to update (remote)
    int guideBasicInterval      = 3600;      // 1 hour  Refresh basic guide
    int guideBasicBeforeHour    = 300;       // 5 minutes before the hour ...
    int guideRandom             = 300;       // ... but up to 5 minutes early
    int guideExtendedEach       = 3600 * 8;  // 8 hours How much is supplied at a time
    int guideExtendedHysteresis = 3600;      // 4 hours max unfilled guide...
};

struct ATTRIBUTE_HIDDEN GlobalsType
{
    bool         isCreated               = false;
    ADDON_STATUS currentStatus           = ADDON_STATUS_UNKNOWN;
    std::string  userPath;
    std::string  clientPath;
    //ADDON::CHelper_libXBMC_addon* XBMC    = nullptr;
    //CHelper_libXBMC_pvr*          PVR     = nullptr;
    PVR_HDHR*                     pvr_hdhr = nullptr;

    SettingsType Settings;
};

extern GlobalsType g;

};

