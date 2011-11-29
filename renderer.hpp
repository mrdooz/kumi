#pragma once

#include "graphics_submit.hpp"
#include "threading.hpp"

class Renderer {
public:
	static Renderer &instance();

	Renderer();
	void *alloc_command_data(size_t size);
	void submit_command(const TrackedLocation &location, RenderKey key, void *data);
	uint16 next_seq_nr() const { assert(_render_commands.size() < 65536); return (uint16)_render_commands.size(); }
	void render();

private:
	static Renderer *_instance;

	struct RenderCmd {
		RenderCmd(const TrackedLocation &location, RenderKey key, void *data) : location(location), key(key), data(data) {}
		TrackedLocation location;
		RenderKey key;
		void *data;
	};

	std::vector<RenderCmd > _render_commands;

	std::unique_ptr<uint8> _effect_data;
	int _effect_data_ofs;

};

#define RENDERER Renderer::instance()