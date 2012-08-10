#pragma once

class Shader;
struct ShaderTemplate;

class ShaderReflection {
public:
  bool do_reflection(const std::vector<char> &text, Shader *shader, ShaderTemplate *shader_template, const std::vector<char> &obj);
};
