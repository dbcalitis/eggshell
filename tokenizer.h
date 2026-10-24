#ifndef TOKENIZER_H
#define TOKENIZER_H

#include "common.h"

enum tokenizer_state
{
    STATE_NEW_TOKEN,
    STATE_GENERAL,
    STATE_IN_QUOTE,
    STATE_IN_DQUOTE,
};

typedef struct tokenizer Tokenizer;
struct tokenizer
{
    const U8 *input;
    const U8 *current_byte;
    enum tokenizer_state state;
};

enum token_type
{
    TOKEN_ERROR = -1,
    TOKEN_LPAREN = 0,
    TOKEN_RPAREN,
    TOKEN_WORD,
    TOKEN_STRING,
    TOKEN_SINGLE_QUOTE,
    TOKEN_DOUBLE_QUOTE,
    TOKEN_PIPE,
    TOKEN_REDIRECTION_IN,
    TOKEN_REDIRECTION_OUT,
    TOKEN_BACKGROUND,
    TOKEN_ENV_VAR,
    TOKEN_EOF
};

typedef struct token Token;
struct token
{
    enum token_type type;
    String string;
};

enum token_type tokenize(Tokenizer *tokenizer, Token *t_out);
void reset_tokenizer(Tokenizer *tokenizer);

#endif // TOKENIZER_H
