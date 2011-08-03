#pragma once

struct Io;
struct Scene;

class KumiLoader {
public:
	bool load(const char *filename, Io *io, Scene **scene);
private:
	bool load_meshes(const char *buf, Scene *scene);
	bool load_materials(const char *buf);
};