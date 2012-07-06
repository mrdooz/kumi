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
  return true;
}

