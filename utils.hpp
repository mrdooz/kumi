#pragma once

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

#define SCOPED_CS(cs) ScopedCs lock(cs);

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
void map_delete(T& container)
{
	for (T::iterator it = container.begin(), e = container.end(); it != e; ++it) {
		delete it->second;
	}
	container.clear();
}

template<class T>
void container_delete(T& container)
{
	for (T::iterator it = container.begin(), e = container.end(); it != e; ++it) {
		delete *it;
	}
	container.clear();
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

#define SAFE_RELEASE(x) if( (x) != 0 ) { (x)->Release(); (x) = 0; }
#define SAFE_FREE(x) if( (x) != 0 ) { free((void*)(x)); (x) = 0; }
#define SAFE_DELETE(x) if( (x) != 0 ) { delete (x); (x) = 0; }
#define SAFE_ADELETE(x) if( (x) != 0 ) { delete [] (x); (x) = 0; }

#define ELEMS_IN_ARRAY(x) sizeof(x) / sizeof((x)[0])
