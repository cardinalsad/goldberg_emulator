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

#include "base.h"

#ifdef STEAM_WIN32

#ifndef EMU_RELEASE_BUILD
#ifdef VS_TRACE_DEBUG
#include <cstdio>

// Visual Studio doesn't seem to support hh* or ll* in OutputDebugString
// So process it before sending it to debug console
bool _trace(const char* format, ...)
{
    constexpr const int len = 1024; // Initial buffer size, hope it is big enought, we will exapnd it if not.
    int res;
    char *buffer = new char[len];

    va_list argptr;
    va_start(argptr, format);
    res = vsnprintf(buffer, len, format, argptr);
    va_end(argptr);
    if (res >= len)
    {
        delete[]buffer;
        // Now we are sure we have enought free space to contain the string
        buffer = new char[++res];
        va_start(argptr, format);
        vsnprintf(buffer, res, format, argptr);
        va_end(argptr);
    }

    OutputDebugString(buffer);
    delete[]buffer;

    return true;
}
#endif // VS_TRACE_DEBUG
#endif // EMU_RELEASE_BUILD


#include <windows.h>
#include <direct.h>

#define SystemFunction036 NTAPI SystemFunction036
#include <ntsecapi.h>
#undef SystemFunction036

static void
randombytes(char * const buf, const size_t size)
{
    BOOLEAN res = RtlGenRandom((PVOID) buf, (ULONG) size);
    PRINT_DEBUG("RtlGenRandom result: %s\n", (res ? "TRUE" : "FALSE"));
}

std::string get_env_variable(std::string name)
{
    char env_variable[1024];
    DWORD ret = GetEnvironmentVariableA(name.c_str(), env_variable, sizeof(env_variable));
    if (ret <= 0) {
        return std::string();
    }

    env_variable[ret] = 0;
    return std::string(env_variable);
}

#else

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

static int fd = -1;

static void randombytes(char *buf, size_t size)
{
  int i;

  if (fd == -1) {
    for (;;) {
      fd = open("/dev/urandom",O_RDONLY);
      if (fd != -1) break;
      sleep(1);
    }
  }

  while (size > 0) {
    if (size < 1048576) i = size; else i = 1048576;

    i = read(fd,buf,i);
    if (i < 1) {
      sleep(1);
      continue;
    }

    buf += i;
    size -= i;
  }
}

std::string get_env_variable(std::string name)
{
    char *env = getenv(name.c_str());
    if (!env) {
        return std::string();
    }

    return std::string(env);
}

#endif

std::recursive_mutex global_mutex;

SteamAPICall_t generate_steam_api_call_id() {
    static SteamAPICall_t a;
    randombytes((char *)&a, sizeof(a));
    ++a;
    if (a == 0) ++a;
    return a;
}

int generate_random_int() {
    int a;
    randombytes((char *)&a, sizeof(a));
    return a;
}

static uint32 generate_steam_ticket_id() {
    /* not random starts with 2? */
    static uint32 a = 1;
    ++a;
    if (a == 0) ++a;
    return a;
}

static unsigned generate_account_id()
{
    int a;
    randombytes((char *)&a, sizeof(a));
    a = abs(a);
    if (!a) ++a;
    return a;
}

CSteamID generate_steam_id_user()
{
    return CSteamID(generate_account_id(), k_unSteamUserDesktopInstance, k_EUniversePublic, k_EAccountTypeIndividual);
}

static CSteamID generate_steam_anon_user()
{
    return CSteamID(generate_account_id(), k_unSteamUserDesktopInstance, k_EUniversePublic, k_EAccountTypeAnonUser);
}

CSteamID generate_steam_id_server()
{
    return CSteamID(generate_account_id(), k_unSteamUserDesktopInstance, k_EUniversePublic, k_EAccountTypeGameServer);
}

CSteamID generate_steam_id_anonserver()
{
    return CSteamID(generate_account_id(), k_unSteamUserDesktopInstance, k_EUniversePublic, k_EAccountTypeAnonGameServer);
}

CSteamID generate_steam_id_lobby()
{
    return CSteamID(generate_account_id(), k_unSteamUserDesktopInstance | k_EChatInstanceFlagLobby, k_EUniversePublic, k_EAccountTypeChat);
}

#ifndef STEAM_WIN32
#include <sys/types.h>
#include <dirent.h>
std::string get_lib_path() {
  std::string dir = "/proc/self/map_files";
  DIR *dp;
  int i = 0;
  struct dirent *ep;
  dp = opendir (dir.c_str());
  unsigned long long int p = (unsigned long long int)&get_lib_path;

  if (dp != NULL)
  {
    while ((ep = readdir (dp))) {
      if (memcmp(ep->d_name, ".", 2) != 0 && memcmp(ep->d_name, "..", 3) != 0) {
            char *upper = NULL;
            unsigned long long int lower_bound = strtoull(ep->d_name, &upper, 16);
            if (lower_bound) {
                ++upper;
                unsigned long long int upper_bound = strtoull(upper, &upper, 16);
                if (upper_bound && (lower_bound < p && p < upper_bound)) {
                    std::string path = dir + PATH_SEPARATOR + ep->d_name;
                    char link[PATH_MAX] = {};
                    if (readlink(path.c_str(), link, sizeof(link)) > 0) {
                        std::string lib_path = link;
                        (void) closedir (dp);
                        return link;
                    }
                }
            }

        i++;
      }
    }

    (void) closedir (dp);
  }

  return ".";
}
#endif

std::string get_full_program_path()
{
    std::string program_path;
#if defined(STEAM_WIN32)
    char   DllPath[MAX_PATH] = {0};
    GetModuleFileName((HINSTANCE)&__ImageBase, DllPath, _countof(DllPath));
    program_path = DllPath;
#else
    program_path = get_lib_path();
#endif
    program_path = program_path.substr(0, program_path.rfind(PATH_SEPARATOR)).append(PATH_SEPARATOR);
    return program_path;
}

std::string get_current_path()
{
    std::string path;
#if defined(STEAM_WIN32)
    char *buffer = _getcwd( NULL, 0 );
#else
    char *buffer = get_current_dir_name();
#endif
    if (buffer) {
        path = buffer;
        path.append(PATH_SEPARATOR);
        free(buffer);
    }

    return path;
}

std::string canonical_path(std::string path)
{
    std::string output;
#if defined(STEAM_WIN32)
    char *buffer = _fullpath(NULL, path.c_str(), 0);
#else
    char *buffer = canonicalize_file_name(path.c_str());
#endif

    if (buffer) {
        output = buffer;
        free(buffer);
    }

    return output;
}

static void steam_auth_ticket_callback(void *object, Common_Message *msg)
{
    PRINT_DEBUG("steam_auth_ticket_callback\n");

    Auth_Ticket_Manager *auth_ticket_manager = (Auth_Ticket_Manager *)object;
    auth_ticket_manager->Callback(msg);
}

Auth_Ticket_Manager::Auth_Ticket_Manager(class Settings *settings, class Networking *network, class SteamCallBacks *callbacks) {
    this->network = network;
    this->settings = settings;
    this->callbacks = callbacks;

    this->network->setCallback(CALLBACK_ID_AUTH_TICKET, settings->get_local_steam_id(), &steam_auth_ticket_callback, this);
    this->network->setCallback(CALLBACK_ID_USER_STATUS, settings->get_local_steam_id(), &steam_auth_ticket_callback, this);
}

void Auth_Ticket_Manager::launch_callback(CSteamID id, EAuthSessionResponse resp)
{
    ValidateAuthTicketResponse_t data;
    data.m_SteamID = id;
    data.m_eAuthSessionResponse = resp;
    data.m_OwnerSteamID = id;
    callbacks->addCBResult(data.k_iCallback, &data, sizeof(data));
}

void Auth_Ticket_Manager::launch_callback_gs(CSteamID id, bool approved)
{
    if (approved) {
        GSClientApprove_t data;
        data.m_SteamID = data.m_OwnerSteamID = id;
        callbacks->addCBResult(data.k_iCallback, &data, sizeof(data));
    } else {
        GSClientDeny_t data;
        data.m_SteamID = id;
        data.m_eDenyReason = k_EDenyNotLoggedOn; //TODO: other reasons?
        callbacks->addCBResult(data.k_iCallback, &data, sizeof(data));
    }
}

#define STEAM_ID_OFFSET_TICKET (4 + 8)
#define STEAM_TICKET_MIN_SIZE (4 + 8 + 8)
Auth_Ticket_Data Auth_Ticket_Manager::getTicketData( void *pTicket, int cbMaxTicket, uint32 *pcbTicket )
{
    uint64 steam_id = settings->get_local_steam_id().ConvertToUint64();
    memset(pTicket, 123, cbMaxTicket);
    ((char *)pTicket)[0] = 0x14;
    ((char *)pTicket)[1] = 0;
    ((char *)pTicket)[2] = 0;
    ((char *)pTicket)[3] = 0;
    memcpy((char *)pTicket + STEAM_ID_OFFSET_TICKET, &steam_id, sizeof(steam_id));
    *pcbTicket = cbMaxTicket;

    Auth_Ticket_Data ticket_data;
    ticket_data.id = settings->get_local_steam_id();
    ticket_data.number = generate_steam_ticket_id();
    uint32 ttt = ticket_data.number;

    memcpy(((char *)pTicket) + sizeof(uint64), &ttt, sizeof(ttt));
    return ticket_data;
}
//Conan Exiles doesn't work with 512 or 128, 256 seems to be the good size
//Steam returns 234
#define STEAM_AUTH_TICKET_SIZE 234

uint32 Auth_Ticket_Manager::getTicket( void *pTicket, int cbMaxTicket, uint32 *pcbTicket )
{
    if (cbMaxTicket < STEAM_TICKET_MIN_SIZE) return 0;
    if (cbMaxTicket > STEAM_AUTH_TICKET_SIZE) cbMaxTicket = STEAM_AUTH_TICKET_SIZE;

    Auth_Ticket_Data ticket_data = getTicketData(pTicket, cbMaxTicket, pcbTicket );
    uint32 ttt = ticket_data.number;
    GetAuthSessionTicketResponse_t data;
    data.m_hAuthTicket = ttt;
    data.m_eResult = k_EResultOK;
    callbacks->addCBResult(data.k_iCallback, &data, sizeof(data));

    outbound.push_back(ticket_data);

    return ttt;
}

CSteamID Auth_Ticket_Manager::fakeUser()
{
    Auth_Ticket_Data data = {};
    data.id = generate_steam_anon_user();
    inbound.push_back(data);
    return data.id;
}

void Auth_Ticket_Manager::cancelTicket(uint32 number)
{
    auto ticket = std::find_if(outbound.begin(), outbound.end(), [&number](Auth_Ticket_Data const& item) { return item.number == number; });
    if (outbound.end() == ticket)
        return;

    Auth_Ticket *auth_ticket = new Auth_Ticket();
    auth_ticket->set_number(number);
    auth_ticket->set_type(Auth_Ticket::CANCEL);
    Common_Message msg;
    msg.set_source_id(settings->get_local_steam_id().ConvertToUint64());
    msg.set_allocated_auth_ticket(auth_ticket);
    network->sendToAll(&msg, true);

    outbound.erase(ticket);
}

bool Auth_Ticket_Manager::SendUserConnectAndAuthenticate( uint32 unIPClient, const void *pvAuthBlob, uint32 cubAuthBlobSize, CSteamID *pSteamIDUser )
{
    if (cubAuthBlobSize < STEAM_TICKET_MIN_SIZE) return false;

    Auth_Ticket_Data data;
    uint64 id;
    memcpy(&id, (char *)pvAuthBlob + STEAM_ID_OFFSET_TICKET, sizeof(id));
    uint32 number;
    memcpy(&number, ((char *)pvAuthBlob) + sizeof(uint64), sizeof(number));
    data.id = CSteamID(id);
    data.number = number;
    if (pSteamIDUser) *pSteamIDUser = data.id;

    for (auto & t : inbound) {
        if (t.id == data.id) {
            //Should this return false?
            launch_callback_gs(id, true);
            return true;
        }
    }

    inbound.push_back(data);
    launch_callback_gs(id, true);
    return true;
}

EBeginAuthSessionResult Auth_Ticket_Manager::beginAuth(const void *pAuthTicket, int cbAuthTicket, CSteamID steamID )
{
    if (cbAuthTicket < STEAM_TICKET_MIN_SIZE) return k_EBeginAuthSessionResultInvalidTicket;

    Auth_Ticket_Data data;
    uint64 id;
    memcpy(&id, (char *)pAuthTicket + STEAM_ID_OFFSET_TICKET, sizeof(id));
    uint32 number;
    memcpy(&number, ((char *)pAuthTicket) + sizeof(uint64), sizeof(number));
    data.id = CSteamID(id);
    data.number = number;

    for (auto & t : inbound) {
        if (t.id == data.id) {
            return k_EBeginAuthSessionResultDuplicateRequest;
        }
    }

    inbound.push_back(data);
    launch_callback(steamID, k_EAuthSessionResponseOK);
    return k_EBeginAuthSessionResultOK;
}

uint32 Auth_Ticket_Manager::countInboundAuth()
{
    return inbound.size();
}

bool Auth_Ticket_Manager::endAuth(CSteamID id)
{
    auto ticket = std::find_if(inbound.begin(), inbound.end(), [&id](Auth_Ticket_Data const& item) { return item.id == id; });
    if (inbound.end() == ticket)
        return false;

    inbound.erase(ticket);
    return true;
}

void Auth_Ticket_Manager::Callback(Common_Message *msg)
{
    if (msg->has_low_level()) {
        if (msg->low_level().type() == Low_Level::CONNECT) {
            
        }

        if (msg->low_level().type() == Low_Level::DISCONNECT) {
            PRINT_DEBUG("TICKET DISCONNECT\n");
            auto t = std::begin(inbound);
            while (t != std::end(inbound)) {
                if (t->id.ConvertToUint64() == msg->source_id()) {
                    launch_callback(t->id, k_EAuthSessionResponseUserNotConnectedToSteam);
                    t = inbound.erase(t);
                } else {
                    ++t;
                }
            }
        }
    }

    if (msg->has_auth_ticket()) {
        if (msg->auth_ticket().type() == Auth_Ticket::CANCEL) {
            PRINT_DEBUG("TICKET CANCEL %llu\n", msg->source_id());
            uint32 number = msg->auth_ticket().number();
            auto t = std::begin(inbound);
            while (t != std::end(inbound)) {
                if (t->id.ConvertToUint64() == msg->source_id() && t->number == number) {
                    launch_callback(t->id, k_EAuthSessionResponseAuthTicketCanceled);
                    t = inbound.erase(t);
                } else {
                    ++t;
                }
            }
        }
    }
}

#ifdef EMU_EXPERIMENTAL_BUILD
#ifdef STEAM_WIN32
#include "../detours/detours.h"

static bool is_lan_ip(const sockaddr *addr, int namelen)
{
    if (!namelen) return false;

    if (addr->sa_family == AF_INET) {
        struct sockaddr_in *addr_in = (struct sockaddr_in *)addr;
        unsigned char ip[4];
        memcpy(ip, &addr_in->sin_addr, sizeof(ip));
        PRINT_DEBUG("CHECK LAN IP %hhu.%hhu.%hhu.%hhu:%u\n", ip[0], ip[1], ip[2], ip[3], ntohs(addr_in->sin_port));
        if (ip[0] == 127) return true;
        if (ip[0] == 10) return true;
        if (ip[0] == 192 && ip[1] == 168) return true;
        if (ip[0] == 169 && ip[1] == 254 && ip[2] != 0) return true;
        if (ip[0] == 172 && ip[1] >= 16 && ip[1] <= 31) return true;
        if ((ip[0] == 100) && ((ip[1] & 0xC0) == 0x40)) return true;
        if (ip[0] == 239) return true; //multicast
        if (ip[0] == 0) return true; //Current network
        if (ip[0] == 192 && (ip[1] == 18 || ip[1] == 19)) return true; //Used for benchmark testing of inter-network communications between two separate subnets.
        if (ip[0] >= 224) return true; //ip multicast (224 - 239) future use (240.0.0.0–255.255.255.254) broadcast (255.255.255.255)
    } else if (addr->sa_family == AF_INET6) {
        struct sockaddr_in6 *addr_in6 = (struct sockaddr_in6 *)addr;
        unsigned char ip[16];
        unsigned char zeroes[16] = {};
        memcpy(ip, &addr_in6->sin6_addr, sizeof(ip));
        PRINT_DEBUG("CHECK LAN IP6 %hhu.%hhu.%hhu.%hhu.%hhu.%hhu.%hhu.%hhu...%hhu\n", ip[0], ip[1], ip[2], ip[3], ip[4], ip[5], ip[6], ip[7], ip[15]);
        if (((ip[0] == 0xFF) && (ip[1] < 3) && (ip[15] == 1)) ||
        ((ip[0] == 0xFE) && ((ip[1] & 0xC0) == 0x80))) return true;
        if (memcmp(zeroes, ip, sizeof(ip)) == 0) return true;
        if (memcmp(zeroes, ip, sizeof(ip) - 1) == 0 && ip[15] == 1) return true;
        if (ip[0] == 0xff) return true; //multicast
        if (ip[0] == 0xfc) return true; //unique local
        if (ip[0] == 0xfd) return true; //unique local
        //TODO: ipv4 mapped?
    }

    PRINT_DEBUG("NOT LAN IP\n");
    return false;
}

int ( WINAPI *Real_SendTo )( SOCKET s, const char *buf, int len, int flags, const sockaddr *to, int tolen) = sendto;
int ( WINAPI *Real_Connect )( SOCKET s, const sockaddr *addr, int namelen ) = connect;
int ( WINAPI *Real_WSAConnect )( SOCKET s, const sockaddr *addr, int namelen, LPWSABUF lpCallerData, LPWSABUF lpCalleeData, LPQOS lpSQOS, LPQOS lpGQOS) = WSAConnect;

static int WINAPI Mine_SendTo( SOCKET s, const char *buf, int len, int flags, const sockaddr *to, int tolen) {
    PRINT_DEBUG("Mine_SendTo\n");
    if (is_lan_ip(to, tolen)) {
        return Real_SendTo( s, buf, len, flags, to, tolen );
    } else {
        return len;
    }
}

static int WINAPI Mine_Connect( SOCKET s, const sockaddr *addr, int namelen )
{
    PRINT_DEBUG("Mine_Connect\n");
    if (is_lan_ip(addr, namelen)) {
        return Real_Connect(s, addr, namelen);
    } else {
        WSASetLastError(WSAECONNREFUSED);
        return SOCKET_ERROR;
    }
}

static int WINAPI Mine_WSAConnect( SOCKET s, const sockaddr *addr, int namelen, LPWSABUF lpCallerData, LPWSABUF lpCalleeData, LPQOS lpSQOS, LPQOS lpGQOS)
{
    PRINT_DEBUG("Mine_WSAConnect\n");
    if (is_lan_ip(addr, namelen)) {
        return Real_WSAConnect(s, addr, namelen, lpCallerData, lpCalleeData, lpSQOS, lpGQOS);
    } else {
        WSASetLastError(WSAECONNREFUSED);
        return SOCKET_ERROR;
    }
}

inline bool file_exists (const std::string& name) {
  struct stat buffer;   
  return (stat (name.c_str(), &buffer) == 0); 
}

#ifdef DETOURS_64BIT
#define DLL_NAME "steam_api64.dll"
#else
#define DLL_NAME "steam_api.dll"
#endif


HMODULE (WINAPI *Real_GetModuleHandleA)(LPCSTR lpModuleName) = GetModuleHandleA;
HMODULE WINAPI Mine_GetModuleHandleA(LPCSTR lpModuleName)
{
    PRINT_DEBUG("Mine_GetModuleHandleA %s\n", lpModuleName);
    if (!lpModuleName) return Real_GetModuleHandleA(lpModuleName);
    std::string in(lpModuleName);
    if (in == std::string(DLL_NAME)) {
        in = std::string("crack") + in;
    }

    return Real_GetModuleHandleA(in.c_str());
}

static void redirect_crackdll()
{
    DetourTransactionBegin();
    DetourUpdateThread( GetCurrentThread() );
    DetourAttach( &(PVOID &)Real_GetModuleHandleA, Mine_GetModuleHandleA );
    DetourTransactionCommit();
}

static void unredirect_crackdll()
{
    DetourTransactionBegin();
    DetourUpdateThread( GetCurrentThread() );
    DetourDetach( &(PVOID &)Real_GetModuleHandleA, Mine_GetModuleHandleA );
    DetourTransactionCommit();
}

HMODULE crack_dll_handle;
static void load_dll()
{
    std::string path = get_full_program_path();
    path += "crack";
    //path += PATH_SEPARATOR;
    path += DLL_NAME;
    PRINT_DEBUG("Crack file %s\n", path.c_str());
    if (file_exists(path)) {
        redirect_crackdll();
        crack_dll_handle = LoadLibraryA(path.c_str());
        unredirect_crackdll();
        PRINT_DEBUG("Loaded crack file\n");
    }
}

#ifdef DETOURS_64BIT
#define LUMA_CEG_DLL_NAME "LumaCEG_Plugin_x64.dll"
#else
#define LUMA_CEG_DLL_NAME "LumaCEG_Plugin_x86.dll"
#endif

static void load_lumaCEG()
{
    std::string path = get_full_program_path();
    path += LUMA_CEG_DLL_NAME;
    if (file_exists(path)) {
        PRINT_DEBUG("loading luma ceg dll %s\n", path.c_str());
        if (LoadLibraryA(path.c_str())) {
            PRINT_DEBUG("Loaded luma ceg dll file\n");
        }
    }
}

//For some reason when this function is optimized it breaks the shogun 2 prophet (reloaded) crack.
#pragma optimize( "", off )
bool crack_SteamAPI_RestartAppIfNecessary(uint32 unOwnAppID)
{
    if (crack_dll_handle) {
        bool (__stdcall* restart_app)(uint32) = (bool (__stdcall *)(uint32))GetProcAddress(crack_dll_handle, "SteamAPI_RestartAppIfNecessary");
        if (restart_app) {
            PRINT_DEBUG("Call crack SteamAPI_RestartAppIfNecessary\n");
            redirect_crackdll();
            bool ret = restart_app(unOwnAppID);
            unredirect_crackdll();
            return ret;
        }
    }

    return false;
}
#pragma optimize( "", on )

bool crack_SteamAPI_Init()
{
    if (crack_dll_handle) {
        bool (__stdcall* init_app)() = (bool (__stdcall *)())GetProcAddress(crack_dll_handle, "SteamAPI_Init");
        if (init_app) {
            PRINT_DEBUG("Call crack SteamAPI_Init\n");
            redirect_crackdll();
            bool ret = init_app();
            unredirect_crackdll();
            return ret;
        }
    }

    return false;
}
#include <winhttp.h>

HINTERNET (WINAPI *Real_WinHttpConnect)(
  IN HINTERNET     hSession,
  IN LPCWSTR       pswzServerName,
  IN INTERNET_PORT nServerPort,
  IN DWORD         dwReserved
);

HINTERNET WINAPI Mine_WinHttpConnect(
  IN HINTERNET     hSession,
  IN LPCWSTR       pswzServerName,
  IN INTERNET_PORT nServerPort,
  IN DWORD         dwReserved
) {
    PRINT_DEBUG("Mine_WinHttpConnect %ls %u\n", pswzServerName, nServerPort);
    struct sockaddr_in ip4;
    struct sockaddr_in6 ip6;
    ip4.sin_family = AF_INET;
    ip6.sin6_family = AF_INET6;

    if ((InetPtonW(AF_INET, pswzServerName, &(ip4.sin_addr)) && is_lan_ip((sockaddr *)&ip4, sizeof(ip4))) || (InetPtonW(AF_INET6, pswzServerName, &(ip6.sin6_addr)) && is_lan_ip((sockaddr *)&ip6, sizeof(ip6)))) {
        return Real_WinHttpConnect(hSession, pswzServerName, nServerPort, dwReserved);
    } else {
        return Real_WinHttpConnect(hSession, L"127.1.33.7", nServerPort, dwReserved);
    }
}

static bool network_functions_attached = false;
BOOL WINAPI DllMain( HINSTANCE, DWORD dwReason, LPVOID ) {
    switch ( dwReason ) {
        case DLL_PROCESS_ATTACH:
            if (!file_exists(get_full_program_path() + "disable_lan_only.txt")) {
                PRINT_DEBUG("Hooking lan only functions\n");
                DetourTransactionBegin();
                DetourUpdateThread( GetCurrentThread() );
                DetourAttach( &(PVOID &)Real_SendTo, Mine_SendTo );
                DetourAttach( &(PVOID &)Real_Connect, Mine_Connect );
                DetourAttach( &(PVOID &)Real_WSAConnect, Mine_WSAConnect );

                HMODULE winhttp = GetModuleHandle("winhttp.dll");
                if (winhttp) {
                    Real_WinHttpConnect = (decltype(Real_WinHttpConnect))GetProcAddress(winhttp, "WinHttpConnect");
                    DetourAttach( &(PVOID &)Real_WinHttpConnect, Mine_WinHttpConnect );
                }
                DetourTransactionCommit();
                network_functions_attached = true;
            }
            load_dll();
            load_lumaCEG();
            break;

        case DLL_PROCESS_DETACH:
            if (network_functions_attached) {
                DetourTransactionBegin();
                DetourUpdateThread( GetCurrentThread() );
                DetourDetach( &(PVOID &)Real_SendTo, Mine_SendTo );
                DetourDetach( &(PVOID &)Real_Connect, Mine_Connect );
                DetourDetach( &(PVOID &)Real_WSAConnect, Mine_WSAConnect );
                if (Real_WinHttpConnect) {
                    DetourDetach( &(PVOID &)Real_WinHttpConnect, Mine_WinHttpConnect );
                }
                DetourTransactionCommit();
            }
        break;
    }

    return TRUE;
}
#endif
#endif
