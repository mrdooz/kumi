#pragma once

using std::string;
using std::vector;

struct Section {
	Section(const string &name, const string &data) : name(name), data(data) {}
	string name;
	string data;
};

void extract_sections(const char *buf, size_t len, vector<Section> *sections);

class ResourceWatcher
{
public:
	typedef FastDelegate3<const char * /*filename*/, void * /*buf*/, size_t /*len*/> FileChanged;
	typedef FastDelegate4<const char * /*filename*/, const char * /*section*/, const char * /*buf*/, size_t /*len*/> SectionChanged;

	void add_file_watch(const char *filename, bool load_file, bool initial_callback, const FileChanged &fn);
	void add_section_watch(const char *filename, const char *section, bool initial_callback, const SectionChanged &fn);
private:

};
