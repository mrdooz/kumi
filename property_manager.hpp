#pragma once
#include "property.hpp"

#ifdef _DEBUG
#define TYPE_CHECK 1
#else
#define TYPE_CHECK 0
#endif

// this is really just a fancy wrapper around a big old slab of memory :)
class PropertyManager {
public:
  static bool create();
  static bool close();

  typedef std::function<void(PropertyId, int, const void *)> FnPropertyChanged;

  static PropertyManager& instance();

  typedef void *Token;

  template<typename T>
  void set_property(PropertyId id, const T &value) {
    set_property_raw(id, (const void *)&value, sizeof(value));
    notify_listeners(id);
  }

  void set_property_raw(PropertyId id, const void *buf, int len) {
    type_check(id, len);
    memcpy(&_raw_data[id], buf, len);
    notify_listeners(id);
  }

  template<typename T>
  T get_property(PropertyId id) {
    type_check(id, sizeof(T));
    return *(T*)&_raw_data[id];
  }

  void get_property_raw(PropertyId id, char *out, int len) {
#if TYPE_CHECK
    KASSERT(*((uint32*)&_raw_data[id-4]) == len);
#endif
    memcpy(out, &_raw_data[id], len);
  }

  PropertyId get_or_create_raw(
    const std::string &name, int size, const void *default_value,
    const FnPropertyChanged &cb = FnPropertyChanged()) {
    return get_or_create_raw_inner(name, &_ids, size, default_value, cb);
  }

  PropertyId get_or_create_raw(
    const std::string &name, Token token, int size, const void *default_value,
    const FnPropertyChanged &cb = FnPropertyChanged()) {
    auto key = std::make_pair(token, name);
    return get_or_create_raw_inner(key, &_compound_ids, size, default_value, cb);
  }

  template<typename T>
  PropertyId get_or_create(const std::string &name, const FnPropertyChanged &cb = FnPropertyChanged()) {
    return get_or_create_raw(name, sizeof(T), nullptr, cb);
  }

  template<typename T>
  PropertyId get_or_create(const char *name, Token token, const FnPropertyChanged &cb = FnPropertyChanged()) {
    return get_or_create_raw(name, token, sizeof(T), nullptr, cb);
  }

  PropertyId get_or_create_placeholder(const std::string &name, const FnPropertyChanged &cb = FnPropertyChanged()) {
    return get_or_create_raw(name, sizeof(PropertyId), nullptr, cb);
  }

private:

  PropertyManager();

#if TYPE_CHECK
  void type_check(PropertyId id, int size) {
    int cur_size = *((uint32*)&_raw_data[id-4]);
    KASSERT(cur_size == size);
  }
#else
  void type_check(PropertyId id, int size) {}
#endif

  void notify_listeners(PropertyId id) {
    auto it = _listeners.find(id);
    if (it != _listeners.end()) {
      for (auto j = begin(it->second); j != end(it->second); ++j) {
        int len = *(int *)&_raw_data[id-4];
        const void *data = (const void *)&_raw_data[id];
        (*j)(id, len, data);
      }
    }
  }

  template<class Cont, class Key>
  PropertyId get_or_create_raw_inner(
    const Key &key, Cont *cont, int size, const void *default_value, const FnPropertyChanged &cb) {
    auto it = cont->find(key);
    PropertyId id;
    if (it == cont->end()) {
      // store len
      *(uint32*)&_raw_data[_property_ofs] = size;
      _property_ofs += sizeof(uint32);
      // store value
      if (default_value)
        memcpy(&_raw_data[_property_ofs], default_value, size);
      id = (*cont)[key] = _property_ofs;
      _property_ofs += size;
    } else {
      id = it->second;
    }
    if (cb)
      _listeners[id].push_back(cb);
    return id;
  }

  typedef std::pair<Token, std::string> CompoundKey;

  std::unordered_map<std::string, PropertyId> _ids;
  std::map<CompoundKey, PropertyId> _compound_ids;
  std::map<PropertyId, std::vector<FnPropertyChanged> > _listeners;

  std::vector<uint8> _raw_data;
  uint32 _property_ofs;

  static PropertyManager *_instance;
};

#define PROPERTY_MANAGER PropertyManager::instance()
