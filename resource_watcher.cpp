#include "stdafx.h"
#include "resource_watcher.hpp"
#include "file_watcher.hpp"
#include "utils.hpp"

using std::vector;

namespace {

	struct RawSection {
		RawSection(const string &name, const char *start, size_t len) : name(name), start(start), len(len) {}
		string name;
		const char *start;
		size_t len;
	};

	const char *next_row(const char *buf, const char **pos, string *row)
	{
		const char *start = buf;
		char ch = *buf;
		while (ch != '\n' && ch != '\r' && ch != '\0')
			ch = *++buf;

		*row = string(start, buf - start);
		*pos = start;

		while (ch == '\r' || ch == '\n')
			ch = *++buf;

		return ch == '\0' ? NULL : buf;
	}

	void parse_sections(const char *buf, size_t len, vector<RawSection> *sections)
	{
		// section header "---[[[ HEADER NAME ]]]---"
		string cur;
		const char *last_section = NULL;
		const char *next = buf;
		string header;
		do {
			const char *pos;
			next = next_row(next, &pos, &cur);
			const char *start = strstr(cur.c_str(), "---[[[");
			const char *end = strstr(cur.c_str(), "]]]---");
			if (start && end) {
				// save the previous section
				if (last_section) 
					sections->push_back(RawSection(header, last_section, pos - last_section));

				int ofs = start - cur.c_str() + 6;
				const char *p = &cur.data()[ofs];
				header = trim(cur.substr(ofs, end - p));
				last_section = next;
			}
		} while (next);

		if (!header.empty())
			sections->push_back(RawSection(header, last_section, &buf[len] - last_section));
	}

	uint32_t fnv_hash(const char *p, size_t len)
	{
		const uint32_t FNV32_prime = 16777619UL;
		const uint32_t FNV32_init  = 2166136261UL;

		static uint32_t h = FNV32_init;

		for (size_t i = 0; i < len; i++) {
			h = h * FNV32_prime;
			h = h ^ p[i];
		}

		return h;
	}
}

void extract_sections(const char *buf, size_t len, Sections *sections)
{
	vector<RawSection> raw_sections;
	parse_sections(buf, len, &raw_sections);
	for (size_t i = 0; i < raw_sections.size(); ++i) {
		const RawSection &raw = raw_sections[i];
		sections->insert(make_pair(raw.name, Section(raw.name, string(raw.start, raw.len))));
	}
}

ResourceWatcher *ResourceWatcher::_instance = nullptr;

ResourceWatcher::ResourceWatcher()
{
}

ResourceWatcher &ResourceWatcher::instance()
{
	if (!_instance)
		_instance = new ResourceWatcher;
	return *_instance;
}

void ResourceWatcher::process_deferred()
{
	Context *ctx = NULL;
	while (_modified_files.try_pop(ctx)) {
		void *buf;
		size_t len;
		if (load_file(ctx->filename.c_str(), &buf, &len))
			ctx->cb(ctx->token, ctx->filename.c_str(), buf, len);
		SAFE_ADELETE(buf);
	}
}

void ResourceWatcher::file_changed(void *token, FileEvent event, const char *old_new, const char *new_name)
{
	Context *context = (Context *)token;
	_modified_files.push(context);
}

bool ResourceWatcher::add_file_watch(void *token, const char *filename, bool initial_load, const FileChanged &fn)
{
	if (initial_load) {
		void *buf;
		size_t len;
		if (load_file(filename, &buf, &len))
			fn(token, filename, buf, len);
		SAFE_ADELETE(buf);
	}

	Context *context = new Context(token, filename, fn);
	_contexts.push_back(context);

	FILE_WATCHER.add_file_watch(context, filename, MakeDelegate(this, &ResourceWatcher::file_changed));

	return true;
}

bool ResourceWatcher::add_section_watch(const char *filename, const char *section, bool initial_load, const SectionChanged &fn)
{
	// handle the initial load first to avoid race issues and other potential strangeness
	if (initial_load) {
		void *buf;
		size_t len;
		if (!load_file(filename, &buf, &len))
			return false;

		Sections sections;
		extract_sections((const char *)buf, len, &sections);

		// find the requested section
		Sections::iterator it = sections.find(section);
		if (it != sections.end())
			fn(filename, section, it->second.data.c_str(), it->second.data.size());
	}

	return true;
}
