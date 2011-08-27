#pragma once
#include <concurrent_queue.h>
#include "utils.hpp"

namespace threading {
	enum ThreadId {
		kMainThread,
		kIoThread,
		kFileMonitorThread,

		kThreadCount,
	};

	struct Deferred {
		typedef std::function<void()> Fn;
		Deferred() {}
		Deferred(const std::string &name, const Fn &fn) : name(name), fn(fn) {}
		void operator()() { fn(); }
		std::string name;
		Fn fn;
	};

	struct TrackedLocation {
		TrackedLocation() {}
		TrackedLocation(const char *function, const char *file, int line) : function(function), file(file), line(line) {}
		std::string function;
		std::string file;
		int line;
	};

#define FROM_HERE TrackedLocation(__FUNCTION__, __FILE__, __LINE__)
#define MAKE_DEFERRED(x) Deferred(#x, x)

	class Thread {
	public:
		friend class Dispatcher;
		virtual ~Thread();

		bool started() const { return _thread != INVALID_HANDLE_VALUE; }
		bool start(void *userdata);
		virtual UINT run(void *userdata) = 0;
	protected:
		Thread();
		static UINT __stdcall run_trampoline(void *thread);

		void process_deferred();

		HANDLE _thread;
	private:
		struct DeferredData {
			DeferredData() : handle(INVALID_HANDLE_VALUE) {}
			DeferredData(const TrackedLocation &location, HANDLE handle, const Deferred &callback) : location(location), handle(handle), callback(callback) {}
			TrackedLocation location;
			HANDLE handle;
			Deferred callback;
		};
		Concurrency::concurrent_queue<DeferredData> _deferred;
		void add_deferred(const TrackedLocation &location, HANDLE h, const Deferred &df);
	};

	class Dispatcher {
	public:

		void set_thread(ThreadId id, Thread *thread);

		static Dispatcher &instance();

		// queue the function on the specified thread's queue
		void invoke(const TrackedLocation &location, ThreadId id, const Deferred &df);

		void invoke_and_wait(const TrackedLocation &location, ThreadId id, const Deferred &df);

	private:

		CriticalSection _thread_cs;
		Thread *_threads[kThreadCount];
		static Dispatcher *_instance;
	};

	struct MessageLoop {
	public:
	private:

	};

}

#define DISPATCHER threading::Dispatcher::instance()
