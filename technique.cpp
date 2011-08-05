#include "stdafx.h"
#include "technique.hpp"
#include "technique_parser.hpp"
#include "io.hpp"
#include "file_utils.hpp"
#include "graphics.hpp"

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

bool do_reflection(void *buf, size_t len)
{
	ID3D11ShaderReflection* reflector = NULL; 
	B_ERR_HR(D3DReflect(buf, len, IID_ID3D11ShaderReflection, (void**)&reflector));
	D3D11_SHADER_DESC shader_desc;
	reflector->GetDesc(&shader_desc);

	D3D11_SHADER_BUFFER_DESC d;
	D3D11_SHADER_VARIABLE_DESC vd;
	D3D11_SHADER_TYPE_DESC td;

	// global constant buffer is called "$Globals"
	for (UINT i = 0; i < shader_desc.ConstantBuffers; ++i) {
		ID3D11ShaderReflectionConstantBuffer* rcb = reflector->GetConstantBufferByIndex(i);
		rcb->GetDesc(&d);
		CD3D11_BUFFER_DESC bb(d.Size, D3D11_BIND_CONSTANT_BUFFER, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE);

		// check if the constant buffer has already been created
/*
		ConstantBuffer* cur_cb = NULL;
		auto it = _constant_buffers.find(d.Name);
		if (it != _constant_buffers.end()) {
			cur_cb = it->second;
		} else {
			ID3D11Buffer *cb = NULL;
			B_ERR_DX(Graphics::instance().device()->CreateBuffer(&bb, NULL, &cb));
			_constant_buffers.insert(make_pair(cur_cb->_name, new ConstantBuffer(d.Name, cb, bb)));
		}
*/
		for (UINT j = 0; j < d.Variables; ++j) {
			ID3D11ShaderReflectionVariable* v = rcb->GetVariableByIndex(j);
			v->GetDesc(&vd);

			ID3D11ShaderReflectionType* t = v->GetType();
			t->GetDesc(&td);
/*
			if (_buffer_variables.find(vd.Name) == _buffer_variables.end()) {
				ID3D11ShaderReflectionType* t = v->GetType();
				t->GetDesc(&td);
				_buffer_variables.insert(make_pair(vd.Name, new BufferVariable(vd.Name, cur_cb, vd, td)));
			}
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
		sprintf(cmd_line, "fxc.exe -T%s -E%s -Vi -Fo %s %s", 
			profile.c_str(),
			shader->entry_point.c_str(),
			shader->filename.c_str(), source.c_str());

		if (CreateProcessA(NULL, cmd_line, NULL, NULL, TRUE, NORMAL_PRIORITY_CLASS, NULL, NULL, &startup_info, &process_info)) {
			WaitForSingleObject(process_info.hProcess, INFINITE);
			DWORD exit_code;
			GetExitCodeProcess(process_info.hProcess, &exit_code);
			FlushFileBuffers(LOG_MGR.handle());

			if (exit_code == 0) {
				void *buf;
				size_t len;
				_io->load_file(shader->filename.c_str(), &buf, &len);

				if (vertex_shader) {
					ID3D11VertexShader *shader;
					GRAPHICS.device()->CreateVertexShader(buf, len, NULL, &shader);
				} else {
					ID3D11PixelShader *shader;
					GRAPHICS.device()->CreatePixelShader(buf, len, NULL, &shader);
				}
				do_reflection(buf, len);
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
