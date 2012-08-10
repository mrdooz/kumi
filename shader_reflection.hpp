#pragma once

class Shader;
struct ShaderTemplate;

class ShaderReflection {
public:
  bool do_reflection(char *text, int textLen, Shader *shader, ShaderTemplate *shader_template, const std::vector<char> &obj);
};
