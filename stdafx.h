#pragma once

#include <windows.h>
#include <atlbase.h>
#include <process.h>

#include <stdio.h>
#include <tchar.h>
#include <stdint.h>

#include <direct.h>
#include <io.h>
#include <sys/stat.h>

#include <boost/config/warning_disable.hpp>
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

#include <vector>
#include <string>
#include <map>
#include <deque>
#include <tuple>
#include <hash_map>

#include <concurrent_queue.h>

#include <d3d11.h>
#include <D3D11Shader.h>
#include <D3Dcompiler.h>
#include <DxErr.h>
#include <xnamath.h>

// these files don't change that much, so we can include them here
#include "fast_delegate.hpp"
#include "utils.hpp"
#include "string_utils.hpp"
#include "file_utils.hpp"
#include "path_utils.hpp"
#include "dx_utils.hpp"
#include "logger.hpp"

#include "d3d_parser.hpp"

#include "stb_truetype.h"
using namespace fastdelegate;
