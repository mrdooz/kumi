#include "stdafx.h"
#include "shader.hpp"
#include "path_utils.hpp"

using namespace std;

string Shader::id() const {
  Path p(_source_filename);
  return p.get_filename_without_ext() + "::" + _entry_point;
}
