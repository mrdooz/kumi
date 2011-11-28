#include "stdafx.h"
#include "technique.hpp"
#include "dx_utils.hpp"
#include "graphics_interface.hpp"
#include "resource_interface.hpp"

using namespace boost::assign;
using namespace std;

Technique::Technique()
	: _vertex_shader(nullptr)
	, _pixel_shader(nullptr)
	, _rasterizer_desc(CD3D11_DEFAULT())
	, _blend_desc(CD3D11_DEFAULT())
	, _depth_stencil_desc(CD3D11_DEFAULT())
	, _vertex_size(-1)
	, _index_format(DXGI_FORMAT_UNKNOWN)
{
}

Technique::~Technique() {
	SAFE_DELETE(_vertex_shader);
	SAFE_DELETE(_pixel_shader);
}

bool Technique::do_reflection(GraphicsInterface *graphics, Shader *shader, void *buf, size_t len, set<string> *used_params)
{
	ID3D11ShaderReflection* reflector = NULL; 
	B_ERR_HR(D3DReflect(buf, len, IID_ID3D11ShaderReflection, (void**)&reflector));
	D3D11_SHADER_DESC shader_desc;
	reflector->GetDesc(&shader_desc);

	// input mappings are only relevant for vertex shaders
	if (shader->type() == Shader::kVertexShader) {
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

		_input_layout = graphics->create_input_layout(&inputs[0], inputs.size(), buf, len);
		if (!_input_layout.is_valid())
			return false;
	}

	// constant buffers are stored per technique as they are shared between the
	// vertex and pixel shaders (at least the global cbuffer is)

	for (UINT i = 0; i < shader_desc.ConstantBuffers; ++i) {
		ID3D11ShaderReflectionConstantBuffer* cb = reflector->GetConstantBufferByIndex(i);
		D3D11_SHADER_BUFFER_DESC cb_desc;
		cb->GetDesc(&cb_desc);

		// see if a cbuffer with the current name already exists..
		int cb_idx = -1;
		bool found = false;
		for (size_t j = 0; j < _constant_buffers.size(); ++j) {
			if (_constant_buffers[j].name == cb_desc.Name) {
				cb_idx = j;
				found = true;
				break;
			}
		}
		if (!found) {
			cb_idx = _constant_buffers.size();
			_constant_buffers.push_back(CBuffer(cb_desc.Name, cb_desc.Size, graphics->create_constant_buffer(cb_desc.Size)));
		}

		// extract all the used variable's cbuffer, starting offset and size
		for (UINT j = 0; j < cb_desc.Variables; ++j) {
			ID3D11ShaderReflectionVariable* v = cb->GetVariableByIndex(j);
			D3D11_SHADER_VARIABLE_DESC var_desc;
			v->GetDesc(&var_desc);
			if (var_desc.uFlags & D3D_SVF_USED) {
				used_params->insert(var_desc.Name);
				CBufferParam *param = shader->find_cbuffer_param(var_desc.Name);
				if (!param) {
					LOG_ERROR("Shader parameter %s not found", var_desc.Name);
					return false;
				}
				param->cbuffer = cb_idx;
				param->start_offset = var_desc.StartOffset;
				param->size = var_desc.Size;
			}
		}
	}

	// get bound resources (textures and buffers)
	for (UINT i = 0; i < shader_desc.BoundResources; ++i) {
		D3D11_SHADER_INPUT_BIND_DESC input_desc;
		reflector->GetResourceBindingDesc(i, &input_desc);
		if (input_desc.BindCount > 1) {
			LOG_ERROR("Only bind counts of 1 are supported");
			return false;
		}

		switch (input_desc.Type) {

			case D3D10_SIT_TEXTURE: {
				ResourceViewParam *param = shader->find_resource_view_param(input_desc.Name);
				if (!param) {
					LOG_ERROR("Resource view %s not found", input_desc.Name);
					return false;
				}
				param->bind_point = input_desc.BindPoint;
				break;
			}

			case D3D10_SIT_SAMPLER: {
				SamplerParam *param = shader->find_sampler_param(input_desc.Name);
				if (!param) {
					LOG_ERROR("Sampler %s not found", input_desc.Name);
					return false;
				}
				param->bind_point = input_desc.BindPoint;
				break;
			}
		}
	}
	return true;
}


bool Technique::compile_shader(GraphicsInterface *res, Shader *shader) {

	STARTUPINFOA startup_info;
	ZeroMemory(&startup_info, sizeof(startup_info));
	startup_info.cb = sizeof(STARTUPINFO);
	// redirect the child's output handles to the log manager's
	// TODO
/*
	startup_info.dwFlags = STARTF_USESTDHANDLES;
	startup_info.hStdOutput = startup_info.hStdError = LOG_MGR.handle();
*/
	PROCESS_INFORMATION process_info;
	ZeroMemory(&process_info, sizeof(process_info));

	const char *profile = shader->type() == Shader::kVertexShader ? res->vs_profile() : res->ps_profile();

	//fxc -nologo -T$(PROFILE) -E$(ENTRY_POINT) -Vi -O3 -Fo $(OBJECT_FILE) $(SOURCE_FILE)
	char cmd_line[MAX_PATH];
	sprintf(cmd_line, "fxc.exe -nologo -T%s -E%s -Vi -O3 -Fo %s %s", 
		profile,
		shader->entry_point.c_str(),
		shader->obj_filename.c_str(), shader->source_filename.c_str());

	DWORD exit_code = 1;

	if (CreateProcessA(NULL, cmd_line, NULL, NULL, TRUE, NORMAL_PRIORITY_CLASS | CREATE_NO_WINDOW, NULL, NULL, &startup_info, &process_info)) {
		WaitForSingleObject(process_info.hProcess, INFINITE);
		GetExitCodeProcess(process_info.hProcess, &exit_code);
		FlushFileBuffers(LOG_MGR.handle());
	} else {
		LOG_ERROR("Unable to launch fxc.exe");
	}

	return exit_code == 0;
}

bool Technique::reload_shaders() {
	return true;
}

bool Technique::init_shader(GraphicsInterface *graphics, ResourceInterface *res, Shader *shader) {

	bool force = true;

	bool ok = true;
	const char *obj = shader->obj_filename.c_str();
	const char *src = shader->source_filename.c_str();
	// compile the shader if the source is newer, or the object file doesn't exist
	if (force || res->file_exists(src) && (!res->file_exists(obj) || res->mdate(src) > res->mdate(obj))) {
			if (!compile_shader(graphics, shader))
				return false;
	} else {
		if (!res->file_exists(obj))
			return false;
	}

	void *buf;
	size_t len;

	set<string> used_params;
	if (!res->load_file(obj, &buf, &len))
		return false;

	if (!do_reflection(graphics, shader, buf, len, &used_params))
		return false;

	// remove any unused parameters
	vector<CBufferParam> &params = shader->cbuffer_params;
	params.erase(
		remove_if(params.begin(), params.end(), [&](const CBufferParam &p) { return used_params.find(p.name) == used_params.end(); }),
		params.end());

	switch (shader->type()) {
		case Shader::kVertexShader: shader->shader = graphics->create_vertex_shader(buf, len, shader->obj_filename); break;
		case Shader::kPixelShader: shader->shader = graphics->create_pixel_shader(buf, len, shader->obj_filename); break;
		case Shader::kGeometryShader: break;
	}
	shader->valid = ok;

	return true;
}

bool Technique::init(GraphicsInterface *graphics, ResourceInterface *res) {

	if (_vertex_shader)
		B_ERR_BOOL(init_shader(graphics, res, _vertex_shader));

	if (_pixel_shader)
		B_ERR_BOOL(init_shader(graphics, res, _pixel_shader));

	_rasterizer_state = graphics->create_rasterizer_state(_rasterizer_desc);
	_blend_state = graphics->create_blend_state(_blend_desc);
	_depth_stencil_state = graphics->create_depth_stencil_state(_depth_stencil_desc);
	for (size_t i = 0; i < _sampler_descs.size(); ++i)
		_sampler_states.push_back(make_pair(_sampler_descs[i].first, graphics->create_sampler_state(_sampler_descs[i].second)));

	return true;
}
/*
void Technique::submit() {
	_key.cmd = RenderKey::kRenderTechnique;
	_key.seq_nr = GRAPHICS.next_seq_nr();
	GRAPHICS.submit_command(FROM_HERE, _key, (void *)this);
}
*/
GraphicsObjectHandle Technique::sampler_state(const char *name) const {
	auto it = find_if(_sampler_states.begin(), _sampler_states.end(), [&](const pair<string, GraphicsObjectHandle> &p) { return p.first == name; });
	return it == _sampler_states.end() ? GraphicsObjectHandle() : it->second;
}
