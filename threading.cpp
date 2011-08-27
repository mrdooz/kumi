#include "stdafx.h"
#include "threading.hpp"
#include <concurrent_queue.h>

using std::pair;
using std::make_pair;
using boost::scoped_ptr;

namespace threading {

	Dispatcher *Dispatcher::_instance = nullptr;

	Dispatcher &Dispatcher::instance() {
		if (!_instance)
			_instance = new Dispatcher;
		return *_instance;
	}

	void Dispatcher::invoke(const TrackedLocation &location, ThreadId id, const Deferred &df) {
		SCOPED_CS(_thread_cs);
		Thread *thread = _threads[id];
		if (!thread)
			return;

		thread->add_deferred(location, INVALID_HANDLE_VALUE, df);
	}

	void Dispatcher::invoke_and_wait(const TrackedLocation &location, ThreadId id, const Deferred &df) {
		SCOPED_CS(_thread_cs);
		Thread *thread = _threads[id];
		if (!thread)
			return;

		HANDLE h = CreateEvent(NULL, FALSE, FALSE, NULL);
		thread->add_deferred(location, h, df);
		WaitForSingleObject(h, INFINITE);
		CloseHandle(h);
	}


	void Dispatcher::set_thread(ThreadId id, Thread *thread) {
		SCOPED_CS(_thread_cs);
		_threads[id] = thread;
	}

	Thread::Thread() 
		: _thread(INVALID_HANDLE_VALUE) 
	{
	}

	Thread::~Thread() {

	}

	typedef pair<Thread *, void *> RunData;

	UINT Thread::run_trampoline(void *thread) {
		scoped_ptr<RunData> data((RunData *)thread);
		return ((Thread *)data->first)->run(data->second);
	}

	bool Thread::start(void *userdata) {
		if (_thread != INVALID_HANDLE_VALUE)
			return false;

		_thread = (HANDLE)_beginthreadex(NULL, 0, &Thread::run_trampoline, (void *)new RunData(this, userdata), 0, NULL);
		return _thread != INVALID_HANDLE_VALUE;
	}

	void Thread::add_deferred(const TrackedLocation &location, HANDLE h, const Deferred &df) {
		_deferred.push(DeferredData(location, h, df));
	}

	void Thread::process_deferred() {
		while (!_deferred.empty()) {
			DeferredData cur;
			if (_deferred.try_pop(cur)) {
				cur.callback();
				if (cur.handle != INVALID_HANDLE_VALUE) {
					SetEvent(cur.handle);
				}
			}
		}
	}
}
