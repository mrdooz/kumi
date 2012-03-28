#pragma once

using std::stack;
using std::vector;
using std::pair;

class GraphicsObjectHandle;

template<class Traits, int N>
class IdBufferBase {
public:
	typedef typename Traits::Type T;
  typedef typename Traits::Elem E;
	typedef std::function<void(T)> Deleter;

	enum { Size = N };

	IdBufferBase(const Deleter &deleter) : _deleter(deleter) {
		ZeroMemory(&_buffer, sizeof(_buffer));
	}

	virtual ~IdBufferBase() {
		if (_deleter) {
			for (int i = 0; i < Size; ++i) {
        T t = Traits::get(_buffer[i]);
				if (t)
					_deleter(t);
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
		for (int i = 0; i < Size; ++i) {
			if (!Traits::get(_buffer[i]))
				return i;
		}
		return -1;
	}

	T &operator[](int idx) {
		assert(idx >= 0 && idx < N);
    return Traits::get(_buffer[idx]);
	}

	void reclaim_index(int idx) {
		_reclaimed_indices.push(idx);
	}

	T get(GraphicsObjectHandle handle) {
		return Traits::get(_buffer[handle.id()]);
	}
protected:
	Deleter _deleter;
  E _buffer[N];
	stack<int> reclaimed_indices;
};

template<class T, class U>
struct SearchableTraits {

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

template<typename T, typename U, int N>
struct SearchableIdBuffer : public IdBufferBase<SearchableTraits<T, U>, N> {

  typedef SearchableTraits<T, U> Traits;
	typedef IdBufferBase<Traits, N> Parent;

	SearchableIdBuffer(const typename Parent::Deleter &fn_deleter) 
		: Parent(fn_deleter)
	{
	}

  void set_pair(int idx, const typename Traits::Elem &e) {
    assert(idx >= 0 && idx < N);
    _buffer[idx] = e;
  }

	int idx_from_token(const typename Traits::Token &t) {
		for (int i = 0; i < Size; ++i) {
			if (Traits::get_token(_buffer[i]) == t)
				return i;
		}
		return -1;
	}
};

template<typename T, int N>
struct IdBuffer : IdBufferBase<SingleTraits<T>, N> {
  typedef SingleTraits<T> Traits;
  typedef IdBufferBase<Traits, N> Parent;

  IdBuffer(const typename Parent::Deleter &fn_deleter) 
    : Parent(fn_deleter)
  {
  }
};
