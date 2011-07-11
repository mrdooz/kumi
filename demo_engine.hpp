#pragma once

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
	uint32 _start_time;
	uint32 _end_time;
};

class DemoEngine {
public:
	static DemoEngine &instance();
	bool init();
	bool tick();
	bool close();

private:
	DemoEngine();
	static DemoEngine *_instance;
};

#define DEMO_ENGINE DemoEngine::instance()
