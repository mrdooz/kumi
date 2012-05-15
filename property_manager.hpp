#pragma once

// this is really just a fancy wrapper around a big old slab of memory :)
class PropertyManager {
public:
  static bool create();
  static bool close();

  static PropertyManager& instance();

  PropertyManager();

  typedef void *Token;
  typedef uint32 PropertyId;

  typedef std::pair<Token, std::string> CompoundKey;

#ifdef _DEBUG
#define TYPE_CHECK(id, type) assert(*((uint32*)&_raw_data[id-4]) == sizeof(type))
#else
#define TYPE_CHECK(id, type)
#endif

  template<typename T>
  void set_property(PropertyId id, const T &value) {
    TYPE_CHECK(id, T);
    *(T*)&_raw_data[id] = value;
  }

  template<typename T>
  T get_property(PropertyId id) {
    TYPE_CHECK(id, T);
    return *(T*)&_raw_data[id];
  }

  void get_property_raw(PropertyId id, uint8 *out, int len) {
#if _DEBUG
    assert(*((uint32*)&_raw_data[id-4]) == len);
#endif
    memcpy(out, &_raw_data[id], len);
  }

  PropertyId get_or_create_raw(const char *name, int size, const void *default_value) {
    auto it = _ids.find(name);
    PropertyId id;
    if (it == _ids.end()) {
#if _DEBUG
      *(uint32*)&_raw_data[_property_ofs] = size;
      _property_ofs += sizeof(uint32);
#endif
      if (default_value)
        memcpy(&_raw_data[_property_ofs], default_value, size);
      id = _ids[name] = _property_ofs;
      _property_ofs += size;
    } else {
      id = it->second;
    }
    return id;
  }

  template<typename T>
  PropertyId get_or_create(const char *name) {
    static T t;
    PropertyId id = get_or_create_raw(name, sizeof(T), &t);
    TYPE_CHECK(id, T);
    return id;
  }

  PropertyId get_or_create_raw(const char *name, Token token, int size, const void *default_value) {
    auto key = std::make_pair(token, std::string(name));
    auto it = _compound_ids.find(key);
    PropertyId id;
    if (it == _compound_ids.end()) {
#if _DEBUG
      *(uint32*)&_raw_data[_property_ofs] = size;
      _property_ofs += sizeof(uint32);
#endif
      if (default_value)
        memcpy(&_raw_data[_property_ofs], default_value, size);
      id = _compound_ids[key] = _property_ofs;
      _property_ofs += size;
    } else {
      id = it->second;
    }
    return id;
  }

  template<typename T>
  PropertyId get_or_create(const char *name, Token token) {
    static T t;
    PropertyId id = get_or_create_raw(name, token, sizeof(T), &t);
    TYPE_CHECK(id, T);
    return id;
  }


private:

  std::unordered_map<std::string, PropertyId> _ids;
  std::map<CompoundKey, PropertyId> _compound_ids;

  std::vector<uint8> _raw_data;
  uint32 _property_ofs;

  static PropertyManager *_instance;
};

#define PROPERTY_MANAGER PropertyManager::instance()
