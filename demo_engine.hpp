#pragma once

#include <queue>

class Effect {
public:
	Effect(const std::string &name) : _name(name) {}
	virtual ~Effect() {}
	virtual bool init() = 0;
	virtual bool update(uint32 global_time, uint32 local_time, uint32 delta, uint32 ticks, uint32 ticks_fraction) = 0;
	virtual bool render() = 0;
	virtual bool close() = 0;
protected:
	std::string _name;
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
				return a.start_time < b.start_time;
			}
		};
		EffectInstance(Effect *effect, uint32 start_time, uint32 end_time) : effect(effect), start_time(start_time), end_time(end_time) {}
		Effect *effect;
		uint32 start_time;
		uint32 end_time;
	};

	DemoEngine();
	static DemoEngine *_instance;

	std::priority_queue<EffectInstance, std::deque<EffectInstance>, EffectInstance::compare> _effects;
	LARGE_INTEGER _frequency;
	LARGE_INTEGER _start_time;
	LARGE_INTEGER _last_time;
	LARGE_INTEGER _elapsed_time;
	
	bool _paused;
};

#define DEMO_ENGINE DemoEngine::instance()
