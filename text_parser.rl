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
        action add_ch { out->push_back(fc); }
        ws = space*;
        
        main := |*
            '[[' => { out->push_back(Token(Token::kStyleOpen)); in_cmd = true; };
            ']]' => { out->push_back(Token(Token::kStyleOpen)); in_cmd = false; };
            (ws 'font-width' ws ':' ws) => { out->push_back(Token(Token::kFontWidth)); };
            digit+ => { out->push_back(Token(Token::kInt, atoi(ts))); };
            alnum+ => { out->push_back(Token(Token::kId, ts, te - ts)); };
            space => { if (!in_cmd) out->push_back(Token(Token::kChar, fc)); };
            any => { out->push_back(Token(Token::kChar, fc)); };
        *|;

        write init;
        write exec;
    }%%
}
