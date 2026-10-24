
#include "tokenizer.h"
#include "common.h"

static Int
is_blank(Int c)
{
    return (c == ' ' || c == '\t');
}

static Int
is_alpha(Int c)
{
    return ((unsigned)c|32) - 'a' < 26;
}

static Int
is_digit(Int c)
{
    return (unsigned)c-'0' < 10;
}

void
reset_tokenizer(Tokenizer *tokenizer)
{
    tokenizer->input = NULL;
    tokenizer->current_byte = NULL;
    tokenizer->state = STATE_NEW_TOKEN;
}

enum token_type
tokenize(
        Tokenizer *tokenizer,
        Token *t_out)
{
    enum token_type detected_token_type = TOKEN_ERROR;

    U8 token_length = 0;

    for (;;tokenizer->current_byte++)
    {
        // EOF / NULL CHARACTER 
        if (*tokenizer->current_byte == '\0'
            && token_length > 0)
        {
            detected_token_type = TOKEN_WORD;
            t_out->string.length = token_length;
            break;
        }
        else if (*tokenizer->current_byte == '\0')
        {
            detected_token_type = TOKEN_EOF;
            break;
        }

        // WHITESPACE - Space & Tab
        Int is_whitespace = is_blank(*tokenizer->current_byte);
        if (is_whitespace
            && token_length > 0)
        {
            detected_token_type = TOKEN_WORD;
            t_out->string.length = token_length;
            break;
        }
        else if (is_whitespace)
        {
            continue;
        }

        if (is_alpha(*tokenizer->current_byte)
            || is_digit(*tokenizer->current_byte))
        {
            if (tokenizer->state == STATE_NEW_TOKEN)
            {
                t_out->string.data = Cast(const U8 *)tokenizer->current_byte;
                tokenizer->state = STATE_GENERAL;
            }

            token_length += 1;
            continue;
        }

        switch(*tokenizer->current_byte)
        {
            case '(':
            {
                detected_token_type = TOKEN_LPAREN;
            } break;
            case ')':
            {
                detected_token_type = TOKEN_RPAREN;
            } break;
            case '\'':
            {
                detected_token_type = TOKEN_SINGLE_QUOTE;
            } break;
            case '\"':
            {
                detected_token_type = TOKEN_DOUBLE_QUOTE;
            } break;
            case '|':
            {
                detected_token_type = TOKEN_PIPE;
            } break;
            case '>':
            {
                detected_token_type = TOKEN_REDIRECTION_IN;
            } break;
            case '<':
            {
                detected_token_type = TOKEN_REDIRECTION_OUT;
            } break;
            default:
            {
                // Unknown character
                detected_token_type = TOKEN_ERROR;
            } break;
        }

        tokenizer->current_byte++;
        break;
    }

    tokenizer->state = STATE_NEW_TOKEN;
    t_out->type = detected_token_type;

    return detected_token_type;
}
