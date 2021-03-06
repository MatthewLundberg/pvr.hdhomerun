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

#include <string>
#include <json/json.h>

#if defined(TARGET_WINDOWS) && defined(DEBUG)
#define USE_DBG_CONSOLE
#endif

#ifdef USE_DBG_CONSOLE
int DbgPrintf(const char* szFormat, ...);
#else
#define DbgPrintf(...)              do {} while(0)
#endif // USE_DBG_CONSOLE

#define KODI_LOG(level, ...)											\
    do																	\
    {                                                                   \
        using namespace ADDON;                                          \
        DbgPrintf("%-10s: ",  #level);									\
        DbgPrintf(__VA_ARGS__);											\
        DbgPrintf("\n");												\
        if (g.XBMC && (level > ADDON::LOG_DEBUG || g.Settings.debugLog))\
            g.XBMC->Log((addon_log_t)level, __VA_ARGS__);               \
    } while (0)


namespace PVRHDHomeRun
{

template<int N>
void pvr_strcpy(char (&dest)[N], const std::string& src)
{
    strncpy(dest, src.c_str(), N-1);
    dest[N-1] = '\0';
}
template<int N>
void pvr_strcpy(char (&dest)[N], const char* src)
{
    strncpy(dest, src, N-1);
    dest[N-1] = '\0';
}

bool GetFileContents(const std::string& url, std::string& content);
bool StringToJson(const std::string& in, Json::Value& out, std::string& err);

std::string EncodeURL(const std::string& strUrl);
std::string FormatIP(uint32_t);
bool IPSubnetMatch(uint32_t a, uint32_t b, uint32_t subnet_mask);
std::string FormatTime(time_t);

};
