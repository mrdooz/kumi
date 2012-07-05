#include "stdafx.h"
#include "shader.hpp"
#include "path_utils.hpp"
#include "utils.hpp"
#include "graphics.hpp"

using namespace std;

Shader::Shader(ShaderType::Enum type) 
  : _type(type), _valid(false) 
{
  _samplers.resize(MAX_SAMPLERS);
}

void Shader::prepare_cbuffers() {
}

bool Shader::on_loaded() {
  return true;
}

