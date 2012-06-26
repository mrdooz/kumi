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
  //, _first_sampler(MAX_SAMPLERS)
  //, _num_valid_samplers(0)
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

#if 0
bool Technique::parse_input_layout(ID3D11ShaderReflection* reflector, const D3D11_SHADER_DESC &shader_desc, void *buf, size_t len) {
  auto mapping = map_list_of
    ("SV_POSITION", DXGI_FORMAT_R32G32B32_FLOAT)
    ("POSITION", DXGI_FORMAT_R32G32B32_FLOAT)
    ("NORMAL", DXGI_FORMAT_R32G32B32_FLOAT)
    ("COLOR", DXGI_FORMAT_R32G32B32A32_FLOAT)
    ("TEXCOORD", DXGI_FORMAT_R32G32_FLOAT);

  // ShaderInputs inputs;
  vector<D3D11_INPUT_ELEMENT_DESC> inputs;
  for (UINT i = 0; i < shader_desc.InputParameters; ++i) {
    D3D11_SIGNATURE_PARAMETER_DESC input;
    reflector->GetInputParameterDesc(i, &input);

    DXGI_FORMAT fmt;
    if (!lookup<string, DXGI_FORMAT>(input.SemanticName, mapping, &fmt)) {
      add_error_msg("Invalid input semantic found: %s", input.SemanticName);
      return false;
    }

    inputs.push_back(CD3D11_INPUT_ELEMENT_DESC(input.SemanticName, input.SemanticIndex, fmt, input.Stream));
  }

  _input_layout = GRAPHICS.create_input_layout(FROM_HERE, &inputs[0], inputs.size(), buf, len);
  if (!_input_layout.is_valid())
    add_error_msg("Invalid input layout");

  return _input_layout.is_valid();
}
#endif
/*
CBufferParam *Technique::find_cbuffer_param(const std::string &name, ShaderType::Enum type) {
  vector<CBufferParam> *params = type == ShaderType::kVertexShader ? &_vs_cbuffer_params : &_ps_cbuffer_params;
  for (auto it = begin(*params); it != end(*params); ++it) {
    if (it->name == name)
      return &(*it);
  }
  return nullptr;
}
*/
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
              default: LOG_WARNING_LN("Unsupported property source for %s", name.c_str());
            }
            param->used = true;
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

      _input_layout = GRAPHICS.create_input_layout(FROM_HERE, &inputs[0], inputs.size(), obj.data(), obj.size());
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
          param->bind_point = bind_point;
          param->used = true;
          string qualified_name = PropertySource::to_string(param->source) + "::" + 
            (param->friendly_name.empty() ? param->name : param->friendly_name);
          shader->_resource_view_names.resize(max((int)shader->_resource_view_names.size(), bind_point+1));
          shader->_resource_view_names[bind_point] = qualified_name;

        } else if (type == "sampler") {
          shader->_sampler_names.resize(max((int)shader->_sampler_names.size(), bind_point+1));
          shader->_sampler_names[bind_point] = name;
/*
          _first_sampler = min(_first_sampler, bind_point);
          _num_valid_samplers = max(bind_point - _first_sampler + 1, _num_valid_samplers);
          // find out where this sampler goes..
          for (auto i = begin(_sampler_states), e = end(_sampler_states); i != e; ++i) {
            if (i->first == name) {
              _ordered_sampler_states[bind_point] = i->second;
              goto FOUND_SAMPLER;
            }
          }
          add_error_msg("Sampler %s not found", name.c_str());
FOUND_SAMPLER:;
          shader->_sampler_names.resize(max((int)shader->_sampler_names.size(), bind_point+1));
          shader->_sampler_names[bind_point] = name;
*/
        }

      }
    }
  }

  return true;
}

#if 0
bool Technique::do_reflection(Shader *shader, void *buf, size_t len)
{
  ID3D11ShaderReflection* reflector = NULL; 
  B_ERR_HR(D3DReflect(buf, len, IID_ID3D11ShaderReflection, (void**)&reflector));
  D3D11_SHADER_DESC shader_desc;
  reflector->GetDesc(&shader_desc);

  // Parse input layout for vertex shaders
  if (shader->type() == ShaderType::kVertexShader && !parse_input_layout(reflector, shader_desc, buf, len))
    return false;
/*
  // Parse shader interfaces
  if (int num_interfaces = reflector->GetNumInterfaceSlots()) {

    // Get the variable names from the constant buffers
    vector<string> interfaces;
    for (UINT i = 0; i < shader_desc.ConstantBuffers; ++i) {
      ID3D11ShaderReflectionConstantBuffer* cb = reflector->GetConstantBufferByIndex(i);
      D3D11_SHADER_BUFFER_DESC cb_desc;
      cb->GetDesc(&cb_desc);
      // skip if not an interface ptr cbuffer
      if (cb_desc.Type != D3D11_CT_INTERFACE_POINTERS)
        continue;
      for (UINT j = 0; j < cb_desc.Variables; ++j) {
        ID3D11ShaderReflectionVariable* v = cb->GetVariableByIndex(j);
        D3D11_SHADER_VARIABLE_DESC var_desc;
        v->GetDesc(&var_desc);
        if (var_desc.uFlags & (D3D_SVF_USED | D3D_SVF_INTERFACE_POINTER)) {
          int ofs = v->GetInterfaceSlot(0);
          interfaces.push_back(var_desc.Name);

          CComPtr<ID3D11ClassInstance> inst;
          GRAPHICS.get_class_linkage()->GetClassInstance("g_DiffuseColor", 0, &inst.p);

          D3D11_CLASS_INSTANCE_DESC instance_desc;
          inst->GetDesc(&instance_desc);
          TCHAR instance_name[256];
          TCHAR type_name[256];
          SIZE_T instance_name_len, type_name_len;
          inst->GetInstanceName(instance_name, &instance_name_len);
          inst->GetTypeName(type_name, &type_name_len);
          int x = 10;
        }
      }
      int a = 10;
    }

    ID3D11ClassInstance **linkage_array = (ID3D11ClassInstance **)_alloca(sizeof(ID3D11ClassInstance *) * num_interfaces);
  }
*/
  // The technique contains both vs and ps cbuffers
  for (UINT i = 0; i < shader_desc.ConstantBuffers; ++i) {
    ID3D11ShaderReflectionConstantBuffer* cb = reflector->GetConstantBufferByIndex(i);
    D3D11_SHADER_BUFFER_DESC cb_desc;
    cb->GetDesc(&cb_desc);

    // skip if not a "true" cbuffer
    if (cb_desc.Type != D3D11_CT_CBUFFER)
      continue;

    CBuffer cbuffer;
    int size = 0;

    for (UINT j = 0; j < cb_desc.Variables; ++j) {
      ID3D11ShaderReflectionVariable* v = cb->GetVariableByIndex(j);
      D3D11_SHADER_VARIABLE_DESC var_desc;
      v->GetDesc(&var_desc);
      if (var_desc.uFlags & D3D_SVF_USED) {
        // Get the parameter from the shader (as specified in the .tec file)
        CBufferParam *param = find_cbuffer_param(var_desc.Name, shader->type());
        if (!param) {
          add_error_msg("Shader parameter %s not found", var_desc.Name);
          return false;
        }

        CBufferVariable var;
        var.ofs = var_desc.StartOffset;
        var.len = var_desc.Size;
        size = max(size, var.ofs + var .len);
        // The id is either a classifer (for mesh/material), or a real id in the system case
        string class_id = PropertySource::to_string(param->source) + "::" + var_desc.Name;
        int var_size = param->source == PropertySource::kMesh ? sizeof(int) : var_desc.Size;
        var.id = PROPERTY_MANAGER.get_or_create_raw(class_id.c_str(), var_size, nullptr);
#ifdef _DEBUG
        var.name = class_id;
#endif
        switch (param->source) {
          case PropertySource::kMaterial: cbuffer.material_vars.emplace_back(var); break;
          case PropertySource::kMesh: cbuffer.mesh_vars.emplace_back(var); break;
          case PropertySource::kSystem: cbuffer.system_vars.emplace_back(var); break;
          default: LOG_WARNING_LN("Unsupported property source for %s", var_desc.Name);
        }
        param->used = true;
      }
    }

    cbuffer.handle = GRAPHICS.create_constant_buffer(FROM_HERE, size, true);
    cbuffer.staging.resize(size);
    if (shader->type() == ShaderType::kVertexShader)
      _vs_cbuffers.emplace_back(cbuffer);
    else
      _ps_cbuffers.emplace_back(cbuffer);
  }

  // get bound resources (textures and buffers)
  for (UINT i = 0; i < shader_desc.BoundResources; ++i) {
    D3D11_SHADER_INPUT_BIND_DESC input_desc;
    reflector->GetResourceBindingDesc(i, &input_desc);
    if (input_desc.BindCount > 1) {
      add_error_msg("Only bind counts of 1 are supported");
      return false;
    }

    switch (input_desc.Type) {

      case D3D10_SIT_TEXTURE: {
        ResourceViewParam *param = shader->find_resource_view_param(input_desc.Name);
        if (!param) {
          add_error_msg("Resource view %s not found", input_desc.Name);
          return false;
        }
        param->bind_point = input_desc.BindPoint;
        param->used = true;
        break;
      }

      case D3D10_SIT_SAMPLER: {
        _first_sampler = min(_first_sampler, (int)input_desc.BindPoint);
        _num_valid_samplers = max((int)input_desc.BindPoint - _first_sampler + 1, _num_valid_samplers);
        // find out where this sampler goes..
        for (auto i = begin(_sampler_states), e = end(_sampler_states); i != e; ++i) {
          if (i->first == input_desc.Name) {
            _ordered_sampler_states[input_desc.BindPoint] = i->second;
            goto FOUND_SAMPLER;
          }
        }
        add_error_msg("Sampler %s not found", input_desc.Name);
        FOUND_SAMPLER:
        break;
      }
    }
  }
  return true;
}
#endif
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
    entry_point.c_str(),
    obj.c_str(), 
    defines.c_str(),
    src.c_str());
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

bool Technique::create_shaders(ResourceInterface *res, ShaderTemplate *shader_template) {

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
    if (force || res->file_exists(src) && (!res->file_exists(obj) || res->mdate(src) > res->mdate(obj))) {
      if (!compile_shader(type, ep, src, obj, cur.flags))
        return false;
    } else {
      if (!res->file_exists(obj))
        return false;
    }

    vector<char> buf;
    if (!res->load_file(obj, &buf))
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
    if (!res->load_file(Path::replace_extension(obj, "h").c_str(), &text))
      return false;

    if (!do_reflection(text, shader, shader_template, buf))
      return false;

    shader->prepare_cbuffers();

    switch (shader->type()) {
      case ShaderType::kVertexShader: 
        shader->set_handle(GRAPHICS.create_vertex_shader(FROM_HERE, buf.data(), len, shader->id()));
        _vertex_shaders.push_back(shader);
        break;
      case ShaderType::kPixelShader: 
        shader->set_handle(GRAPHICS.create_pixel_shader(FROM_HERE, buf.data(), len, shader->id()));
        _pixel_shaders.push_back(shader);
        break;
      case ShaderType::kGeometryShader: break;
    }
  }
#if 0
/*
    const char *obj = shader->_obj_filename.c_str();
    // compile the shader if the source is newer, or the object file doesn't exist
    if (force || res->file_exists(src) && (!res->file_exists(obj) || res->mdate(src) > res->mdate(obj))) {
      if (!compile_shader(shader->_type, shader->_entry_point.c_str(), src, shader->_obj_filename.c_str(), vector<string>()))
        return false;
    } else {
      if (!res->file_exists(obj))
        return false;
    }

  } else {
    size_t num_flags = shader->_flags.size();
    // generate flags combinations
    for (int i = 0; i < (1 << num_flags); ++i) {
      vector<string> flags;
      string filename_suffix;
      for (size_t j = 0; j < num_flags; ++j) {
        if (i & (1 << j)) {
          flags.push_back(shader->_flags[j]);
          filename_suffix.append("1");
        } else {
          filename_suffix.append("0");
        }
      }

      string obj = output_base + filename_suffix + "." + ext;
      // compile the shader if the source is newer, or the object file doesn't exist
      if (force || res->file_exists(src) && (!res->file_exists(obj.c_str()) || res->mdate(src) > res->mdate(obj.c_str()))) {
        if (!compile_shader(shader->_type, shader->_entry_point.c_str(), src, obj.c_str(), flags))
          return false;
      } else {
        if (!res->file_exists(obj.c_str()))
          return false;
      }

    }
  }
*/
/*
  const char *obj = shader->obj_filename().c_str();
  const char *src = shader->source_filename().c_str();
  // compile the shader if the source is newer, or the object file doesn't exist
  if (force || res->file_exists(src) && (!res->file_exists(obj) || res->mdate(src) > res->mdate(obj))) {
    if (!compile_shader(shader))
      return false;
  } else {
    if (!res->file_exists(obj))
      return false;
  }
*/
/*
  // load compiled shaders
  vector<uint8> buf;
  if (!res->load_file(obj, &buf))
    return false;

  int len = (int)buf.size();

  // load corresponding .h file
  vector<uint8> text;
  if (!res->load_file(Path::replace_extension(obj, "h").c_str(), &text))
    return false;
  do_reflection((const char *)text.data(), (const char *)text.data() + text.size(), shader, buf.data(), len);
/*
  if (!do_reflection(graphics, shader, buf.data(), len))
    return false;
*/
  shader->prune_unused_parameters();

  switch (shader->type()) {
    case ShaderType::kVertexShader: 
      shader->set_handle(GRAPHICS.create_vertex_shader(FROM_HERE, buf.data(), len, shader->id()));
      break;
    case Shader::kPixelShader: 
      shader->set_handle(GRAPHICS.create_pixel_shader(FROM_HERE, buf.data(), len, shader->id()));
      break;
    case Shader::kGeometryShader: break;
  }

  shader->validate();
  // check that our samplers are sequential
  if (_ordered_sampler_states[_first_sampler].is_valid()) {
    bool prev_valid = true;
    for (int i = _first_sampler + 1; i < MAX_SAMPLERS; ++i) {
      bool cur_valid = _ordered_sampler_states[i].is_valid();
      if (cur_valid && !prev_valid) {
        add_error_msg("Nonsequential sampler states found!");
      }
      prev_valid = cur_valid;
    }
  }
#endif
  return true;
}

bool Technique::init(ResourceInterface *res) {

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
    if (!create_shaders(res, _vs_shader_template))
      return false;
  }

  if (_ps_shader_template) {
    if (!create_shaders(res, _ps_shader_template))
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

  prepare_textures();

  return true;
}
/*
GraphicsObjectHandle Technique::sampler_state(const char *name) const {
  for (auto it = begin(_sampler_states), e = end(_sampler_states); it != e; ++it) {
    if (it->first == name)
      return it->second;
  }
  return GraphicsObjectHandle();
}
*/
/*
void Technique::get_render_objects(RenderObjects *obj, int vs_flags, int ps_flags) {
  obj->vs = _vertex_shaders[vs_flags]->handle();
  obj->gs = GraphicsObjectHandle();
  obj->ps = _pixel_shaders[ps_flags]->handle();
  obj->layout = _input_layout;
  obj->rs = _rasterizer_state;
  obj->dss = _depth_stencil_state;
  obj->bs = _blend_state;
  obj->stencil_ref = GRAPHICS.default_stencil_ref();
  memcpy(obj->blend_factors, GRAPHICS.default_blend_factors(), sizeof(obj->blend_factors));
  memcpy(obj->samplers, &_ordered_sampler_states[_first_sampler], _num_valid_samplers * sizeof(GraphicsObjectHandle));
  obj->first_sampler = _first_sampler;
  obj->num_valid_samplers = _num_valid_samplers;
  obj->sample_mask = GRAPHICS.default_sample_mask();
}
*/

void Technique::prepare_textures() {
  // TODO
/*
  auto &params = _pixel_shader->resource_view_params();
  for (auto it = begin(params), e = end(params); it != e; ++it) {
    auto &name = it->name;
    auto &friendly_name = it->friendly_name;
    int bind_point = it->bind_point;
    _render_data.textures[bind_point] = GRAPHICS.find_resource(friendly_name.empty() ? name.c_str() : friendly_name.c_str());
    _render_data.first_texture = min(_render_data.first_texture, bind_point);
    _render_data.num_textures = max(_render_data.num_textures, bind_point - _render_data.first_texture + 1);
  }
*/
}

void Technique::fill_cbuffer(CBuffer *cbuffer) {
  for (size_t i = 0; i < cbuffer->system_vars.size(); ++i) {
    auto &cur = cbuffer->system_vars[i];
    PROPERTY_MANAGER.get_property_raw(cur.id, &cbuffer->staging[cur.ofs], cur.len);
  }
}

void Technique::fill_samplers(const SparseProperty& input, std::vector<GraphicsObjectHandle> *out) {
  out->resize(input.res.size());
  for (size_t i = 0; i < input.res.size(); ++i) {
    for (size_t j = 0; j < _sampler_states.size(); ++j) {
      if (input.res[i] == _sampler_states[j].first) {
        (*out)[i] = _sampler_states[j].second;
      }
    }
  }
}
