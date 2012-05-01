#pragma once

#include <zmq.h>

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

// these files don't change that much, so we can include them here
//#include "logger.hpp"

#define USE_CEF 0
#if USE_CEF
#include <cef.h>
#endif

#include <concurrent_queue.h>

#include "FW1FontWrapper.h"

typedef std::basic_string<TCHAR> ustring;
