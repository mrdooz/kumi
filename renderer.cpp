#include "stdafx.h"
#include "renderer.hpp"
#include "graphics.hpp"
#include "technique.hpp"
#include "material_manager.hpp"

using namespace std;

static const int cEffectDataSize = 1024 * 1024;


Renderer *Renderer::_instance;

Renderer::Renderer()
	: _effect_data(new uint8[cEffectDataSize])
	, _effect_data_ofs(0)
{
}

Renderer &Renderer::instance() {
	if (!_instance)
		_instance = new Renderer();
	return *_instance;
}

void Renderer::render() {

	ID3D11DeviceContext *ctx = GRAPHICS.context();

	Graphics::BackedResources *res = GRAPHICS.get_backed_resources();

	ctx->RSSetViewports(1, &GRAPHICS.viewport());

	// sort by keys
	sort(_render_commands.begin(), _render_commands.end(), [&](const RenderCmd &a, const RenderCmd &b) { return a.key.data < b.key.data; });

	// delete commands are sorted before render commands, so we can just save the
	// deleted items when we find them
	set<void *> deleted_items;

	for (size_t i = 0; i < _render_commands.size(); ++i) {
		const RenderCmd &cmd = _render_commands[i];
		RenderKey key = cmd.key;
		void *data = cmd.data;

		switch (key.cmd) {

			case RenderKey::kDelete:
				deleted_items.insert(data);
				break;

			case RenderKey::kSetRenderTarget: {
				GraphicsObjectHandle h = key.handle;
				if (RenderTargetData *rt = res->_render_targets.get(h)) {
					ctx->OMSetRenderTargets(1, &(rt->rtv.p), rt->dsv);
					if (data) {
						const XMFLOAT4 &f = *(XMFLOAT4 *)cmd.data;
						GRAPHICS.clear(h, f);
					}
				}
				break;
			}

			case RenderKey::kRenderTechnique: {
				if (deleted_items.find(data) != deleted_items.end())
					break;

				TechniqueRenderData *d = (TechniqueRenderData *)data;
				Technique *technique = res->_techniques.get(d->technique);

				Shader *vertex_shader = technique->vertex_shader();
				Shader *pixel_shader = technique->pixel_shader();

				ctx->VSSetShader(res->_vertex_shaders.get(vertex_shader->shader), NULL, 0);
				ctx->GSSetShader(NULL, 0, 0);
				ctx->PSSetShader(res->_pixel_shaders.get(pixel_shader->shader), NULL, 0);

				GRAPHICS.set_vb(ctx, res->_vertex_buffers.get(technique->vb()), technique->vertex_size());
				ctx->IASetIndexBuffer(res->_index_buffers.get(technique->ib()), technique->index_format(), 0);
				ctx->IASetInputLayout(res->_input_layouts.get(technique->input_layout()));

				ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

				ctx->RSSetState(res->_rasterizer_states.get(technique->rasterizer_state()));
				ctx->OMSetDepthStencilState(res->_depth_stencil_states.get(technique->depth_stencil_state()), ~0);
				ctx->OMSetBlendState(res->_blend_states.get(technique->blend_state()), GRAPHICS.default_blend_factors(), GRAPHICS.default_sample_mask());

				GRAPHICS.set_samplers(technique, pixel_shader);
				int resource_mask = 0;
				GRAPHICS.set_resource_views(technique, pixel_shader, &resource_mask);

				ctx->DrawIndexed(technique->index_count(), 0, 0);
				GRAPHICS.unbind_resource_views(resource_mask);

				break;
			}

			case RenderKey::kRenderMesh: {
				if (deleted_items.find(data) != deleted_items.end())
					break;
				MeshRenderData *render_data = (MeshRenderData *)data;
				const uint16 material_id = render_data->material_id;
				const Material *material = MATERIAL_MANAGER.get_material(material_id);
				Technique *technique = res->_techniques.get(render_data->technique);
				Shader *vertex_shader = technique->vertex_shader();
				Shader *pixel_shader = technique->pixel_shader();

				ctx->VSSetShader(res->_vertex_shaders.get(vertex_shader->shader), NULL, 0);
				ctx->GSSetShader(NULL, 0, 0);
				ctx->PSSetShader(res->_pixel_shaders.get(pixel_shader->shader), NULL, 0);

				GRAPHICS.set_vb(ctx, res->_vertex_buffers.get(render_data->vb), render_data->vertex_size);
				ctx->IASetIndexBuffer(res->_index_buffers.get(render_data->ib), render_data->index_format, 0);
				ctx->IASetInputLayout(res->_input_layouts.get(technique->input_layout()));

				ctx->IASetPrimitiveTopology(render_data->topology);

				ctx->RSSetState(res->_rasterizer_states.get(technique->rasterizer_state()));
				ctx->OMSetDepthStencilState(res->_depth_stencil_states.get(technique->depth_stencil_state()), ~0);
				ctx->OMSetBlendState(res->_blend_states.get(technique->blend_state()), GRAPHICS.default_blend_factors(), GRAPHICS.default_sample_mask());

				GRAPHICS.set_cbuffer_params(technique, vertex_shader, material_id, render_data->mesh_id);
				GRAPHICS.set_cbuffer_params(technique, pixel_shader, material_id, render_data->mesh_id);
				GRAPHICS.set_samplers(technique, pixel_shader);
				int resource_mask;
				GRAPHICS.set_resource_views(technique, pixel_shader, &resource_mask);

				ctx->DrawIndexed(render_data->index_count, 0, 0);
				GRAPHICS.unbind_resource_views(resource_mask);
				break;
			}
		}
	}

	_render_commands.clear();
	_effect_data_ofs = 0;
}

void *Renderer::alloc_command_data(size_t size) {
	if (size + _effect_data_ofs >= cEffectDataSize)
		return nullptr;

	void *ptr = &_effect_data.get()[_effect_data_ofs];
	_effect_data_ofs += size;
	return ptr;
}

void Renderer::validate_command(RenderKey key, const void *data) {

	Graphics::BackedResources *res = GRAPHICS.get_backed_resources();

	switch (key.cmd) {

	case RenderKey::kSetRenderTarget:
		break;

	case RenderKey::kRenderMesh: {
		MeshRenderData *render_data = (MeshRenderData *)data;
		const uint16 material_id = render_data->material_id;
		const Material *material = MATERIAL_MANAGER.get_material(material_id);
		Technique *technique = res->_techniques.get(render_data->technique);
		assert(technique);
		Shader *vertex_shader = technique->vertex_shader();
		assert(vertex_shader);
		Shader *pixel_shader = technique->pixel_shader();
		assert(pixel_shader);
		break;
	}

	case RenderKey::kRenderTechnique:
		break;
	}
}

void Renderer::submit_command(const TrackedLocation &location, RenderKey key, void *data) {
#if _DEBUG
	validate_command(key, data);
#endif
	_render_commands.push_back(RenderCmd(location, key, data));
}
