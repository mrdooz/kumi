#include "stdafx.h"
#include "shader.hpp"
#include "path_utils.hpp"
#include "utils.hpp"
#include "graphics.hpp"

using namespace boost::assign;
using namespace std;

string Shader::id() const {
  Path p(_source_filename);
  return p.get_filename_without_ext() + "::" + _entry_point;
}

void Shader::prune_unused_parameters() {
  auto not_used = [&](const ParamBase &param) { return !param.used; };
  //erase_remove_if(&_cbuffer_params, r);
  //erase_remove_if(&_resource_view_params, r);
}

void Shader::prepare_cbuffers() {
  prune_unused_parameters();
}

bool Shader::on_loaded() {

  _resource_views.res.resize(_resource_view_names.size());
  for (int i = 0; i < (int)_resource_view_names.size(); ++i) {
    auto &cur = _resource_view_names[i];
    if (!cur.empty()) {
      _resource_views.first = min(_resource_views.first, i);
      _resource_views.count = i - _resource_views.first + 1;
      _resource_views.res[i] = PROPERTY_MANAGER.get_or_create_placeholder(cur);
    }
  }

  _sampler_states.res.resize(_sampler_names.size());
  for (int i = 0; i < (int)_sampler_names.size(); ++i) {
    auto &cur = _sampler_names[i];
    if (!cur.empty()) {
      _sampler_states.first = min(_sampler_states.first, i);
      _sampler_states.count = i - _sampler_states.first + 1;
      _sampler_states.res[i] = PROPERTY_MANAGER.get_or_create_placeholder(cur);
    }
  }

  return true;
}

const SparseProperty &Shader::samplers() const {
  return _sampler_states;
}

const SparseProperty &Shader::resource_views() const {
  return _resource_views;
}
