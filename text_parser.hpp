#pragma once
#include <string.h>

struct Token {
	enum Type {
		kChar,
		kInt,
		kId,
		kStyleOpen,
		kStyleClose,
		kFontFamily,
		kFontWidth,
		kFontStyle
	};

	Token(Type cmd) : type(cmd) {}
	Token(Type cmd, int value) : type(cmd), num(value) { }
	Token(Type cmd, const char *id, int len) : type(cmd), id(strdup(id)) { }
	Token(const Token &rhs) { assign(rhs); }
	Token &operator=(const Token &rhs) { assign(rhs); return *this; }
	~Token() { if (type == kId) { free(id); id= NULL; } }

	Type type;
	union {
		char ch;
		int num;
		char *id;
		int width;
		int style;
	};

	void assign(const Token &rhs) { if (this == &rhs) return; type = rhs.type; if (type == kId) id = strdup(rhs.id); else num = rhs.num; }
};
