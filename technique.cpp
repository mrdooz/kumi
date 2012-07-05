#include "stdafx.h"
#include "technique.hpp"
#include "dx_utils.hpp"
#include "resource_interface.hpp"
#include "tracked_location.hpp"
#include "path_utils.hpp"
#include "graphics.hpp"
#include "string_utils.hpp"

#pragma comment(lib, "d3dcompiler.lib")

using namespace boost::assign;
using namespace std;

Technique::Technique()
  : _rasterizer_desc(CD3D11_DEFAULT())
  , _blend_desc(CD3D11_DEFAULT())
  , _depth_stencil_desc(CD3D11_DEFAULT())
  , _vertex_size(-1)
  , _index_format(DXGI_FORMAT_UNKNOWN)
  , _valid(false)
  , _vs_shader_template(nullptr)
  , _ps_shader_template(nullptr)
  , _vs_flag_mask(0)
  , _ps_flag_mask(0)
{
}

Technique::~Technique() {
  delete exch_null(_vs_shader_template);
  delete exch_null(_ps_shader_template);
  seq_delete(&_vertex_shaders);
  seq_delete(&_pixel_shaders);
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

static const char *skip_line(const char *start, const char *end) {
  while (start < end && *start != '\r' && *start != '\n')
    ++start;

  while (start < end && *start == '\r' || *start == '\n')
    ++start;

  return start;
}

static vector<string> split(const char *start, const char *end) {
  vector<string> res;

  while (start < end) {
    const char *tmp = start;
    while (!isspace((int)*tmp))
      ++tmp;

    res.emplace_back(string(start, tmp));

    while (tmp < end && isspace((int)*tmp))
      ++tmp;

    start = tmp;
  }

  return res;
}

static void trim_input(const vector<char> &input, vector<vector<string>> *res) {
  const char *start = input.data();
  const char *end = start + input.size();
  // extract the relevant text
  if (strstr(start, "#if 0"))
    start = skip_line(start, end);

  while (start < end) {
    // skip leading comments and whitespace
    if (*start != '/')
      break;
    while (*start == '/' || *start == ' ')
      ++start;

    // read until "\r\n" found
    const char *tmp = start;
    while (*tmp != '\r')
      ++tmp;
    if (tmp - start > 0)
      res->emplace_back(split(start, tmp));
    while (*tmp == '\r' || *tmp == '\n')
      ++tmp;
    start = tmp;
  }
}

bool Technique::do_reflection(const std::vector<char> &text, Shader *shader, ShaderTemplate *shader_template, const vector<char> &obj) {

  // extract the relevant text
  vector<vector<string>> input;
  trim_input(text, &input);

  // parse it!
  for (size_t i = 0; i < input.size(); ++i) {
    auto &cur_line = input[i];

    if (cur_line[0] == "cbuffer") {
      // Parse cbuffer
      CBuffer &cbuffer = dummy_push_back(&shader->_cbuffers);
      int size = 0;

      for (i += 2; i < input.size(); ++i) {
        auto &row = input[i];
        if (row[0] == "}")
          break;
        if (row.size() == 7 || row.size() == 8) {
          bool unused = row.size() == 8 && row[7] == "[unused]";
          if (!unused) {
            string name = string(row[1].c_str(), row[1].size() - 1); // skip trailing ';'
            // strip any []
            int bracket = name.find('[');
            if (bracket != name.npos) {
              name = string(name.c_str(), bracket);
            }
            // Find the parameter in the template, so we can get the source
            CBufferParam *param = shader_template->find_cbuffer_param(name);
            if (!param) {
              add_error_msg("Shader parameter %s not found", name.c_str());
              return false;
            }

            CBufferVariable var;
            var.ofs = atoi(row[4].c_str());
            var.len = atoi(row[6].c_str());
            size = max(size, var.ofs + var .len);
            // The id is either a classifer (for mesh/material), or a real id in the system case
            string class_id = PropertySource::to_string(param->source) + "::" + name;
            int var_size = param->source == PropertySource::kMesh ? sizeof(int) : var.len;
            var.id = PROPERTY_MANAGER.get_or_create_raw(class_id.c_str(), var_size, nullptr);
#ifdef _DEBUG
            var.name = class_id;
#endif
            switch (param->source) {
              case PropertySource::kMaterial: cbuffer.material_vars.emplace_back(var); break;
              case PropertySource::kMesh: cbuffer.mesh_vars.emplace_back(var); break;
              case PropertySource::kSystem: cbuffer.system_vars.emplace_back(var); break;
              case PropertySource::kInstance: cbuffer.instance_vars.emplace_back(var); break;
              default: LOG_WARNING_LN("Unsupported property source for %s", name.c_str());
            }
          }
        }
      }
      cbuffer.handle = GRAPHICS.create_constant_buffer(FROM_HERE, size, true);
      cbuffer.staging.resize(size);

    } else if (shader->type() == ShaderType::kVertexShader && cur_line[0] == "Input" && cur_line[1] == "signature:") {

      // Parse input layout
      auto mapping = map_list_of
        ("SV_POSITION", DXGI_FORMAT_R32G32B32_FLOAT)
        ("POSITION", DXGI_FORMAT_R32G32B32_FLOAT)
        ("NORMAL", DXGI_FORMAT_R32G32B32_FLOAT)
        ("COLOR", DXGI_FORMAT_R32G32B32A32_FLOAT)
        ("TEXCOORD", DXGI_FORMAT_R32G32_FLOAT);

      vector<D3D11_INPUT_ELEMENT_DESC> inputs;
      for (i += 3; i < input.size(); ++i) {
        auto &row = input[i];
        if (row.size() != 7) {
          --i;  // rewind to the previous row
          break;
        }

        DXGI_FORMAT fmt;
        if (!lookup<string, DXGI_FORMAT>(row[0], mapping, &fmt)) {
          add_error_msg("Invalid input semantic found: %s", row[0].c_str());
          return false;
        }

        inputs.push_back(CD3D11_INPUT_ELEMENT_DESC(row[0].c_str(), atoi(row[1].c_str()), fmt, 0));
      }

      _input_layout = GRAPHICS.create_input_layout(FROM_HERE, inputs, obj);
      if (!_input_layout.is_valid()) {
        add_error_msg("Invalid input layout");
        return false;
      }
    } else if (cur_line[0] == "Resource" && cur_line[1] == "Bindings:") {

      for (i += 3; i < input.size(); ++i) {
        auto &row = input[i];
        if (row.size() != 6) {
          --i;  // rewind to the previous row
          break;
        }

        const string &name = row[0];
        const string &type = row[1];
        int bind_point = atoi(row[4].c_str());

        if (type == "texture") {
          ResourceViewParam *param = shader_template->find_resource_view(name.c_str());
          if (!param) {
            add_error_msg("Resource view %s not found", name.c_str());
            return false;
          }

          SparseUnknown &res = shader->_resource_views;
          res.first = min(bind_point, res.first);
          res.count = max(res.count, bind_point - res.first + 1);
          res.res.resize(max(bind_point + 1, (int)res.res.size()));
          res.res[bind_point].source = param->source;

          // the system resource has already been created, so we can grab it's handle right now
          string qualified_name = PropertySource::qualify_name(param->friendly_name.empty() ? param->name : param->friendly_name, param->source);
          if (param->source == PropertySource::kSystem) {
            //GraphicsObjectHandle h = GRAPHICS.find_resource(name);
            //assert(h.is_valid());
            *res.res[bind_point].pid() = PROPERTY_MANAGER.get_or_create<GraphicsObjectHandle>(qualified_name);
          } else {
            *res.res[bind_point].pid() = PROPERTY_MANAGER.get_or_create_placeholder(qualified_name);
          }

        } else if (type == "sampler") {

          auto &samplers = shader->_sampler_states;
          samplers.first = min(bind_point, samplers.first);
          samplers.count = max(samplers.count, bind_point - samplers.first + 1);
          samplers.res.resize(max(bind_point+1, (int)samplers.res.size()));
          samplers.res[bind_point] = PROPERTY_MANAGER.get_or_create_placeholder(name);
        }
      }
    }
  }

  return true;
}

GraphicsObjectHandle Technique::cbuffer_handle() {
  return _constant_buffers.empty() ? GraphicsObjectHandle() : _constant_buffers[0].handle;
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

  const char *profile = type == ShaderType::kVertexShader ? GRAPHICS.vs_profile() : GRAPHICS.ps_profile();

  char cmd_line[MAX_PATH];

  // create the defines for the current flags
  string defines;
  for (size_t i = 0; i < flags.size(); ++i) {
    defines.append("-D" + flags[i]);
    if (i != flags.size() - 1)
      defines.append(" ");
  }

#ifdef _DEBUG
  sprintf(cmd_line, "fxc.exe -nologo -T%s -E%s -Vi -O1 -Zi -Fo %s -Fh %s %s %s", 
    profile,
    entry_point,
    obj,
    Path::replace_extension(obj, "h").c_str(),
    defines.c_str(),
    src);
#else
  sprintf(cmd_line, "fxc.exe -nologo -T%s -E%s -Vi -O3 -Fo %s %s %s", 
    profile,
    entry_point,
    obj,
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
    int len = GetFileSize(stdout_read, NULL);
    char *buf = (char *)_alloca(len + 1);
    buf[len] = 0;
    DWORD bytes_read;
    ReadFile(stdout_read, buf, len, &bytes_read, NULL);
    add_error_msg(buf);
    LOG_WARNING_LN(buf);
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

  bool force = true;
  bool ok = true;

  const char *src = shader_template->_source_filename.c_str();
  const char *ep = shader_template->_entry_point.c_str();
  string output_base = strip_extension(shader_template->_obj_filename);
  string output_ext = get_extension(shader_template->_obj_filename);
  ShaderType::Enum type = shader_template->_type;

  vector<ShaderInstance> shaders;

  // Create all the shader permutations
  if (shader_template->_flags.empty()) {
    shaders.push_back(ShaderInstance(shader_template->_obj_filename));
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
    // compile the shader if the source is newer, or the object file doesn't exist
    if (force || RESOURCE_MANAGER.file_exists(src) && (!RESOURCE_MANAGER.file_exists(obj) || RESOURCE_MANAGER.mdate(src) > RESOURCE_MANAGER.mdate(obj))) {
      if (!compile_shader(type, ep, src, obj, cur.flags))
        return false;
    } else {
      if (!RESOURCE_MANAGER.file_exists(obj))
        return false;
    }

    vector<char> buf;
    if (!RESOURCE_MANAGER.load_file(obj, &buf))
      return false;

    int len = (int)buf.size();

    Shader *shader = new Shader(type);
#if _DEBUG
    shader->_entry_point = ep;
    shader->_source_filename = src;
    shader->_obj_filename = obj;
    shader->_flags = cur.flags;
#endif

    // load corresponding .h file
    vector<char> text;
    if (!RESOURCE_MANAGER.load_file(Path::replace_extension(obj, "h").c_str(), &text))
      return false;

    if (!do_reflection(text, shader, shader_template, buf))
      return false;

    shader->prepare_cbuffers();

    switch (shader->type()) {
      case ShaderType::kVertexShader: 
        shader->_handle = GRAPHICS.create_vertex_shader(FROM_HERE, buf, obj);
        _vertex_shaders.push_back(shader);
        break;
      case ShaderType::kPixelShader: 
        shader->_handle = GRAPHICS.create_pixel_shader(FROM_HERE, buf, obj);
        _pixel_shaders.push_back(shader);
        break;
      case ShaderType::kGeometryShader: break;
    }
  }
  return true;
}

bool Technique::init() {

  _rasterizer_state = GRAPHICS.create_rasterizer_state(FROM_HERE, _rasterizer_desc);
  _blend_state = GRAPHICS.create_blend_state(FROM_HERE, _blend_desc);
  _depth_stencil_state = GRAPHICS.create_depth_stencil_state(FROM_HERE, _depth_stencil_desc);

  // create all the sampler states from the descs
  for (auto i = begin(_sampler_descs), e = end(_sampler_descs); i != e; ++i) {
    PropertyId id = PROPERTY_MANAGER.get_or_create_placeholder(i->first);
    _sampler_states.push_back(make_pair(id, GRAPHICS.create_sampler_state(FROM_HERE, i->second)));
  }

  // create the shaders from the templates
  if (_vs_shader_template) {
    if (!create_shaders(_vs_shader_template))
      return false;
  }

  if (_ps_shader_template) {
    if (!create_shaders(_ps_shader_template))
      return false;
  }

  for (size_t i = 0; i < _vertex_shaders.size(); ++i) {
    if (!_vertex_shaders[i]->on_loaded())
      return false;
  }

  for (size_t i = 0; i<  _pixel_shaders.size(); ++i) {
    if (!_pixel_shaders[i]->on_loaded())
      return false;
  }

  return true;
}

void Technique::fill_cbuffer(CBuffer *cbuffer) const {
  for (size_t i = 0; i < cbuffer->system_vars.size(); ++i) {
    auto &cur = cbuffer->system_vars[i];
    PROPERTY_MANAGER.get_property_raw(cur.id, &cbuffer->staging[cur.ofs], cur.len);
  }
}

void Technique::fill_samplers(const SparseProperty& input, std::vector<GraphicsObjectHandle> *out) const {
  out->resize(input.res.size());
  for (size_t i = 0; i < input.res.size(); ++i) {
    for (size_t j = 0; j < _sampler_states.size(); ++j) {
      if (input.res[i] == _sampler_states[j].first) {
        (*out)[i] = _sampler_states[j].second;
      }
    }
  }
}

void Technique::fill_resource_views(const SparseUnknown &props, std::vector<GraphicsObjectHandle> *out) const {

  out->resize(props.res.size());

  for (size_t i = 0; i < props.res.size(); ++i) {
    auto &cur = props.res[i];
    if (cur.source == PropertySource::kSystem) {
      auto goh = PROPERTY_MANAGER.get_property<GraphicsObjectHandle>(*cur.pid());
      (*out)[i] = goh;
    }
  }
}
