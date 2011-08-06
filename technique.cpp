#include "stdafx.h"
#include "technique.hpp"
#include "technique_parser.hpp"
#include "io.hpp"
#include "file_utils.hpp"
#include "graphics.hpp"
#include "dx_utils.hpp"

using std::make_pair;
using namespace boost::assign;
/*
struct ShaderInput {
	ShaderInput(const string &name, int index, int slot) : name(name), index(index), slot(slot) {}
	string name;
	int index;
	int slot;
};

typedef vector<ShaderInput> ShaderInputs;

bool create_layout(void *buf, int len, const ShaderInputs &inputs) {

	auto mapping = map_list_of
		("SV_POSITION", DXGI_FORMAT_R32G32B32_FLOAT)
		("POSITION", DXGI_FORMAT_R32G32B32_FLOAT)
		("NORMAL", DXGI_FORMAT_R32G32B32_FLOAT)
		("TEXCOORD", DXGI_FORMAT_R32G32_FLOAT);

	// the format is determined from the semantic name
	vector<D3D11_INPUT_ELEMENT_DESC> input_desc;
	for (size_t i = 0; i < inputs.size(); ++i) {
		const ShaderInput &cur = inputs[i];

		DXGI_FORMAT fmt;
		if (!lookup<string, DXGI_FORMAT>(cur.name, mapping, &fmt))
				return false;

		input_desc.push_back(CD3D11_INPUT_ELEMENT_DESC(cur.name.c_str(), cur.index, fmt, cur.slot));

		ID3D11InputLayout* layout = NULL;
		if (FAILED(GRAPHICS.device()->CreateInputLayout(elems, num_elems, _vs._blob->GetBufferPointer(), _vs._blob->GetBufferSize(), &layout)))
			return nullptr;
		return layout;


	}

	return true;
}
*/

Technique::Technique(Io *io)
	: _io(io)
	, _vertex_shader(nullptr)
	, _pixel_shader(nullptr)
	, _rasterizer_desc(CD3D11_DEFAULT())
	, _sampler_desc(CD3D11_DEFAULT())
	, _blend_desc(CD3D11_DEFAULT())
	, _depth_stencil_desc(CD3D11_DEFAULT())
{
}

Technique::~Technique() {
	SAFE_DELETE(_vertex_shader);
	SAFE_DELETE(_pixel_shader);
}

Technique *Technique::create_from_file(const char *filename, Io *io) {
	void *buf;
	size_t len;
	if (!io->load_file(filename, &buf, &len))
		return NULL;

	TechniqueParser parser;
	Technique *t = new Technique(io);
	bool ok = parser.parse_main((const char *)buf, (const char *)buf + len, t);
	ok &= t->init();
	if (!ok)
		SAFE_DELETE(t);

	return t;
}

struct CBuffer {
	string name;
	int size;
};

bool Technique::do_reflection(Shader *shader, bool vertex_shader, void *buf, size_t len)
{
	ID3D11ShaderReflection* reflector = NULL; 
	B_ERR_HR(D3DReflect(buf, len, IID_ID3D11ShaderReflection, (void**)&reflector));
	D3D11_SHADER_DESC shader_desc;
	reflector->GetDesc(&shader_desc);

	// input mappings are only relevant for vertex shaders
	if (vertex_shader) {
		auto mapping = map_list_of
			("SV_POSITION", DXGI_FORMAT_R32G32B32_FLOAT)
			("POSITION", DXGI_FORMAT_R32G32B32_FLOAT)
			("NORMAL", DXGI_FORMAT_R32G32B32_FLOAT)
			("TEXCOORD", DXGI_FORMAT_R32G32_FLOAT);

		//ShaderInputs inputs;
		vector<D3D11_INPUT_ELEMENT_DESC> inputs;
		for (UINT i = 0; i < shader_desc.InputParameters; ++i) {
			D3D11_SIGNATURE_PARAMETER_DESC input;
			reflector->GetInputParameterDesc(i, &input);

			DXGI_FORMAT fmt;
			if (!lookup<string, DXGI_FORMAT>(input.SemanticName, mapping, &fmt)) {
				LOG_ERROR("Invalid input semantic found: %s", input.SemanticName);
				return false;
			}

			inputs.push_back(CD3D11_INPUT_ELEMENT_DESC(input.SemanticName, input.SemanticIndex, fmt, input.Stream));
		}

		_input_layout = GRAPHICS.create_input_layout(&inputs[0], inputs.size(), buf, len);
		if (!_input_layout.is_valid())
			return false;
	}

	for (UINT i = 0; i < shader_desc.ConstantBuffers; ++i) {
		ID3D11ShaderReflectionConstantBuffer* cb = reflector->GetConstantBufferByIndex(i);
		D3D11_SHADER_BUFFER_DESC cb_desc;
		cb->GetDesc(&cb_desc);
		shader->constant_buffers.push_back(GRAPHICS.create_dynamic_constant_buffer(cb_desc.Size));

		for (UINT j = 0; j < cb_desc.Variables; ++j) {
			ID3D11ShaderReflectionVariable* v = cb->GetVariableByIndex(j);
			D3D11_SHADER_VARIABLE_DESC var_desc;
			v->GetDesc(&var_desc);
			ShaderParam *param = shader->find_by_name(var_desc.Name);
			if (!param) {
				LOG_ERROR("Shader parameter %s not found", var_desc.Name);
				return false;
			}
			param->cbuffer = i;
			param->start_offset = var_desc.StartOffset;
/*
			ID3D11ShaderReflectionType* t = v->GetType();
			D3D11_SHADER_TYPE_DESC type_desc;
			t->GetDesc(&type_desc);
*/
		}
	}

	// get bound resources (textures and buffers)
	for (UINT i = 0; i < shader_desc.BoundResources; ++i) {
		D3D11_SHADER_INPUT_BIND_DESC input_desc;
		reflector->GetResourceBindingDesc(i, &input_desc);
		switch (input_desc.Type) {
		case D3D10_SIT_TEXTURE:
			//_bound_textures.insert(make_pair(input_desc.Name, input_desc));
			break;
		case D3D10_SIT_SAMPLER:
			//_bound_samplers.insert(make_pair(input_desc.Name, input_desc));
			break;

		}
	}
	return true;
}


bool Technique::init_shader(Shader *shader, const string &profile, bool vertex_shader) {

	const string source = replace_extension(shader->filename.c_str(), "fx");

	// compile the shader if the source is newer, or the object file doesn't exist

	bool force = true;
	if (force || _io->file_exists(source.c_str()) && (!_io->file_exists(shader->filename.c_str()) || _io->mdate(source.c_str()) > _io->mdate(shader->filename.c_str()))) {

		STARTUPINFOA startup_info;
		ZeroMemory(&startup_info, sizeof(startup_info));
		startup_info.cb = sizeof(STARTUPINFO);
		startup_info.dwFlags = STARTF_USESTDHANDLES;
		startup_info.hStdOutput = startup_info.hStdError = LOG_MGR.handle();
		PROCESS_INFORMATION process_info;
		ZeroMemory(&process_info, sizeof(process_info));

		string fname;
		split_path(shader->filename.c_str(), NULL, NULL, &fname, NULL);

		//fxc -Tvs_4_0 -EvsMain -Vi -O3 -Fo debug_font.vso debug_font.fx

		char cmd_line[MAX_PATH];
		sprintf(cmd_line, "fxc.exe -nologo -T%s -E%s -Vi -Fo %s %s", 
			profile.c_str(),
			shader->entry_point.c_str(),
			shader->filename.c_str(), source.c_str());

		if (CreateProcessA(NULL, cmd_line, NULL, NULL, TRUE, NORMAL_PRIORITY_CLASS | CREATE_NO_WINDOW, NULL, NULL, &startup_info, &process_info)) {
			WaitForSingleObject(process_info.hProcess, INFINITE);
			DWORD exit_code;
			GetExitCodeProcess(process_info.hProcess, &exit_code);
			FlushFileBuffers(LOG_MGR.handle());

			if (exit_code == 0) {
				void *buf;
				size_t len;
				_io->load_file(shader->filename.c_str(), &buf, &len);

				if (vertex_shader) {
					ID3D11VertexShader *vs;
					GRAPHICS.device()->CreateVertexShader(buf, len, NULL, &vs);
				} else {
					ID3D11PixelShader *ps;
					GRAPHICS.device()->CreatePixelShader(buf, len, NULL, &ps);
				}
				do_reflection(shader, vertex_shader, buf, len);
			}

		} else {
			LOG_ERROR("Unable to launch fxc.exe");
			return false;
		}
	}

	return true;
}

bool Technique::init() {

	if (_vertex_shader) {
		B_ERR_BOOL(init_shader(_vertex_shader, "vs_4_0", true));
	}

	if (_pixel_shader) {
		B_ERR_BOOL(init_shader(_pixel_shader, "ps_4_0", false));
	}

	return true;
}

