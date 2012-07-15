#include "stdafx.h"
#include "shader.hpp"
#include "path_utils.hpp"
#include "utils.hpp"
#include "graphics.hpp"

using namespace std;

Shader::Shader(ShaderType::Enum type) 
  : _type(type)
{
}

bool Shader::on_loaded() {
  for (size_t i = 0; i < _cbuffers.size(); ++i) {
    _cbuffers[i].init();
  }
  return true;
}

