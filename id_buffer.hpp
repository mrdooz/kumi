#pragma once

using std::stack;
using std::vector;
using std::pair;
namespace mpl = boost::mpl;

class GraphicsObjectHandle;

template<class Traits, int N>
struct IdBufferBase {

	typedef typename Traits::Type T;

	enum { Size = N };
	IdBufferBase() {
		ZeroMemory(&_buffer, sizeof(_buffer));
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

	typename Traits::Elem _buffer[N];
	stack<int> reclaimed_indices;
};

template<class Traits, int N>
struct SearchableIdBuffer : public IdBufferBase<Traits, N> {

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
	template<class Traits, int N>
	struct IdBufferBase {

		typedef typename Traits::Type T;

		enum { Size = N };
		IdBufferBase() {
			ZeroMemory(&_buffer, sizeof(_buffer));
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

		typename Traits::Elem _buffer[N];
		stack<int> reclaimed_indices;
	};

	template<class Traits, int N>
	struct SearchableIdBuffer : public IdBufferBase<Traits, N> {

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
};
