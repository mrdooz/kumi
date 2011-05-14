#pragma once

using std::string;
using std::vector;
using std::map;
using Concurrency::concurrent_queue;

enum FileEvent;

struct Section {
	Section(const string &name, const string &data) : name(name), data(data) {}
	string name;
	string data;
};

typedef map<string, Section> Sections;

void extract_sections(const char *buf, size_t len, Sections *sections);

class ResourceWatcher
{
public:
	typedef FastDelegate3<const char * /*filename*/, const void * /*buf*/, size_t /*len*/> FileChanged;
	typedef FastDelegate4<const char * /*filename*/, const char * /*section*/, const char * /*buf*/, size_t /*len*/> SectionChanged;

	bool add_file_watch(const char *filename, bool initial_load, const FileChanged &fn);
	bool add_section_watch(const char *filename, const char *section, bool initial_load, const SectionChanged &fn);

	static ResourceWatcher &instance();
private:
	void file_changed(FileEvent event, const char *old_new, const char *new_name);

	concurrent_queue<string> _modified_files;

	ResourceWatcher();
	static ResourceWatcher *_instance;
};

#define RESOURCE_WATCHER ResourceWatcher::instance()