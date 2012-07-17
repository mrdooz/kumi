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
  : _vertex_size(-1)
  , _index_format(DXGI_FORMAT_UNKNOWN)
  , _valid(false)
  , _vs_shader_template(nullptr)
  , _ps_shader_template(nullptr)
  , _vs_flag_mask(0)
  , _ps_flag_mask(0)
{
}

Technique::~Technique() {
  seq_delete(&_vertex_shaders);
  seq_delete(&_pixel_shaders);
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
  _vs_shader_template = parent->_vs_shader_template;
  _ps_shader_template = parent->_ps_shader_template;

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

static PropertySource::Enum source_from_name(const std::string &cbuffer_name) {

  static struct {
    string prefix;
    PropertySource::Enum src;
  } prefixes[] = {
    { "PerFrame", PropertySource::kSystem },
    { "PerMaterial", PropertySource::kMaterial},
    { "PerInstance", PropertySource::kInstance},
    { "PerMesh", PropertySource::kMesh},
  };

  for (int i = 0; i < ARRAYSIZE(prefixes); ++i) {
    if (begins_with(cbuffer_name, prefixes[i].prefix))
      return prefixes[i].src;
  }

  return PropertySource::kUnknown;
}

bool Technique::do_reflection(const std::vector<char> &text, Shader *shader, ShaderTemplate *shader_template, const vector<char> &obj) {

  // extract the relevant text
  vector<vector<string>> input;
  trim_input(text, &input);

  set<string> cbuffers_found;

  // parse it!
  for (size_t i = 0; i < input.size(); ++i) {
    auto &cur_line = input[i];

    if (cur_line[0] == "cbuffer") {
      string cbuffer_name(cur_line[1]);
      // Parse cbuffer
      CBuffer *cbuffer = nullptr;
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

            // Get the parameter source from the cbuffer name
            auto source = source_from_name(cbuffer_name);
            if (source == PropertySource::kUnknown) {
              add_error_msg("Unable to deduce source from cbuffer name: %s", cbuffer_name.c_str());
              return false;
            }

            if (cbuffers_found.find(cbuffer_name) != cbuffers_found.end()) {
              add_error_msg("Found duplicate cbuffer: %s", cbuffer_name.c_str());
              return false;
            }

            switch (source) {
              case PropertySource::kMaterial: cbuffer = &shader->_material_cbuffer; break;
              case PropertySource::kMesh: cbuffer = &shader->_mesh_cbuffer; break;
              case PropertySource::kSystem: cbuffer = &shader->_system_cbuffer; break;
              case PropertySource::kInstance: cbuffer = &shader->_instance_cbuffer; break;
            }

            int ofs = atoi(row[4].c_str());
            int len = atoi(row[6].c_str());
            size = max(size, ofs + len);
            // The id is either a classifer (for mesh/material), or a real id in the system case
            string class_id = PropertySource::qualify_name(name, source);
            int var_size = source == PropertySource::kMesh ? sizeof(int) : len;
            auto id = PROPERTY_MANAGER.get_or_create_raw(class_id.c_str(), var_size, nullptr);
            CBufferVariable var(ofs, len, id);
#ifdef _DEBUG
            var.name = class_id;
#endif
            cbuffer->vars.emplace_back(var);
          }
        }
      }
      cbuffer->handle = GRAPHICS.create_constant_buffer(FROM_HERE, size, true);
      cbuffer->staging.resize(size);

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

        if (type == "cbuffer") {
          // arrange the cbuffer in the correct slot
          shader->set_cbuffer_slot(source_from_name(name), bind_point);

        } else if (type == "texture") {
          ResourceViewParam *param = shader_template->find_resource_view(name.c_str());
          if (!param) {
            add_error_msg("Resource view %s not found", name.c_str());
            return false;
          }

          string qualified_name = PropertySource::qualify_name(param->friendly_name.empty() ? param->name : param->friendly_name, param->source);

          if (param->source == PropertySource::kMaterial) {
            auto pid = PROPERTY_MANAGER.get_or_create_placeholder(qualified_name);
            shader->_resource_views2.material_views.emplace_back(ResourceId<PropertyId>(pid, bind_point, name));

          } else if (param->source == PropertySource::kSystem) {
            GraphicsObjectHandle h = GRAPHICS.find_resource(name);
            shader->_resource_views2.system_views.emplace_back(ResourceId<GraphicsObjectHandle>(h, bind_point, name));

          } else if (param->source == PropertySource::kUser) {
            shader->_resource_views2.user_views.push_back(bind_point);

          } else {
            LOG_WARNING_LN("Unsupported property source for texture: %s", name.c_str());
          }

          auto &rv = shader->_resource_views;
          rv[bind_point].source = param->source;
          rv[bind_point].used = true;

          if (param->source == PropertySource::kSystem) {
            rv[bind_point].class_id = PROPERTY_MANAGER.get_or_create<GraphicsObjectHandle>(qualified_name);
          } else {
            rv[bind_point].class_id = PROPERTY_MANAGER.get_or_create_placeholder(qualified_name);
          }

        } else if (type == "sampler") {
          GraphicsObjectHandle sampler = GRAPHICS.find_sampler(name);
          KASSERT(sampler.is_valid());
          shader->_samplers[bind_point] = sampler;
          //shader->_first_sampler = min(shader->_first_sampler, bind_point);
        }
      }
    }
  }

  return true;
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
      LOG_WARNING_LN(buf);
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
    shader->_source_filename = src;
#if _DEBUG
    shader->_entry_point = ep;
    shader->_obj_filename = obj;
    shader->_flags = cur.flags;
#endif

    // load corresponding .h file
    vector<char> text;
    if (!RESOURCE_MANAGER.load_file(Path::replace_extension(obj, "h").c_str(), &text))
      return false;

    if (!do_reflection(text, shader, shader_template, buf))
      return false;

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
  if (!_rasterizer_state.is_valid())
    _rasterizer_state = GRAPHICS.default_rasterizer_state();

  if (!_blend_state.is_valid())
    _blend_state = GRAPHICS.default_blend_state();

  if (!_depth_stencil_state.is_valid())
    _depth_stencil_state = GRAPHICS.default_depth_stencil_state();

  // create the shaders from the templates
  if (_vs_shader_template) {
    if (!create_shaders(_vs_shader_template.get()))
      return false;
  }

  if (_ps_shader_template) {
    if (!create_shaders(_ps_shader_template.get()))
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

