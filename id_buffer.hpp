#pragma once

using std::stack;
using std::vector;
using std::pair;

class GraphicsObjectHandle;

template<class Traits, int N>
class IdBufferBase {
public:
  typedef typename Traits::Value T;
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

template<class Key, class Value>
struct SearchableTraits {

  typedef std::pair<Key, Value> KeyValuePair;
  typedef Value Value;
  typedef Key Key;
  typedef KeyValuePair Elem;

  static Key &get_key(KeyValuePair &kv) {
    return kv.first;
  }

  static Value &get(KeyValuePair &kv) {
    return kv.second;
  }
};

template<class T>
struct SingleTraits {
  typedef T Type;
  typedef T Value;
  typedef T Elem;
  static T& get(Elem& t) {
    return t;
  }
};

template<typename Key, typename Value, int N>
struct SearchableIdBuffer : public IdBufferBase<SearchableTraits<Key, Value>, N> {

  typedef SearchableTraits<Key, Value> Traits;
  typedef IdBufferBase<Traits, N> Parent;

  SearchableIdBuffer(const typename Parent::Deleter &fn_deleter) 
    : Parent(fn_deleter)
  {
  }

  void set_pair(int idx, const typename Traits::Elem &e) {
    assert(idx >= 0 && idx < N);
    _buffer[idx] = e;
    _lookup[Traits::get_key(_buffer[idx])] = idx;
  }

  int idx_from_token(const typename Traits::Key &key) {
    auto it = _lookup.find(key);
    return it == end(_lookup) ? -1 : it->second;
  }

  template<typename R>
  R find(const typename Traits::Key &key, R def) {
    int idx = idx_from_token(key);
    return idx == -1 ? def : Traits::get(_buffer[idx]);
  }

  std::unordered_map<typename Traits::Key, int> _lookup;
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
