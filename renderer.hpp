#pragma once

#include "graphics_submit.hpp"
#include "threading.hpp"

class Renderer {
public:
	static Renderer &instance();

	Renderer();
  template <typename T>
  T *alloc_command_data() { return (T *)alloc_command_data(sizeof(T)); }
	void *alloc_command_data(size_t size);
	void submit_command(const TrackedLocation &location, RenderKey key, void *data);
  void submit_technique(GraphicsObjectHandle technique);
	uint16 next_seq_nr() const;
	void render();

private:
	static Renderer *_instance;

	void validate_command(RenderKey key, const void *data);

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
