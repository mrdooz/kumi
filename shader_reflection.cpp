#include "stdafx.h"
#include "shader_reflection.hpp"
#include "property.hpp"
#include "string_utils.hpp"
#include "shader.hpp"
#include "graphics.hpp"
#include "logger.hpp"
#include "dx_utils.hpp"

using namespace std;
using namespace boost::assign;

struct Substring {
  Substring(const char *start, size_t len) : start(start), len(len) {}

  bool operator==(const char *rhs) {
    int rhsLen = strlen(rhs);
    if (len != rhsLen)
      return false;

    for (size_t i = 0; i < len; ++i) {
      if (start[i] != rhs[i])
        return false;
    }
    return true;
  }
  const char *c_str() { return start; }
  std::string to_string() { return string(start, len); }

  const char* start;
  size_t len;
};

static const char *skip_line(const char *start, const char *end) {
  while (start < end && *start != '\r' && *start != '\n')
    ++start;

  while (start < end && *start == '\r' || *start == '\n')
    ++start;

  return start;
}

static vector<Substring> split(char *start, const char *end) {

  // Note! split will NULL-terminate the string at delimiters
  vector<Substring> res;

  while (start < end) {
    char *tmp = start;
    while (!isspace((int)*tmp))
      ++tmp;

    res.emplace_back(Substring(start, tmp-start));

    // NULL-terminate the string.
    // Note, we must increment the ptr here to skip the first iteration
    // of the isspace loop below
    *tmp++ = '\0';

    while (tmp < end && isspace((int)*tmp))
      ++tmp;

    start = tmp;
  }

  return res;
}

static void trim_input(char *text, int textLen, vector<vector<Substring>> *res) {

  char *start = text;
  const char *end = text + textLen;
  // extract the relevant text
  if (strstr(start, "#if 0"))
    start = (char *)skip_line(start, end);

  while (start < end) {
    // skip leading comments and whitespace
    if (*start != '/')
      break;
    while (*start == '/' || *start == ' ')
      ++start;

    // read until "\r\n" found
    char *tmp = start;
    while (*tmp != '\r')
      ++tmp;
    if (tmp - start > 0)
      res->emplace_back(split(start, tmp));
    while (tmp < end && (*tmp == '\0' || *tmp == '\r' || *tmp == '\n'))
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

bool ShaderReflection::do_reflection(char *text, int textLen, Shader *shader, ShaderTemplate *shader_template, const vector<char> &obj) {

  // extract the relevant text
  vector<vector<Substring>> input;
  trim_input(text, textLen, &input);

  set<string> cbuffers_found;

  // parse it!
  for (size_t i = 0; i < input.size(); ++i) {
    auto &cur_line = input[i];

    if (cur_line[0] == "cbuffer") {
      string cbuffer_name(cur_line[1].to_string());
      CBuffer cbuffer;
      auto source = source_from_name(cbuffer_name);
      int size = 0;

      for (i += 2; i < input.size(); ++i) {
        auto &row = input[i];
        if (row[0] == "}")
          break;
        if (row.size() == 7 || row.size() == 8) {
          bool unused = row.size() == 8 && row[7] == "[unused]";
          if (!unused) {
            string name = string(row[1].start, row[1].len - 1); // skip trailing ';'
            // strip any []
            int bracket = name.find('[');
            if (bracket != name.npos) {
              name = string(name.c_str(), bracket);
            }

            // Get the parameter source from the cbuffer name
            auto source = source_from_name(cbuffer_name);

            // If the cbuffer isn't qualified with a source name, the just assume that
            // it's going to be filled in by hand

            if (source == PropertySource::kUnknown) {
              LOG_INFO_LN("Untyped cbuffer found: %s", cbuffer_name.c_str());
            }

            if (cbuffers_found.find(cbuffer_name) != cbuffers_found.end()) {
              LOG_ERROR_LN("Found duplicate cbuffer: %s", cbuffer_name.c_str());
              return false;
            }

            int ofs = atoi(row[4].c_str());
            int len = atoi(row[6].c_str());
            size = max(size, ofs + len);
            PropertyId id = ~0;
            string class_id;
            if (source != PropertySource::kUnknown) {
              // The id is either a classifer (for mesh/material), or a real id in the system case
              class_id = PropertySource::qualify_name(name, source);
              int var_size = source == PropertySource::kMesh ? sizeof(int) : len;
              id = PROPERTY_MANAGER.get_or_create_raw(class_id.c_str(), var_size, nullptr);
            }
            CBufferVariable var(ofs, len, id);
#ifdef _DEBUG
            var.name = class_id.empty() ? name : class_id;
#endif
            cbuffer.vars.emplace_back(var);
          }
        }
      }

      if (size > 0) {
        cbuffer.handle = GRAPHICS.create_buffer(FROM_HERE, D3D11_BIND_CONSTANT_BUFFER, size, true, nullptr);
        cbuffer.staging.resize(size);

        if (source == PropertySource::kUnknown) {
          shader->add_named_cbuffer(cbuffer_name.c_str(), cbuffer);

        } else {
          shader->set_cbuffer(source, cbuffer);
        }
      }

    } else if (shader->type() == ShaderType::kVertexShader && cur_line[0] == "Input" && cur_line[1] == "signature:") {

      // Parse input layout
      auto mapping = map_list_of
        ("SV_POSITION", DXGI_FORMAT_R32G32B32A32_FLOAT)
        ("POSITION", DXGI_FORMAT_R32G32B32_FLOAT)
        ("NORMAL", DXGI_FORMAT_R32G32B32_FLOAT)
        ("COLOR", DXGI_FORMAT_R32G32B32A32_FLOAT)
        ("SIZE", DXGI_FORMAT_R32_FLOAT)
        ("TEXCOORD", DXGI_FORMAT_R32G32_FLOAT);

      vector<D3D11_INPUT_ELEMENT_DESC> inputs;
      for (i += 3; i < input.size(); ++i) {
        auto &row = input[i];
        if (row.size() != 7) {
          --i;  // rewind to the previous row
          break;
        }

        DXGI_FORMAT fmt;
        if (!lookup<string, DXGI_FORMAT>(row[0].to_string(), mapping, &fmt)) {
          LOG_ERROR_LN("Invalid input semantic found: %s", row[0].c_str());
          return false;
        }

        inputs.push_back(CD3D11_INPUT_ELEMENT_DESC(row[0].c_str(), atoi(row[1].c_str()), fmt, 0));
      }

      shader->_input_layout = GRAPHICS.create_input_layout(FROM_HERE, inputs, obj);
      if (!shader->_input_layout.is_valid()) {
        LOG_ERROR_LN("Invalid input layout");
        return false;
      }
    } else if (cur_line[0] == "Resource" && cur_line[1] == "Bindings:") {

      for (i += 3; i < input.size(); ++i) {
        auto &row = input[i];
        if (row.size() != 6) {
          --i;  // rewind to the previous row
          break;
        }

        const string &name = row[0].to_string();
        const string &type = row[1].to_string();
        int bind_point = atoi(row[4].c_str());

        if (type == "cbuffer") {
          // arrange the cbuffer in the correct slot
          shader->set_cbuffer_slot(source_from_name(name), bind_point);

        } else if (type == "texture") {
          ResourceViewParam *param = shader_template->find_resource_view(name.c_str());
          // If we can't find a resource, assume it's going to be set by hand
          if (!param) {
            LOG_INFO_LN("Found unbound resource: %s", name.c_str());
          } else {
            string &friendly = param->friendly_name;
            string qualified_name = PropertySource::qualify_name(friendly.empty() ? param->name : friendly, param->source);

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
