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
	typedef FastDelegate4<void * /*token*/, const char * /*filename*/, const void * /*buf*/, size_t /*len*/> FileChanged;
	typedef FastDelegate4<const char * /*filename*/, const char * /*section*/, const char * /*buf*/, size_t /*len*/> SectionChanged;

	bool add_file_watch(void *token, const char *filename, bool initial_load, const FileChanged &fn);
	bool add_section_watch(const char *filename, const char *section, bool initial_load, const SectionChanged &fn);
	void process_deferred();

	static ResourceWatcher &instance();
private:
	void file_changed(void *token, FileEvent event, const string &old_new, const string &new_name);

	struct Context {
		Context(void *token, const string &filename, const FileChanged &cb) : token(token), filename(filename), cb(cb) {}
		void *token;
		string filename;
		FileChanged cb;
	};

	vector<Context *> _contexts;
	typedef concurrent_queue<Context *> ModifiedFiles;
	ModifiedFiles _modified_files;

	ResourceWatcher();
	static ResourceWatcher *_instance;
};

#define RESOURCE_WATCHER ResourceWatcher::instance()