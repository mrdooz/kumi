#include "stdafx.h"
#include "shader.hpp"
#include "path_utils.hpp"
#include "utils.hpp"
#include "graphics.hpp"

using namespace std;

void Shader::prepare_cbuffers() {
}

bool Shader::on_loaded() {
  return true;
}

const SparseProperty &Shader::samplers() const {
  return _sampler_states;
}

const SparseUnknown &Shader::resource_views() const {
  return _resource_views;
}
