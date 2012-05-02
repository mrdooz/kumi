#pragma once

#ifdef DISTRIBUTION
#define WITH_CEF 0
#define WITH_ZMQ_LOGSERVER 0
#define WITH_WEBSOCKETS 0
#define WITH_GWEN 0
#else
#define WITH_CEF 0
#define WITH_ZMQ_LOGSERVER 0
#define WITH_WEBSOCKETS 1
#define WITH_GWEN 0
#endif

#if WITH_CEF
#endif

#if WITH_ZMQ_LOGSERVER
#pragma comment(lib, "libzmq.lib")
#endif

#if WITH_WEBSOCKETS
#pragma comment(lib, "websocketpp.lib")
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
#include <WinSock2.h>
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
/*
#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/phoenix_core.hpp>
#include <boost/spirit/include/phoenix_operator.hpp>
#include <boost/spirit/include/phoenix_fusion.hpp>
#include <boost/spirit/include/phoenix_stl.hpp>
#include <boost/spirit/include/phoenix_bind.hpp>
#include <boost/fusion/include/adapt_struct.hpp>
#include <boost/variant/recursive_variant.hpp>
#include <boost/foreach.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/scoped_array.hpp>

#include <boost/any.hpp>
#include <boost/function_types/function_arity.hpp>
#include <boost/function_types/parameter_types.hpp>
#include <boost/function_types/result_type.hpp>
#include <boost/mpl/at.hpp>
#include <boost/mpl/for_each.hpp>
#include <boost/mpl/if.hpp> 
#include <boost/mpl/int.hpp>
#include <boost/mpl/size.hpp>
#include <boost/mpl/transform.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/type_traits/is_void.hpp> 
#include <boost/type_traits/remove_reference.hpp>
#include <boost/type_traits/is_member_function_pointer.hpp>

*/

#include <boost/scoped_ptr.hpp>
#include <boost/assign/list_of.hpp>

#include <vector>
#include <string>
#include <map>
#include <deque>
#include <tuple>
#include <unordered_map>
#include <type_traits>
#include <stack>
#include <hash_set>
#include <set>
#include <time.h>
#include <functional>

#include <concurrent_queue.h>

#include <d3d11.h>
#include <D3D11Shader.h>
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

typedef std::basic_string<TCHAR> ustring;
