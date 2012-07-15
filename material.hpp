#pragma once

#include "property.hpp"
#include "property_manager.hpp"
#include "shader.hpp"

class Material {
public:
  struct Property {

    Property(const std::string &name, PropertyType::Enum type, float value)
      : name(name), type(type) { _float4[0] = value; }

    Property(const std::string &name, PropertyType::Enum type, int value)
      : name(name), type(type) { _int = value; }

    Property(const std::string &name, PropertyType::Enum type, const XMFLOAT4 &value)
      : name(name), type(type) { memcpy(&_float4, &value, sizeof(value)); }

    void property_changed(PropertyId id, int len, const void *value) {
      memcpy(_data, value, len);
    }

    std::string name;
    std::string filename;
    PropertyType::Enum type;
    union {
      float _float4[4];
      int _int;
      char *_data[4*sizeof(float)];
    };
    GraphicsObjectHandle resource;
    PropertyId id;
    PropertyId class_id;
  };

  Material(const std::string &name);
  ~Material();

  template<typename T>
  Property *add_property(const std::string &name, PropertyType::Enum type, const T &value) {
    using namespace std::tr1::placeholders;
    using namespace std;
    Property *prop = new Property(name, type, value);
    auto id = PROPERTY_MANAGER.get_or_create_raw(name.c_str(), this, sizeof(T), &value, std::bind(&Property::property_changed, prop, _1, _2, _3));
    prop->id = id;
    std::string class_name = "Material::" + name;
    PropertyId class_id = PROPERTY_MANAGER.get_or_create_placeholder(class_name.c_str());
    prop->class_id = class_id;
    _properties.push_back(prop);
    _properties_by_name[name] = prop;
    _properties_by_class_id[prop->class_id] = prop;
    _property_list.push_back(make_pair(class_id, prop));
    typedef std::pair<PropertyId, Property *> V;
    sort(begin(_property_list), end(_property_list), 
      [&](const V &a, const V& b) { return a.second->class_id < b.second->class_id; });
    return prop;
  }

  const std::string &name() const { return _name; }
  const std::vector<Property *> &properties() const { return _properties; }

  Property *property_by_name(const std::string &name);

  void fill_resource_views(const ResourceViewArray &props, TextureArray *out) const;
  void fill_cbuffer(CBuffer *cbuffer) const;
  int flags() const;
  void add_flag(int flag);

private:


  int _flags;
  std::string _name;
  std::vector<Property *> _properties;
  std::map<std::string, Property *> _properties_by_name;
  std::map<int, Property *> _properties_by_class_id;
  std::vector<std::pair<PropertyId, Property *> > _property_list; // [(class-id, property)]
};
