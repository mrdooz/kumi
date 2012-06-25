#include "stdafx.h"
#include "shader.hpp"
#include "path_utils.hpp"
#include "utils.hpp"

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
