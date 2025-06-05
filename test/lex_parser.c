#include <stdio.h>
#include <stdlib.h>

#define MAX_ARR 1024

enum lex_type {
  TOKEN_ERROR = -1,
  TOKEN_LPAREN = 0,
  TOKEN_RPAREN,
  TOKEN_IDENT,
  TOKEN_STRING,
  TOKEN_PIPE,
  TOKEN_REDIRECTION,
  TOKEN_BACKGROUND,
  TOKEN_ENV_VAR,
  TOKEN_EOF
};

enum ast_type {
  AST_COMMAND,
  AST_STRING_LITERAL,
  AST_PIPE,
  AST_REDIRECTION,
  AST_CAPTURE_STRING
};

struct token {
  enum lex_type type;
  char *value;
};

struct ast {
  enum ast_type type;

  union {
    struct {
      char *command_name;
      struct ast **args;
      int nargs;
    };

    struct {
      struct ast *pipe_left_child;
      struct ast *pipe_right_child;
    };

    struct {
      char redir_type[3];
      struct ast *command;
      char *target;
    };

    struct {
      char *string_literal;
    };
  };
};

struct ast *_ast_nodes[MAX_ARR];
int _n_ast_nodes = 0;

struct ast *ast_new() {
  _ast_nodes[++_n_ast_nodes] = (struct ast *)malloc(sizeof(struct ast));
  return _ast_nodes[_n_ast_nodes];
};

void clear_ast() {
  for (int i = 0; i < _n_ast_nodes; i++) {
    struct ast *tree = _ast_nodes[i];
    if (tree->type == AST_COMMAND) {
      if (tree->command_name != NULL) {
        free(tree->command_name);
      }
    } else if (tree->type == AST_STRING_LITERAL) {
      if (tree->string_literal != NULL) {
        free(tree->string_literal);
      }
    } else if (tree->type == AST_PIPE) {
      // nothing, maybe pointers = null?
    } else if (tree->type == AST_REDIRECTION) {
      if (tree->target != NULL) {
        free(tree->target);
      }
    }

    // TODO(daria): add capture string later.

    free(tree);
  }

  _n_ast_nodes = 0;
}

int lex(struct token *t, const char **line) {
  const char **s = line;

  while (isspace(**line)) {
    (*line)++;
  }

  int i = 0;
  int j = -1; // number of tokens

  if (**s == '\0') {
    *t = (struct token){.type = TOKEN_EOF, .value = "\0"};
    return TOKEN_EOF;
  } else if (**s == '>') {
    *t = (struct token){.type = TOKEN_REDIRECTION};
    t->value = (char *)calloc(sizeof(char), 2);
    t->value[0] = '>';
    t->value[1] = '\0';
    (*line)++;
  }

  return 0;
}

int main() { return 0; }
