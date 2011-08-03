#pragma once

struct Io;

class KumiLoader {
public:
	bool load(const char *filename, Io *io);
private:
	bool load_meshes(const char *buf);
	bool load_materials(const char *buf);
};