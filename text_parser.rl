#include "stdafx.h"
#include "text_parser.hpp"
using std::vector;

%%{
    machine foo;
    write data;
}%%

void parse_text(const char *str, vector<Token> *out)
{
    const char *p = str, *pe = str + strlen(str);
    const char *ts = str, *te = str;
    char *eof = NULL;
    int cs, act;
    bool in_cmd = false;
    %%{
        action add_ch { out->push_back(Token(Token::kChar, fc)); }
		action add_id { if (in_cmd) { out->push_back(Token(ts, te - ts)); } 
						else { for (const char *p = ts; p != te; ++p) out->push_back(Token(Token::kChar, *p)); } }
        ws = space*;
        
        main := |*
            '[[' => { out->push_back(Token(Token::kCmdOpen)); in_cmd = true; };
            ']]' => { out->push_back(Token(Token::kCmdClose)); in_cmd = false; };
            (ws 'pos-x' ws ':' ws) => { out->push_back(Token(Token::kPosX)); };
            (ws 'pos-y' ws ':' ws) => { out->push_back(Token(Token::kPosY)); };
            (ws 'font-weight' ws ':' ws) => { out->push_back(Token(Token::kFontWeight)); };
            (ws 'font-size' ws ':' ws) => { out->push_back(Token(Token::kFontSize)); };
            (ws 'font-style' ws ':' ws) => { out->push_back(Token(Token::kFontStyle)); };
            (ws 'font-family' ws ':' ws) => { out->push_back(Token(Token::kFontFamily)); };
            digit+ => { out->push_back(Token(Token::kInt, atoi(ts))); };
            alnum+ =>  add_id;
            space => { if (!in_cmd) out->push_back(Token(Token::kChar, fc)); };
            any =>  add_ch;
        *|;

        write init;
        write exec;
    }%%
}
