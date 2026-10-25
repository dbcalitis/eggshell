#ifndef TOKENIZER_H
#define TOKENIZER_H

#include "common.h"

#define TOKENS \
    TOKEN(TOKEN_ERROR) \
    TOKEN(TOKEN_LPAREN) \
    TOKEN(TOKEN_RPAREN) \
    TOKEN(TOKEN_WORD) \
    TOKEN(TOKEN_STRING) \
    TOKEN(TOKEN_SINGLE_QUOTE) \
    TOKEN(TOKEN_DOUBLE_QUOTE) \
    TOKEN(TOKEN_PIPE) \
    TOKEN(TOKEN_REDIRECTION_IN) \
    TOKEN(TOKEN_REDIRECTION_OUT) \
    TOKEN(TOKEN_BACKGROUND) \
    TOKEN(TOKEN_ENV_VAR) \
    TOKEN(TOKEN_EOF) \

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
    #define TOKEN(e) e,
    TOKENS
    #undef TOKEN
    TOKEN_COUNT
};

#if defined(DEBUG) && !defined(TOKENIZER_H)
extern const char * const TOKEN_STRINGS[];
#endif // DEBUG


typedef struct token Token;
struct token
{
    enum token_type type;
    String string;
};

enum token_type tokenize(Tokenizer *tokenizer, Token *t_out);
void reset_tokenizer(Tokenizer *tokenizer);

#endif // TOKENIZER_H
