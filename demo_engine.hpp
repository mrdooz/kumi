#pragma once

#include <queue>
#include "graphics.hpp"

class Effect {
public:
	Effect(GraphicsObjectHandle context, const std::string &name) : _context(context), _name(name) {}
	virtual ~Effect() {}
	virtual bool init() { return true; }
	virtual bool update(int64 global_time, int64 local_time, int64 frequency, int32 num_ticks, float ticks_fraction) { return true; }
	virtual bool render() { return true; }
	virtual bool close() { return true; }
protected:
	std::string _name;
	GraphicsObjectHandle _context;
};

class DemoEngine {
public:
	static DemoEngine &instance();

	void add_effect(Effect *effect, uint32 start_time, uint32 end_time);
	bool start();
	void pause(bool pause);
	bool init();
	bool tick();
	bool close();

private:

	struct EffectInstance {
		struct compare {
			bool operator()(const EffectInstance &a, const EffectInstance &b) {
				return a._start_time < b._start_time;
			}
		};
		EffectInstance(Effect *effect, uint32 start_time, uint32 end_time) : _effect(effect), _running(false), _start_time(start_time), _end_time(end_time) {}
		Effect *_effect;
		bool _running;
		uint32 _start_time;
		uint32 _end_time;
	};

	DemoEngine();
	static DemoEngine *_instance;

	std::deque<EffectInstance *> _active_effects;
	std::deque<EffectInstance *> _inactive_effects;
	std::vector<EffectInstance *> _effects;
	int _cur_effect;
	int64 _frequency;
	int64 _last_time;
	int64 _current_time_ms;
	
	bool _paused;
	bool _inited;
};

#define DEMO_ENGINE DemoEngine::instance()
