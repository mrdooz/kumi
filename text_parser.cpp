
#line 1 "test1.rl"
#include "stdafx.h"
#include "text_parser.hpp"
using std::vector;


#line 2 "text_parser.cpp"
static const char _foo_actions[] = {
	0, 1, 0, 1, 1, 1, 5, 1, 
	6, 1, 7, 1, 8, 1, 9, 1, 
	10, 1, 11, 1, 12, 1, 13, 1, 
	14, 2, 2, 3, 2, 2, 4
};

static const char _foo_key_offsets[] = {
	0, 4, 5, 6, 7, 8, 9, 10, 
	11, 12, 13, 17, 29, 33, 36, 42, 
	48, 49, 50, 57, 64, 71
};

static const char _foo_trans_keys[] = {
	32, 102, 9, 13, 111, 110, 116, 45, 
	119, 105, 100, 116, 104, 32, 58, 9, 
	13, 32, 91, 93, 102, 9, 13, 48, 
	57, 65, 90, 97, 122, 32, 102, 9, 
	13, 32, 9, 13, 48, 57, 65, 90, 
	97, 122, 48, 57, 65, 90, 97, 122, 
	91, 93, 111, 48, 57, 65, 90, 97, 
	122, 110, 48, 57, 65, 90, 97, 122, 
	116, 48, 57, 65, 90, 97, 122, 45, 
	48, 57, 65, 90, 97, 122, 0
};

static const char _foo_single_lengths[] = {
	2, 1, 1, 1, 1, 1, 1, 1, 
	1, 1, 2, 4, 2, 1, 0, 0, 
	1, 1, 1, 1, 1, 1
};

static const char _foo_range_lengths[] = {
	1, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 1, 4, 1, 1, 3, 3, 
	0, 0, 3, 3, 3, 3
};

static const char _foo_index_offsets[] = {
	0, 4, 6, 8, 10, 12, 14, 16, 
	18, 20, 22, 26, 35, 39, 42, 46, 
	50, 52, 54, 59, 64, 69
};

static const char _foo_indicies[] = {
	1, 2, 1, 0, 3, 0, 4, 0, 
	5, 0, 6, 0, 8, 7, 9, 7, 
	10, 7, 11, 7, 12, 7, 12, 13, 
	12, 7, 15, 18, 19, 20, 15, 16, 
	17, 17, 14, 1, 2, 1, 21, 13, 
	13, 22, 16, 17, 17, 23, 17, 17, 
	17, 24, 26, 25, 27, 25, 28, 17, 
	17, 17, 24, 29, 17, 17, 17, 24, 
	30, 17, 17, 17, 24, 6, 17, 17, 
	17, 24, 0
};

static const char _foo_trans_targs[] = {
	11, 0, 1, 2, 3, 4, 5, 11, 
	6, 7, 8, 9, 10, 13, 11, 12, 
	14, 15, 16, 17, 18, 11, 11, 11, 
	11, 11, 11, 11, 19, 20, 21
};

static const char _foo_trans_actions[] = {
	21, 0, 0, 0, 0, 0, 0, 23, 
	0, 0, 0, 0, 0, 0, 9, 28, 
	0, 0, 0, 0, 0, 17, 11, 13, 
	15, 19, 5, 7, 0, 0, 25
};

static const char _foo_to_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 1, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0
};

static const char _foo_from_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 3, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0
};

static const char _foo_eof_trans[] = {
	1, 1, 1, 1, 1, 8, 8, 8, 
	8, 8, 8, 0, 22, 23, 24, 25, 
	26, 26, 25, 25, 25, 25
};

static const int foo_start = 11;
static const int foo_first_final = 11;
static const int foo_error = -1;

static const int foo_en_main = 11;


#line 8 "test1.rl"


void parse_text(const char *str, vector<Token> *out)
{
    const char *p = str, *pe = str + strlen(str);
    const char *ts = str, *te = str;
    char *eof = NULL;
    int cs, act;
    bool in_cmd = false;
    
#line 97 "text_parser.cpp"
	{
	cs = foo_start;
	ts = 0;
	te = 0;
	act = 0;
	}

#line 103 "text_parser.cpp"
	{
	int _klen;
	unsigned int _trans;
	const char *_acts;
	unsigned int _nacts;
	const char *_keys;

	if ( p == pe )
		goto _test_eof;
_resume:
	_acts = _foo_actions + _foo_from_state_actions[cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 1:
#line 1 "NONE"
	{ts = p;}
	break;
#line 120 "text_parser.cpp"
		}
	}

	_keys = _foo_trans_keys + _foo_key_offsets[cs];
	_trans = _foo_index_offsets[cs];

	_klen = _foo_single_lengths[cs];
	if ( _klen > 0 ) {
		const char *_lower = _keys;
		const char *_mid;
		const char *_upper = _keys + _klen - 1;
		while (1) {
			if ( _upper < _lower )
				break;

			_mid = _lower + ((_upper-_lower) >> 1);
			if ( (*p) < *_mid )
				_upper = _mid - 1;
			else if ( (*p) > *_mid )
				_lower = _mid + 1;
			else {
				_trans += (unsigned int)(_mid - _keys);
				goto _match;
			}
		}
		_keys += _klen;
		_trans += _klen;
	}

	_klen = _foo_range_lengths[cs];
	if ( _klen > 0 ) {
		const char *_lower = _keys;
		const char *_mid;
		const char *_upper = _keys + (_klen<<1) - 2;
		while (1) {
			if ( _upper < _lower )
				break;

			_mid = _lower + (((_upper-_lower) >> 1) & ~1);
			if ( (*p) < _mid[0] )
				_upper = _mid - 2;
			else if ( (*p) > _mid[1] )
				_lower = _mid + 2;
			else {
				_trans += (unsigned int)((_mid - _keys)>>1);
				goto _match;
			}
		}
		_trans += _klen;
	}

_match:
	_trans = _foo_indicies[_trans];
_eof_trans:
	cs = _foo_trans_targs[_trans];

	if ( _foo_trans_actions[_trans] == 0 )
		goto _again;

	_acts = _foo_actions + _foo_trans_actions[_trans];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 )
	{
		switch ( *_acts++ )
		{
	case 2:
#line 1 "NONE"
	{te = p+1;}
	break;
	case 3:
#line 26 "test1.rl"
	{act = 5;}
	break;
	case 4:
#line 27 "test1.rl"
	{act = 6;}
	break;
	case 5:
#line 22 "test1.rl"
	{te = p+1;{ out->push_back(Token(Token::kStyleOpen)); in_cmd = true; }}
	break;
	case 6:
#line 23 "test1.rl"
	{te = p+1;{ out->push_back(Token(Token::kStyleOpen)); in_cmd = false; }}
	break;
	case 7:
#line 28 "test1.rl"
	{te = p+1;{ out->push_back(Token(Token::kChar, (*p))); }}
	break;
	case 8:
#line 24 "test1.rl"
	{te = p;p--;{ out->push_back(Token(Token::kFontWidth)); }}
	break;
	case 9:
#line 25 "test1.rl"
	{te = p;p--;{ out->push_back(Token(Token::kInt, atoi(ts))); }}
	break;
	case 10:
#line 26 "test1.rl"
	{te = p;p--;{ out->push_back(Token(Token::kId, (int)ts)); }}
	break;
	case 11:
#line 27 "test1.rl"
	{te = p;p--;{ if (!in_cmd) out->push_back(Token(Token::kChar, (*p))); }}
	break;
	case 12:
#line 28 "test1.rl"
	{te = p;p--;{ out->push_back(Token(Token::kChar, (*p))); }}
	break;
	case 13:
#line 27 "test1.rl"
	{{p = ((te))-1;}{ if (!in_cmd) out->push_back(Token(Token::kChar, (*p))); }}
	break;
	case 14:
#line 1 "NONE"
	{	switch( act ) {
	case 5:
	{{p = ((te))-1;} out->push_back(Token(Token::kId, (int)ts)); }
	break;
	case 6:
	{{p = ((te))-1;} if (!in_cmd) out->push_back(Token(Token::kChar, (*p))); }
	break;
	}
	}
	break;
#line 232 "text_parser.cpp"
		}
	}

_again:
	_acts = _foo_actions + _foo_to_state_actions[cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 0:
#line 1 "NONE"
	{ts = 0;}
	break;
#line 243 "text_parser.cpp"
		}
	}

	if ( ++p != pe )
		goto _resume;
	_test_eof: {}
	if ( p == eof )
	{
	if ( _foo_eof_trans[cs] > 0 ) {
		_trans = _foo_eof_trans[cs] - 1;
		goto _eof_trans;
	}
	}

	}

#line 33 "test1.rl"

}
