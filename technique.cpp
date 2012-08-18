#include "stdafx.h"
#include "technique.hpp"
#include "dx_utils.hpp"
#include "resource_interface.hpp"
#include "tracked_location.hpp"
#include "path_utils.hpp"
#include "graphics.hpp"
#include "string_utils.hpp"
#include "shader_reflection.hpp"
#include "file_utils.hpp"

#pragma comment(lib, "d3dcompiler.lib")

using namespace boost::assign;
using namespace std;

Technique::Technique()
  : _vertex_size(-1)
  , _index_format(DXGI_FORMAT_UNKNOWN)
  , _valid(false)
  , _vs_flag_mask(0)
  , _ps_flag_mask(0)
  , _cs_flag_mask(0)
  , _gs_flag_mask(0)
{
  _all_shaders.push_back(&_vertex_shaders);
  _all_shaders.push_back(&_pixel_shaders);
  _all_shaders.push_back(&_compute_shaders);
  _all_shaders.push_back(&_geometry_shaders);
}

Technique::~Technique() {
  for (size_t i = 0; i < _all_shaders.size(); ++i)
    seq_delete(_all_shaders[i]);
  _all_shaders.clear();
}

void Technique::init_from_parent(const Technique *parent) {
  _vertex_size = parent->_vertex_size;
  _index_format = parent->_index_format;
  _index_count = parent->_index_count;
  _vb = parent->_vb;
  _ib = parent->_ib;

  _rasterizer_state = parent->_rasterizer_state;
  _blend_state = parent->_blend_state;
  _depth_stencil_state = parent->_depth_stencil_state;

  _vs_flag_mask = parent->_vs_flag_mask;
  _ps_flag_mask = parent->_ps_flag_mask;
  _cs_flag_mask = parent->_cs_flag_mask;
  _gs_flag_mask = parent->_gs_flag_mask;
  _vs_shader_template = parent->_vs_shader_template;
  _ps_shader_template = parent->_ps_shader_template;
  _cs_shader_template = parent->_cs_shader_template;
  _gs_shader_template = parent->_gs_shader_template;
}

void Technique::add_error_msg(const char *fmt, ...) {
  va_list arg;
  va_start(arg, fmt);

  const int len = _vscprintf(fmt, arg) + 1;

  char* buf = (char*)_alloca(len);
  vsprintf_s(buf, len, fmt, arg);
  va_end(arg);

  if (_error_msg.empty())
    _error_msg = buf;
  else
    _error_msg += "\n" + string(buf);

  _valid = false;
}

bool Technique::compile_shader(ShaderType::Enum type, const char *entry_point, const char *src, const char *obj, const vector<string> &flags) {

  STARTUPINFOA startup_info;
  ZeroMemory(&startup_info, sizeof(startup_info));
  startup_info.cb = sizeof(STARTUPINFO);

  // create a pipe, and use it for stdout/stderr
  HANDLE stdout_read, stdout_write;
  SECURITY_ATTRIBUTES sattr;
  ZeroMemory(&sattr, sizeof(sattr));
  sattr.nLength = sizeof(SECURITY_ATTRIBUTES); 
  sattr.bInheritHandle = TRUE; 
  sattr.lpSecurityDescriptor = NULL;

  if (!CreatePipe(&stdout_read, &stdout_write, &sattr, 0)) {
    DWORD err = GetLastError();
    return false;
  }

  startup_info.dwFlags = STARTF_USESTDHANDLES;
  startup_info.hStdError = startup_info.hStdOutput = stdout_write;

  PROCESS_INFORMATION process_info;
  ZeroMemory(&process_info, sizeof(process_info));

  const char *profile = nullptr;
  switch (type) {
    case ShaderType::kVertexShader: profile = GRAPHICS.vs_profile(); break;
    case ShaderType::kPixelShader: profile = GRAPHICS.ps_profile(); break;
    case ShaderType::kComputeShader: profile = GRAPHICS.cs_profile(); break;
    case ShaderType::kGeometryShader: profile = GRAPHICS.gs_profile(); break;
    default: LOG_ERROR_LN("Implement me!");
  }

  char cmd_line[MAX_PATH];

  // create the defines for the current flags
  string defines;
  for (size_t i = 0; i < flags.size(); ++i) {
    defines.append("-D" + flags[i]);
    if (i != flags.size() - 1)
      defines.append(" ");
  }

#ifdef _DEBUG
  sprintf(cmd_line, "fxc.exe -nologo -T%s -E%s -Vi -Od -Zi -Fo %s -Fh %s %s %s", 
    profile,
    entry_point,
    obj,
    Path::replace_extension(obj, "h").c_str(),
    defines.c_str(),
    src);
#else
  sprintf(cmd_line, "fxc.exe -nologo -T%s -E%s -Vi -O3 -Zi -Fo %s -Fh %s %s %s", 
    profile,
    entry_point,
    obj,
    Path::replace_extension(obj, "h").c_str(),
    defines.c_str(),
    src);
#endif

  DWORD exit_code = 1;

  if (CreateProcessA(NULL, cmd_line, NULL, NULL, TRUE, NORMAL_PRIORITY_CLASS | CREATE_NO_WINDOW, NULL, NULL, 
                     &startup_info, &process_info)) {
    WaitForSingleObject(process_info.hProcess, INFINITE);
    GetExitCodeProcess(process_info.hProcess, &exit_code);
    FlushFileBuffers(LOGGER.handle());
    CloseHandle(process_info.hProcess);
    CloseHandle(process_info.hThread);
  } else {
    add_error_msg("Unable to launch fxc.exe");
  }

  if (exit_code) {
    // read the pipe
    if (int len = GetFileSize(stdout_read, NULL)) {
      char *buf = (char *)_alloca(len + 1);
      buf[len] = 0;
      DWORD bytes_read;
      ReadFile(stdout_read, buf, len, &bytes_read, NULL);
      add_error_msg(buf);
      LOG_WARNING_NAKED_LN(buf);
      LOG_WARNING_LN("cmd: %s", cmd_line);
    }
  }
  CloseHandle(stdout_read);
  CloseHandle(stdout_write);

  return exit_code == 0;
}

bool Technique::reload_shaders() {
  return true;
}

struct ShaderInstance {
  ShaderInstance(const string &obj) : obj(obj) {}
  ShaderInstance(const string &obj, const vector<string> &flags) : obj(obj), flags(flags) {}
  string obj;
  vector<string> flags;
};

bool Technique::create_shaders(ShaderTemplate *shader_template) {

  bool force = false;
  bool ok = true;

  Path outputPath(shader_template->_templateFilename);
  string src = outputPath.get_path() + shader_template->_source_filename;
  const char *ep = shader_template->_entry_point.c_str();
  string basePath = outputPath.get_path() + "obj/" ;
#if WITH_UNPACKED_RESOUCES
  string outputDir = stripTrailingSlash(basePath);
  if (!directory_exists(outputDir.c_str()))
    CreateDirectoryA(outputDir.c_str(), 0);
#endif

  string output_base = basePath + Path(shader_template->_obj_filename).get_filename_without_ext();
  string output_ext = Path::get_ext(shader_template->_obj_filename);
  ShaderType::Enum type = shader_template->_type;

  vector<ShaderInstance> shaders;

  // Create all the shader permutations
  if (shader_template->_flags.empty()) {
    shaders.push_back(ShaderInstance(output_base + "." + output_ext));
  } else {
    size_t num_flags = shader_template->_flags.size();
    // generate flags combinations
    for (int i = 0; i < (1 << num_flags); ++i) {
      vector<string> flags;
      string filename_suffix;
      for (size_t j = 0; j < num_flags; ++j) {
        bool flag_set = !!(i & (1 << j));
        flags.push_back(to_string("%s=%d", shader_template->_flags[j].c_str(), flag_set ? 1 : 0));
        filename_suffix.append(flag_set ? "1" : "0");
      }

      string obj = output_base + "_" + filename_suffix + "." + output_ext;
      shaders.push_back(ShaderInstance(obj, flags));
    }
  }

  // Compile all the shaders
  for (size_t i = 0; i < shaders.size(); ++i) {

    auto &cur = shaders[i];
    const char *obj = cur.obj.c_str();

#if WITH_UNPACKED_RESOUCES
    // compile the shader if the source is newer, or the object file doesn't exist
    if (force || RESOURCE_MANAGER.file_exists(src.c_str()) && (!RESOURCE_MANAGER.file_exists(obj) || RESOURCE_MANAGER.mdate(src.c_str()) > RESOURCE_MANAGER.mdate(obj))) {
      if (!compile_shader(type, ep, src.c_str(), obj, cur.flags))
        return false;
    } else {
      if (!RESOURCE_MANAGER.file_exists(obj))
        return false;
    }

    vector<char> buf;
    if (!RESOURCE_MANAGER.load_file(obj, &buf))
      return false;
#else
    vector<char> buf;
    RESOURCE_MANAGER.load_file(obj, &buf);
#endif

    int len = (int)buf.size();

    Shader *shader = new Shader(type);
    shader->_source_filename = src;
#if _DEBUG
    shader->_entry_point = ep;
    shader->_obj_filename = obj;
    shader->_flags = cur.flags;
#endif

    // load corresponding .h file
    vector<char> text;
    string headerFile = Path::replace_extension(obj, "h");
    if (!RESOURCE_MANAGER.load_file(headerFile.c_str(), &text)) {
      add_error_msg("Unable to load .h file: %s", headerFile.c_str());
      return false;
    }

    ShaderReflection ref;
    if (!ref.do_reflection(text.data(), text.size(), shader, shader_template, buf)) {
      add_error_msg("Reflection failed");
      return false;
    }

    switch (shader->type()) {

      case ShaderType::kVertexShader: 
        shader->_handle = GRAPHICS.create_vertex_shader(FROM_HERE, buf, obj);
        _vertex_shaders.push_back(shader);
        break;

      case ShaderType::kPixelShader: 
        shader->_handle = GRAPHICS.create_pixel_shader(FROM_HERE, buf, obj);
        _pixel_shaders.push_back(shader);
        break;

      case ShaderType::kComputeShader:
        shader->_handle = GRAPHICS.create_compute_shader(FROM_HERE, buf, obj);
        _compute_shaders.push_back(shader);
        break;

      case ShaderType::kGeometryShader:
        shader->_handle = GRAPHICS.create_geometry_shader(FROM_HERE, buf, obj);
        _geometry_shaders.push_back(shader);
        break;

      default:
        LOG_ERROR_LN("Implement me");
        break;
    }
  }
  return true;
}

bool Technique::init() {
  if (!_rasterizer_state.is_valid())
    _rasterizer_state = GRAPHICS.default_rasterizer_state();

  if (!_blend_state.is_valid())
    _blend_state = GRAPHICS.default_blend_state();

  if (!_depth_stencil_state.is_valid())
    _depth_stencil_state = GRAPHICS.default_depth_stencil_state();

  // create the shaders from the templates
  ShaderTemplate *templates[] = { 
    _vs_shader_template.get(), _ps_shader_template.get(), _cs_shader_template.get(), _gs_shader_template.get() };
  for (int i = 0; i < ELEMS_IN_ARRAY(templates); ++i) {
    if (templates[i] && !create_shaders(templates[i])) {
      add_error_msg("Error creating shader: %s", templates[i]->_source_filename.c_str());
      return false;
    }
  }

  for (size_t i = 0; i < _all_shaders.size(); ++i) {
    for (size_t j = 0; j < _all_shaders[i]->size(); ++j) {
      if (!(*_all_shaders[i])[j]->on_loaded())
        return false;
    }
  }

  return true;
}

void Technique::fill_cbuffer(CBuffer *cbuffer) const {
  for (size_t i = 0; i < cbuffer->vars.size(); ++i) {
    auto &cur = cbuffer->vars[i];
    PROPERTY_MANAGER.get_property_raw(cur.id, &cbuffer->staging[cur.ofs], cur.len);
  }
}

Shader *Technique::vertex_shader(int flags) const { 
  return _vertex_shaders.empty() ? nullptr : _vertex_shaders[flags & _vs_flag_mask];
}

Shader *Technique::pixel_shader(int flags) const { 
  return _pixel_shaders.empty() ? nullptr : _pixel_shaders[flags & _ps_flag_mask];
}

Shader *Technique::compute_shader(int flags) const { 
  return _compute_shaders.empty() ? nullptr : _compute_shaders[flags & _cs_flag_mask];
}

Shader *Technique::geometry_shader(int flags) const { 
  return _geometry_shaders.empty() ? nullptr : _geometry_shaders[flags & _gs_flag_mask];
}

