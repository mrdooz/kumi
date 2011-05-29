
#line 1 "text_parser.rl"
#include "stdafx.h"
#include "text_parser.hpp"
using std::vector;


#line 2 "text_parser.cpp"
static const char _foo_actions[] = {
	0, 1, 0, 1, 1, 1, 5, 1, 
	6, 1, 7, 1, 8, 1, 9, 1, 
	10, 1, 11, 1, 12, 1, 13, 1, 
	14, 1, 15, 1, 16, 1, 17, 1, 
	18, 1, 19, 2, 2, 3, 2, 2, 
	4
};

static const unsigned char _foo_key_offsets[] = {
	0, 5, 6, 7, 8, 9, 12, 13, 
	14, 15, 16, 17, 21, 23, 24, 25, 
	29, 30, 31, 32, 36, 37, 38, 39, 
	40, 41, 45, 46, 47, 48, 50, 54, 
	58, 71, 76, 79, 82, 85, 88, 91, 
	94, 100, 106, 107, 108, 115, 122, 129, 
	136, 143, 150
};

static const char _foo_trans_keys[] = {
	32, 102, 112, 9, 13, 111, 110, 116, 
	45, 102, 115, 119, 97, 109, 105, 108, 
	121, 32, 58, 9, 13, 105, 116, 122, 
	101, 32, 58, 9, 13, 121, 108, 101, 
	32, 58, 9, 13, 101, 105, 103, 104, 
	116, 32, 58, 9, 13, 111, 115, 45, 
	120, 121, 32, 58, 9, 13, 32, 58, 
	9, 13, 32, 91, 93, 102, 112, 9, 
	13, 48, 57, 65, 90, 97, 122, 32, 
	102, 112, 9, 13, 32, 9, 13, 32, 
	9, 13, 32, 9, 13, 32, 9, 13, 
	32, 9, 13, 32, 9, 13, 48, 57, 
	65, 90, 97, 122, 48, 57, 65, 90, 
	97, 122, 91, 93, 111, 48, 57, 65, 
	90, 97, 122, 110, 48, 57, 65, 90, 
	97, 122, 116, 48, 57, 65, 90, 97, 
	122, 45, 48, 57, 65, 90, 97, 122, 
	111, 48, 57, 65, 90, 97, 122, 115, 
	48, 57, 65, 90, 97, 122, 45, 48, 
	57, 65, 90, 97, 122, 0
};

static const char _foo_single_lengths[] = {
	3, 1, 1, 1, 1, 3, 1, 1, 
	1, 1, 1, 2, 2, 1, 1, 2, 
	1, 1, 1, 2, 1, 1, 1, 1, 
	1, 2, 1, 1, 1, 2, 2, 2, 
	5, 3, 1, 1, 1, 1, 1, 1, 
	0, 0, 1, 1, 1, 1, 1, 1, 
	1, 1, 1
};

static const char _foo_range_lengths[] = {
	1, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 1, 0, 0, 0, 1, 
	0, 0, 0, 1, 0, 0, 0, 0, 
	0, 1, 0, 0, 0, 0, 1, 1, 
	4, 1, 1, 1, 1, 1, 1, 1, 
	3, 3, 0, 0, 3, 3, 3, 3, 
	3, 3, 3
};

static const unsigned char _foo_index_offsets[] = {
	0, 5, 7, 9, 11, 13, 17, 19, 
	21, 23, 25, 27, 31, 34, 36, 38, 
	42, 44, 46, 48, 52, 54, 56, 58, 
	60, 62, 66, 68, 70, 72, 75, 79, 
	83, 93, 98, 101, 104, 107, 110, 113, 
	116, 120, 124, 126, 128, 133, 138, 143, 
	148, 153, 158
};

static const char _foo_indicies[] = {
	1, 2, 3, 1, 0, 4, 0, 5, 
	0, 6, 0, 7, 0, 9, 10, 11, 
	8, 12, 8, 13, 8, 14, 8, 15, 
	8, 16, 8, 16, 17, 16, 8, 18, 
	19, 8, 20, 8, 21, 8, 21, 22, 
	21, 8, 23, 8, 24, 8, 25, 8, 
	25, 26, 25, 8, 27, 8, 28, 8, 
	29, 8, 30, 8, 31, 8, 31, 32, 
	31, 8, 33, 0, 34, 0, 35, 0, 
	36, 37, 8, 36, 38, 36, 8, 37, 
	39, 37, 8, 41, 44, 45, 46, 47, 
	41, 42, 43, 43, 40, 1, 2, 3, 
	1, 48, 17, 17, 49, 22, 22, 50, 
	26, 26, 51, 32, 32, 52, 38, 38, 
	53, 39, 39, 54, 42, 43, 43, 55, 
	43, 43, 43, 56, 58, 57, 59, 57, 
	60, 43, 43, 43, 56, 61, 43, 43, 
	43, 56, 62, 43, 43, 43, 56, 7, 
	43, 43, 43, 56, 63, 43, 43, 43, 
	56, 64, 43, 43, 43, 56, 35, 43, 
	43, 43, 56, 0
};

static const char _foo_trans_targs[] = {
	32, 0, 1, 26, 2, 3, 4, 5, 
	32, 6, 12, 20, 7, 8, 9, 10, 
	11, 34, 13, 16, 14, 15, 35, 17, 
	18, 19, 36, 21, 22, 23, 24, 25, 
	37, 27, 28, 29, 30, 31, 38, 39, 
	32, 33, 40, 41, 42, 43, 44, 48, 
	32, 32, 32, 32, 32, 32, 32, 32, 
	32, 32, 32, 32, 45, 46, 47, 49, 
	50
};

static const char _foo_trans_actions[] = {
	31, 0, 0, 0, 0, 0, 0, 0, 
	33, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	9, 38, 0, 0, 0, 0, 0, 0, 
	27, 21, 17, 19, 15, 11, 13, 23, 
	25, 29, 5, 7, 0, 0, 35, 0, 
	35
};

static const char _foo_to_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	1, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0
};

static const char _foo_from_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	3, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0
};

static const unsigned char _foo_eof_trans[] = {
	1, 1, 1, 1, 1, 9, 9, 9, 
	9, 9, 9, 9, 9, 9, 9, 9, 
	9, 9, 9, 9, 9, 9, 9, 9, 
	9, 9, 1, 1, 1, 9, 9, 9, 
	0, 49, 50, 51, 52, 53, 54, 55, 
	56, 57, 58, 58, 57, 57, 57, 57, 
	57, 57, 57
};

static const int foo_start = 32;
static const int foo_first_final = 32;
static const int foo_error = -1;

static const int foo_en_main = 32;


#line 8 "text_parser.rl"


void parse_text(const char *str, vector<Token> *out)
{
    const char *p = str, *pe = str + strlen(str);
    const char *ts = str, *te = str;
    char *eof = NULL;
    int cs, act;
    bool in_cmd = false;
    
#line 158 "text_parser.cpp"
	{
	cs = foo_start;
	ts = 0;
	te = 0;
	act = 0;
	}

#line 164 "text_parser.cpp"
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
#line 181 "text_parser.cpp"
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
#line 19 "text_parser.rl"
	{act = 10;}
	break;
	case 4:
#line 34 "text_parser.rl"
	{act = 11;}
	break;
	case 5:
#line 24 "text_parser.rl"
	{te = p+1;{ out->push_back(Token(Token::kCmdOpen)); in_cmd = true; }}
	break;
	case 6:
#line 25 "text_parser.rl"
	{te = p+1;{ out->push_back(Token(Token::kCmdClose)); in_cmd = false; }}
	break;
	case 7:
#line 18 "text_parser.rl"
	{te = p+1;{ out->push_back(Token(Token::kChar, (*p))); }}
	break;
	case 8:
#line 26 "text_parser.rl"
	{te = p;p--;{ out->push_back(Token(Token::kPosX)); }}
	break;
	case 9:
#line 27 "text_parser.rl"
	{te = p;p--;{ out->push_back(Token(Token::kPosY)); }}
	break;
	case 10:
#line 28 "text_parser.rl"
	{te = p;p--;{ out->push_back(Token(Token::kFontWeight)); }}
	break;
	case 11:
#line 29 "text_parser.rl"
	{te = p;p--;{ out->push_back(Token(Token::kFontSize)); }}
	break;
	case 12:
#line 30 "text_parser.rl"
	{te = p;p--;{ out->push_back(Token(Token::kFontStyle)); }}
	break;
	case 13:
#line 31 "text_parser.rl"
	{te = p;p--;{ out->push_back(Token(Token::kFontFamily)); }}
	break;
	case 14:
#line 32 "text_parser.rl"
	{te = p;p--;{ out->push_back(Token(Token::kInt, atoi(ts))); }}
	break;
	case 15:
#line 19 "text_parser.rl"
	{te = p;p--;{ if (in_cmd) { out->push_back(Token(ts, te - ts)); } 
						else { for (const char *p = ts; p != te; ++p) out->push_back(Token(Token::kChar, *p)); } }}
	break;
	case 16:
#line 34 "text_parser.rl"
	{te = p;p--;{ if (!in_cmd) out->push_back(Token(Token::kChar, (*p))); }}
	break;
	case 17:
#line 18 "text_parser.rl"
	{te = p;p--;{ out->push_back(Token(Token::kChar, (*p))); }}
	break;
	case 18:
#line 34 "text_parser.rl"
	{{p = ((te))-1;}{ if (!in_cmd) out->push_back(Token(Token::kChar, (*p))); }}
	break;
	case 19:
#line 1 "NONE"
	{	switch( act ) {
	case 10:
	{{p = ((te))-1;} if (in_cmd) { out->push_back(Token(ts, te - ts)); } 
						else { for (const char *p = ts; p != te; ++p) out->push_back(Token(Token::kChar, *p)); } }
	break;
	case 11:
	{{p = ((te))-1;} if (!in_cmd) out->push_back(Token(Token::kChar, (*p))); }
	break;
	}
	}
	break;
#line 310 "text_parser.cpp"
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
#line 321 "text_parser.cpp"
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

#line 40 "text_parser.rl"

}
