#pragma once

using std::stack;
using std::vector;
using std::pair;
namespace mpl = boost::mpl;

class GraphicsObjectHandle;

template<class Traits, int N>
struct IdBufferBase {

	typedef typename Traits::Type T;
	typedef std::function<void(typename Traits::Type)> Deleter;

	enum { Size = N };

	IdBufferBase(const Deleter &deleter = Deleter()) : _deleter(deleter) {
		ZeroMemory(&_buffer, sizeof(_buffer));
	}

	~IdBufferBase() {
		if (_deleter) {
			for (int i = 0; i < N; ++i) {
				if (Traits::get(_buffer[i]))
					_deleter(Traits::get(_buffer[i]));
			}
		}

		ZeroMemory(&_buffer, sizeof(_buffer));
		while (!reclaimed_indices.empty())
			reclaimed_indices.pop();
	}

	int find_free_index() {
		if (!reclaimed_indices.empty()) {
			int tmp = reclaimed_indices.top();
			reclaimed_indices.pop();
			return tmp;
		}
		for (int i = 0; i < N; ++i) {
			if (!Traits::get(_buffer[i]))
				return i;
		}
		return -1;
	}

	typename Traits::Type &operator[](int idx) {
		assert(idx >= 0 && idx < N);
		return _buffer[idx];
	}

	void reclaim_index(int idx) {
		_reclaimed_indices.push(idx);

	}

	typename Traits::Type get(GraphicsObjectHandle handle) {
		return Traits::get(_buffer[handle.id()]);
	}

	Deleter _deleter;
	typename Traits::Elem _buffer[N];
	stack<int> reclaimed_indices;
};

template<class Traits, int N>
struct SearchableIdBuffer : public IdBufferBase<Traits, N> {

	typedef IdBufferBase<Traits, N> Parent;

	SearchableIdBuffer(const typename IdBufferBase<Traits, N>::Deleter &fn_deleter) 
		: Parent(fn_deleter)
	{
	}

	typename Traits::Elem &operator[](int idx) {
		assert(idx >= 0 && idx < N);
		return _buffer[idx];
	}

	int find_by_token(const typename Traits::Token &t) {
		for (int i = 0; i < Size; ++i) {
			if (Traits::get_token(_buffer[i]) == t)
				return i;
		}
		return -1;
	}
};

template<class T, class U>
struct PairTraits {
	typedef T Type;
	typedef U Token;
	typedef std::pair<T, U> Elem;
	static T& get(Elem &t) {
		return t.first;
	}

	static U get_token(const Elem &e) {
		return e.second;
	}
};

template<class T>
struct SingleTraits {
	typedef T Type;
	typedef T Elem;
	static T& get(Elem& t) {
		return t;
	}
};

template<typename T, int N, typename SearchToken = void>
struct IdBuffer : public
	mpl::if_<typename std::tr1::is_void<SearchToken>::type,
	IdBufferBase<SingleTraits<T>, N>,
	SearchableIdBuffer<PairTraits<T, SearchToken>, N> >::type
{
	typedef typename mpl::if_<typename std::tr1::is_void<SearchToken>::type,
		IdBufferBase<SingleTraits<T>, N>,
		SearchableIdBuffer<PairTraits<T, SearchToken>, N> >::type Parent;

	typedef typename IdBufferBase<SingleTraits<T>, N>::Deleter Deleter;

	IdBuffer(const Deleter &fn_deleter = Deleter())
		: Parent(fn_deleter)
	{
	}
};
