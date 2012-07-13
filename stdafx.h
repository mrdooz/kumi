#pragma once

#ifdef DISTRIBUTION
#define WITH_CEF 0
#define WITH_ZMQ_LOGSERVER 0
#define WITH_WEBSOCKETS 0
#define WITH_GWEN 0
#define WITH_TRACKED_LOCATION 0
#else
#define WITH_CEF 0
#define WITH_ZMQ_LOGSERVER 0
#define WITH_WEBSOCKETS 1
#define WITH_GWEN 0
#define WITH_TRACKED_LOCATION 1
#endif

#if WITH_CEF
#endif

#if WITH_ZMQ_LOGSERVER
#pragma comment(lib, "libzmq.lib")
#endif

#if WITH_WEBSOCKETS
//#pragma comment(lib, "websocketpp.lib")
#endif

#if WITH_GWEN
#if DEBUG
#pragma comment(lib, "gwend_static.lib")
#else
#pragma comment(lib, "gwen_static.lib")
#endif
#pragma comment(lib, "FW1FontWrapper.lib")
#endif

#if WITH_ZMQ_LOGSERVER
#include <zmq.h>
#else
#if WITH_WEBSOCKETS
#include <WinSock2.h>
#include <ws2tcpip.h>
#endif
#endif

#include <windows.h>
#include <atlbase.h>
#include <process.h>
#include <TlHelp32.h>
#include <CommCtrl.h>

#include <stdio.h>
#include <tchar.h>
#include <stdint.h>
#include <assert.h>

#include <exception>

typedef int8_t int8;
typedef uint8_t uint8;
typedef int16_t int16;
typedef uint16_t uint16;
typedef int32_t int32;
typedef uint32_t uint32;
typedef int64_t int64;
typedef uint64_t uint64;

#include <direct.h>
#include <io.h>
#include <sys/stat.h>

#include <boost/scoped_ptr.hpp>
#include <boost/assign/list_of.hpp>
#include <boost/algorithm/string.hpp>

#include <array>
#include <deque>
#include <functional>
#include <hash_set>
#include <map>
#include <set>
#include <sstream>
#include <stack>
#include <string>
#include <time.h>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include <concurrent_queue.h>

#include <d3d11.h>
#include <D3D11Shader.h>
#include <D3DX11tex.h>
#include <D3Dcompiler.h>
#include <DxErr.h>
#include <xnamath.h>


#if WITH_CEF
#include <cef.h>
#endif

#include <concurrent_queue.h>

#if WITH_GWEN
#include "FW1FontWrapper.h"
#endif

#include "logger.hpp"

#ifdef DISTRIBUTION
#define KASSERT(x) __assume((x));
#else
#define KASSERT(x) do { if (!(x)) { LOG_ERROR_LN("** Assert failed: %s", #x); __debugbreak(); } } while(0);
#endif

typedef std::basic_string<TCHAR> ustring;

