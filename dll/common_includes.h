/* Copyright (C) 2019 Mr Goldberg
   This file is part of the Goldberg Emulator

   The Goldberg Emulator is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 3 of the License, or (at your option) any later version.

   The Goldberg Emulator is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the Goldberg Emulator; if not, see
   <http://www.gnu.org/licenses/>.  */

#ifndef __INCLUDED_COMMON_INCLUDES__
#define __INCLUDED_COMMON_INCLUDES__

#if defined(WIN64) || defined(_WIN64) || defined(__MINGW64__)
    #define __WINDOWS_64__
#elif defined(WIN32) || defined(_WIN32) || defined(__MINGW32__)
    #define __WINDOWS_32__
#endif

#if defined(__WINDOWS_32__) || defined(__WINDOWS_64__)
    #define __WINDOWS__
#endif

#if defined(__linux__) || defined(linux)
    #if defined(__x86_64__)
        #define __LINUX_64__
    #else
        #define __LINUX_32__
    #endif
#endif

#if defined(__LINUX_32__) || defined(__LINUX_64__)
    #define __LINUX__
#endif

#if defined(__APPLE__)
    #if defined(__x86_64__)
        #define __APPLE_64__
        #define __64BITS__
    #else
        #define __APPLE_32__
        #define __32BITS__
    #endif
#endif

#if defined(__WINDOWS__)
    #define STEAM_WIN32
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
#endif

#define STEAM_API_EXPORTS

#if defined(__WINDOWS__)
    #include <winsock2.h>
    #include <windows.h>
    #include <ws2tcpip.h>
    #include <processthreadsapi.h>
    #include <windows.h>
    #include <direct.h>
    #include <iphlpapi.h> // Include winsock2 before this, or winsock2 iphlpapi will be unavailable
    #include <shlobj.h>

    #define SystemFunction036 NTAPI SystemFunction036
    #include <ntsecapi.h>
    #undef SystemFunction036

    #ifndef EMU_RELEASE_BUILD
        #define PRINT_DEBUG(a, ...) do {FILE *t = fopen("STEAM_LOG.txt", "a"); fprintf(t, "%u " a, GetCurrentThreadId(), __VA_ARGS__); fclose(t); WSASetLastError(0);} while (0)
    #endif

    EXTERN_C IMAGE_DOS_HEADER __ImageBase;
    #define PATH_SEPARATOR "\\"

    #ifdef EMU_EXPERIMENTAL_BUILD
        #include <winhttp.h>

        #include "../detours/detours.h"

        #ifdef DETOURS_64BIT
            #define LUMA_CEG_DLL_NAME "LumaCEG_Plugin_x64.dll"
            #define DLL_NAME "steam_api64.dll"
        #else
            #define LUMA_CEG_DLL_NAME "LumaCEG_Plugin_x86.dll"
            #define DLL_NAME "steam_api.dll"
        #endif
    #endif

#elif defined(__LINUX__) || defined(__APPLE__)
    #if defined(__LINUX__)
        // Insert here Linux specific headers
    #else
		// Insert here MacOS specific headers
        #include <sys/sysctl.h>
        #include <mach-o/dyld_images.h>
    #endif
	#include <ifaddrs.h>// getifaddrs
    #include <arpa/inet.h>

    #include <sys/types.h>
    #include <sys/stat.h>
    #include <sys/ioctl.h>
    #include <sys/socket.h>
    #include <sys/mount.h>
    #include <sys/stat.h>
    #include <sys/statvfs.h>
    #include <sys/time.h>

    #include <netinet/in.h>

    #include <fcntl.h>
    #include <unistd.h>
    #include <dirent.h>
    #include <netdb.h>
    #include <dlfcn.h>
    #include <utime.h>

    #define PATH_MAX_STRING_SIZE 512

    #ifndef EMU_RELEASE_BUILD
        #define PRINT_DEBUG(...) {FILE *t = fopen("STEAM_LOG.txt", "a"); fprintf(t, __VA_ARGS__); fclose(t);}
    #endif
    #define PATH_SEPARATOR "/" 
	
#endif

#ifndef MSG_NOSIGNAL
	#define MSG_NOSIGNAL 0
#endif

//#define PRINT_DEBUG(...) fprintf(stdout, __VA_ARGS__)
#ifdef EMU_RELEASE_BUILD
    #define PRINT_DEBUG(...)
#endif

// C/C++ includes
#include <cstdint>
#include <algorithm>
#include <string>
#include <chrono>
#include <cctype>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <iterator>

#include <vector>
#include <map>
#include <set>
#include <queue>
#include <list>

#include <thread>
#include <mutex>
#include <condition_variable>

#include <string.h>
#include <stdio.h>

// Other libs includes
#include <nlohmann/json.hpp>
#include <nlohmann/fifo_map.hpp>
#include "../controller/gamepad.h"

// Steamsdk includes
#include "../sdk_includes/steam_api.h"
#include "../sdk_includes/steam_gameserver.h"
#include "../sdk_includes/steamdatagram_tickets.h"

// Emulator includes
#include "net.pb.h"
#include "settings.h"
#include "local_storage.h"
#include "network.h"

// Emulator defines
#define CLIENT_HSTEAMUSER 1
#define SERVER_HSTEAMUSER 1

#define DEFAULT_NAME "Goldberg"
#define PROGRAM_NAME "Goldberg SteamEmu"
#define DEFAULT_LANGUAGE "english"

#define LOBBY_CONNECT_APPID ((uint32)-2)

#endif//__INCLUDED_COMMON_INCLUDES__