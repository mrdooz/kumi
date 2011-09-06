#pragma once

using std::hash_map;

class CriticalSection {
public:
	CriticalSection();
	~CriticalSection();
	void enter();
	void leave();
private:
	CRITICAL_SECTION _cs;
};

class ScopedCs {
public:
	ScopedCs(CriticalSection &cs);
	~ScopedCs();
private:
	CriticalSection &_cs;
};

class ScopedHandle {
public:
	ScopedHandle(HANDLE h) : _h(h) {}
	~ScopedHandle() {
		if (_h != INVALID_HANDLE_VALUE)
			CloseHandle(_h);
	}
	operator HANDLE() { return _h; }
	HANDLE handle() const { return _h; }
private:
	HANDLE _h;
};


struct ScopedObj
{
	typedef std::function<void()> Fn;
	ScopedObj(const Fn& fn) : fn(fn) {}
	~ScopedObj() { fn(); }
	Fn fn;
};

#define SCOPED_CS(cs) ScopedCs lock(cs);

// Macro for creating "local" names
#define GEN_NAME2(prefix, line) prefix##line
#define GEN_NAME(prefix, line) GEN_NAME2(prefix, line)
#define MAKE_SCOPED(type) type GEN_NAME(ANON, __LINE__)

#define SCOPED_OBJ(x) MAKE_SCOPED(ScopedObj)((x));

// A macro to disallow the copy constructor and operator= functions
// This should be used in the private: declarations for a class
#define DISALLOW_COPY_AND_ASSIGN(TypeName) \
	TypeName(const TypeName&);               \
	void operator=(const TypeName&)


template< class Container >
void safe_erase(Container& c, const typename Container::value_type& v)
{
	auto it = std::find(c.begin(), c.end(), v);
	if (it != c.end())
		c.erase(it);
}

template<class T> 
void seq_delete(T* t) {
	for (T::iterator it = t->begin(); it != t->end(); ++it)
		delete *it;
	t->clear();
}

template<class T> 
void assoc_delete(T* t) {
	for (T::iterator it = t->begin(); it != t->end(); ++it)
		delete it->second;
	t->clear();
}

template<class T>
void release_obj(T t) {
	if (t)
		t->Release();
}

template<class T>
void delete_obj(T t) {
	delete t;
}

// I'm not sure if this is crazy overkill or not.. I guess what I want is lazy evaluation of function arguments
#define either_or(a, b) [&]() ->decltype((a)) { auto t = (a); return t ? t : (b); }()

template <class T>
T exch_null(T &t)
{
	T tmp = t;
	t = nullptr;
	return tmp;
}

template<typename T>
T max3(const T &a, const T &b, const T &c) {
	return max(a, max(b, c));
}

template<typename T>
T max4(const T &a, const T &b, const T &c, const T &d) {
	return max(a, max3(b, c, d));
}


#define SAFE_RELEASE(x) if( (x) != 0 ) { (x)->Release(); (x) = 0; }
#define SAFE_FREE(x) if( (x) != 0 ) { free((void*)(x)); (x) = 0; }
#define SAFE_DELETE(x) if( (x) != 0 ) { delete (x); (x) = 0; }
#define SAFE_ADELETE(x) if( (x) != 0 ) { delete [] (x); (x) = 0; }

#define ELEMS_IN_ARRAY(x) sizeof(x) / sizeof((x)[0])

template<typename Key, typename Value>
bool lookup(Key str, const hash_map<Key, Value> &candidates, Value *res) {
	auto it = candidates.find(str);
	if (it != candidates.end()) {
		*res = it->second;
		return true;
	}
	return false;
}

template<typename Container, typename Key>
bool contains(const Container &c, const Key key) {
	Container::const_iterator it = c.find(key);
	return it != c.end();
}

template<typename Key, typename Value>
Value lookup_default(Key str, const hash_map<Key, Value> &candidates, Value default_value) {
	auto it = candidates.find(str);
	return it != candidates.end() ? it->second : default_value;
}
