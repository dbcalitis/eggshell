#include <ctype.h>
#include <termios.h>
#include <unistd.h>

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <locale.h>

#include <sys/mman.h>
#include <sys/stat.h>

#include <pwd.h>
#include <spawn.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <fcntl.h>

#define static_arr_size(arr) (sizeof(arr) / sizeof(*(arr)))

#define uint8_t u_int8_t

#define BUFF_SIZE 100
#define MAX_ARGS 20

// To reset from terminal raw mode
struct termios orig_termios;

struct node {
  struct node *next;
  struct node *prev;
  char line[BUFF_SIZE];
};

struct linked_list {
  struct node *head;
  struct node *tail;
  int length;
};

struct linked_list history = {.head = NULL, .tail = NULL, .length = 0};

char *curr_path = NULL;

char *args[BUFF_SIZE]; // edit the amount later
int nargs = 0;

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
  AST_CAPTURE_STRING // $()
};

struct ast {
  uint8_t type;

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
      char *redir_type;
      struct ast *command;
      char *target;
    };

    // edit later
    struct {
      char *string_literal;
    };
  };
};

struct token {
  enum lex_type type;
  char *value;
};

struct ast *ast_new() {
  struct ast *tree = (struct ast *)malloc(sizeof(struct ast));

  return tree;
}
char _input[BUFF_SIZE] = "\0";
char _line[BUFF_SIZE] = "\0";

void disable_raw_mode();
void enable_raw_mode();

void add_history(const char *line);
int get_history(struct node **n, int dir);
void clear_history();

int exec_from_path(char **args, int nargs);

int egg_exit(char **args, int nargs);
int egg_history(char **args, int nargs);
int egg_cd(char **args, int nargs);

int egg_num_builtins();

int egg_execute_cmd(struct ast *head);

char *builtin_str[] = {"cd", "history", "exit"};
int (*builtin_func[])(char **, int) = {&egg_cd, &egg_history, &egg_exit};

uint8_t lex(struct token *t, const char **line);
uint8_t parse(struct ast **out, const char *line);
// PERF(daria): memory leaks from ast

int main() {
  setlocale(LC_ALL, "");
  atexit(disable_raw_mode);

  // Clears the screen
  printf("\e[1;1H\e[2J");

  enable_raw_mode();

  int length = 0;

  curr_path = getcwd(NULL, 0);

  struct node *current_history = NULL;
  atexit(clear_history);

  printf("%s\n\r > ", curr_path);
  fflush(stdout);

  // Shell input loop
  while (length != -1 && _input[0] != 17) { // CTRLQ
    _input[length] = '\0';
    printf("\r > %s", _line);
    fflush(stdout);

    length = read(STDIN_FILENO, _input, sizeof(_input));

    // Ignore Esc characters; includes arrow keys
    if (_input[0] == '\e') {
      continue;
    }

    if (_input[0] == 127) { // BSPACE
      current_history = NULL;
      _line[strlen(_line) - 1] = '\0';
    } else if (_input[0] == 13) { // ENTER
      current_history = NULL;
      add_history(_line);

      struct ast *tree;
      parse(&tree, _line);

      disable_raw_mode();
      egg_execute_cmd(tree);
      enable_raw_mode();

      _line[0] = '\0';
      printf("\r\n\r%s\n\r", curr_path);
    } else if (_input[0] == 11) { // CTRLK Go to older history
      if (get_history(&current_history, 1) == 1) {
        strcpy(_line, current_history->line);
      }
    } else if (_input[0] == 10) { // CTRLJ Go to newer history
      if (get_history(&current_history, 0)) {
        strcpy(_line, current_history->line);
      }
    } else if (_input[0] == 9) { // TAB for autocompletion
      // TODO(daria): make autocomplete with bash
    } else {
      if (strlen(_line) + strlen(_input) < BUFF_SIZE) {
        strcat(_line, _input);
      }
    }

    // clears current display line
    printf("\e[1G\e[2K");
  }

  return 0;
}

void disable_raw_mode() { tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios); }

void enable_raw_mode() {
  tcgetattr(STDIN_FILENO, &orig_termios);
  atexit(disable_raw_mode);

  // disables echo and canonical mode
  struct termios raw = orig_termios;
  raw.c_iflag &=
      ~(IGNBRK | PARMRK | IGNCR | BRKINT | INPCK | ISTRIP | ICRNL | IXON);
  raw.c_oflag &= ~(OPOST);
  raw.c_lflag &= ~(ECHO | ECHONL | IEXTEN | ICANON | ISIG);
  raw.c_cflag |= (CS8);

  raw.c_cc[VMIN] = 1;
  raw.c_cc[VTIME] = 0;

  tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

void add_history(const char *line) {
  struct node *n = (struct node *)malloc(sizeof(struct node));
  n->prev = NULL;
  n->next = NULL;
  strcpy(n->line, line);

  if (history.length == 0) {
    history.head = n;
    history.tail = n;
  } else {
    history.tail->next = n;
    n->prev = history.tail;
    history.tail = n;

    if (history.length == 5) {
      struct node *temp = history.head;
      history.head = history.head->next;
      free(temp);
      history.head->prev = NULL;
      history.length--;
    }
  }

  history.length++;
}

int get_history(struct node **n, int dir) {
  if (dir == 1) // go to older history
  {
    if (*n == NULL) {
      if (history.tail != NULL) {
        *n = history.tail;
        return 1;
      }
    } else if ((*n)->prev != NULL) {
      *n = (*n)->prev;
      return 1;
    }
  } else // go to newer history
  {
    if (*n != NULL) {
      if ((*n)->next != NULL) {
        *n = (*n)->next;
        return 1;
      }
    }
  }
  return 0;
}

void clear_history() {
  struct node *temp = history.head;
  while (history.head != NULL) {
    temp = history.head;
    history.head = temp->next;
    free(temp);
  }

  history.length = 0;
}

int exec_from_path(char **args, int nargs) {
  printf("\r\n\r");

  disable_raw_mode();
  int pid = fork();

  if (pid == 0) {
    int result = execvp(args[0], args);
    perror("execvp");
    exit(0);
  } else {
    int cpid = wait(NULL);
  }

  enable_raw_mode();

  return 1;
}

int egg_exit(char **args, int nargs) {
  // kill children
  // wait children
  exit(0);
}

int egg_history(char **args, int nargs) {
  if (nargs == 1) {
    struct node *curr = history.head;
    printf("\r\n\nPast Inputs:");

    while (curr != NULL) {
      printf("\n\r- %s \r", curr->line);
      curr = curr->next;
    }
  } else if (nargs > 1) {
    if (strcmp(args[1], "clear") == 0) {
      clear_history();
    }
  }

  return 1;
}

int egg_cd(char **args, int nargs) {
  int result;
  if (nargs == 1) {
    result = chdir(getenv("HOME"));
  } else {
    result = chdir(args[1]);
  }

  if (result == -1) {
    printf("\r\n");
    perror("chdir");
  } else {
    curr_path = getcwd(NULL, 0);
  }

  return 1;
}

int egg_num_builtins() { return sizeof(builtin_str) / sizeof(char *); }

int egg_execute_cmd(struct ast *head) {
  switch (head->type) {
  case AST_COMMAND: {
    char *args[head->nargs + 2];
    args[0] = head->command_name;
    size_t nargs = 1;

    args[0] = head->command_name;

    for (int i = 0; i < head->nargs; i++) {
      if (head->args[i]->type == AST_STRING_LITERAL) {
        args[nargs++] = head->args[i]->string_literal;
      }
    }

    args[nargs] = NULL;

    // TODO(daria): add builtins
    printf("\n\r");
    pid_t pid = fork();

    if (pid == 0) {
      execvp(head->command_name, args); // fix
      perror("execvp");
      exit(EXIT_FAILURE);
    } else if (pid > 0) {
      int status;
      waitpid(pid, &status, 0);

      if (WIFEXITED(status)) {
        printf("\n\rChild exited w/ status: %d", WIFEXITED(status));
      }
    } else {
      perror("fork");
    }
    break;
  }
  case AST_REDIRECTION: {
    if (strcmp(head->redir_type, ">") == 0) {
      int og_stdout = dup(STDOUT_FILENO);
      int fd = open(head->target, O_WRONLY | O_CREAT | O_TRUNC, 0644);

      if (fd < 0) {
        printf("\n\r");
        perror("open");
        return -1;
        // exit(EXIT_FAILURE);
      }

      dup2(fd, STDOUT_FILENO);
      close(fd);

      egg_execute_cmd(head->command);

      dup2(og_stdout, STDOUT_FILENO);
      close(og_stdout);
      break;
    } else if (strcmp(head->redir_type, "<") == 0) {
      int og_stdin = dup(STDIN_FILENO);
      int fd = open(head->target, O_RDONLY);

      if (fd < 0) {
        printf("\n\r");
        perror("open");
        return -1;
      }

      dup2(fd, STDIN_FILENO);
      close(fd);

      egg_execute_cmd(head->command);

      dup2(og_stdin, STDIN_FILENO);
      close(og_stdin);
    }
  }
  case AST_PIPE: {
    int pipefd[2];
    pipe(pipefd);

    pid_t pid1 = fork();

    if (pid1 == 0) {
      close(pipefd[0]);
      dup2(pipefd[1], STDOUT_FILENO);
      close(pipefd[1]);
      egg_execute_cmd(head->pipe_left_child);
      exit(EXIT_SUCCESS);
    }

    pid_t pid2 = fork();
    if (pid2 == 0) {
      close(pipefd[1]);
      dup2(pipefd[0], STDIN_FILENO);
      close(pipefd[0]);
      egg_execute_cmd(head->pipe_right_child);
      exit(EXIT_SUCCESS);
    }

    close(pipefd[0]);
    close(pipefd[1]);

    waitpid(pid1, NULL, 0);
    waitpid(pid2, NULL, 0);
  }
  }

  return 1;
}

uint8_t lex(struct token *t, const char **line) {
  const char **s = line;

  // ignore whitespace
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
    return TOKEN_REDIRECTION;
  } else if (**s == '<') {
    *t = (struct token){.type = TOKEN_REDIRECTION};
    t->value = (char *)calloc(sizeof(char), 2);
    t->value[0] = '<';
    t->value[1] = '\0';
    (*line)++;
    return TOKEN_REDIRECTION;
  } else if (**s == '|') {
    *t = (struct token){.type = TOKEN_PIPE};
    t->value = (char *)calloc(sizeof(char), 2);
    t->value[0] = '|';
    t->value[1] = '\0';
    (*line)++;
    return TOKEN_PIPE;
  } else if (**s == '(') {
    *t = (struct token){.type = TOKEN_LPAREN};
    t->value = (char *)calloc(sizeof(char), 2);
    t->value[0] = '(';
    t->value[1] = '\0';
    (*line)++;
    return TOKEN_LPAREN;
  } else if (**s == ')') {
    *t = (struct token){.type = TOKEN_RPAREN};
    t->value = (char *)calloc(sizeof(char), 2);
    t->value[0] = ')';
    t->value[1] = '\0';
    (*line)++;
    return TOKEN_RPAREN;
  } else if (**s == '&') {
    *t = (struct token){.type = TOKEN_BACKGROUND};
    t->value = (char *)calloc(sizeof(char), 2);
    t->value[0] = '&';
    t->value[1] = '\0';
    (*line)++;
    return TOKEN_BACKGROUND;
  } else if (**s == '$') {
    *t = (struct token){.type = TOKEN_ENV_VAR,
                        .value = calloc(sizeof(char), 10)};

    size_t len = strlen(t->value);
    size_t size = 10;

    // add parentheses
    while (isalpha(**s)) {
      if (len + 1 > size - 1) {
        size += 5;
        t->value = realloc(t->value, size);
        t->value[size - 1] = '\0';
      }

      t->value[len++] = **s;
      (*s)++;
    }

    return TOKEN_ENV_VAR;
  } else {
    *t =
        (struct token){.type = TOKEN_STRING, .value = calloc(sizeof(char), 10)};
    size_t is_str_lit = 0;

    if (**s == '"') {
      (*s)++;
      is_str_lit = 1;
    }

    while (**s != '\0' && **s != ' ') {
      // TODO(daria): account for format string ${}
      if (is_str_lit && **s == '"') {
        return TOKEN_STRING;
      }

      size_t len = strlen((t->value));
      size_t size = 10;

      if (len + 1 > sizeof(*(t->value))) {
        size += 5;
        t->value = realloc(t->value, size);
        t->value[size + 1] = '\0';
      }
      t->value[len++] = **s;
      (*s)++;
    }

    return TOKEN_STRING;
  }

  // error cannot identify token
  return TOKEN_EOF;
}

uint8_t parse(struct ast **out, const char *line) {
  struct ast *head = ast_new();
  size_t isfirst = 1;

  const char *l = line;
  struct token t;
  enum lex_type r;
  size_t size = 3; // size of ast.args

  while ((r = lex(&t, &l)) != TOKEN_EOF) {
    if (isfirst && r == TOKEN_STRING) {
      isfirst = 0;
      head->type = AST_COMMAND;
      head->command_name = t.value;
      head->args = (struct ast **)calloc(sizeof(struct ast *), size);
      head->nargs = 0;
    } else if (!isfirst && r == TOKEN_STRING) {
      if (head->type == AST_REDIRECTION) {
        head->target = t.value;
      } else {
        if (head->nargs + 1 > size) {
          size += 2;
          head->args = realloc(head->args, size);
        }
        head->args[head->nargs] = ast_new();
        head->args[head->nargs]->type = AST_STRING_LITERAL;
        head->args[head->nargs]->string_literal = t.value;
        head->nargs++;
      }
    } else if (r == TOKEN_PIPE) {
      struct ast *temp = ast_new();
      temp->type = AST_PIPE;
      temp->pipe_left_child = head;

      // printf("\r\n\nParsing right child");
      temp->pipe_right_child = ast_new();

      r = lex(&t, &l);

      if (r == TOKEN_STRING) {
        temp->pipe_right_child->type = AST_COMMAND;
        temp->pipe_right_child->command_name = t.value;

        size_t size = 3;
        temp->pipe_right_child->args =
            (struct ast **)calloc(sizeof(struct ast *), size);
        temp->pipe_right_child->nargs = 0;

        // HACK(daria): pipe right child should be done with recursion
        while ((r = lex(&t, &l)) != TOKEN_EOF && r == TOKEN_STRING) {
          if (temp->pipe_right_child->nargs + 1 > size) {
            size += 2;
            temp->pipe_right_child->args =
                realloc(temp->pipe_right_child->args, size);
          }
          temp->pipe_right_child->args[temp->pipe_right_child->nargs] =
              ast_new();
          temp->pipe_right_child->args[temp->pipe_right_child->nargs]->type =
              AST_STRING_LITERAL;
          temp->pipe_right_child->args[temp->pipe_right_child->nargs]
              ->string_literal = t.value;
          temp->pipe_right_child->nargs++;
        }
      } else {
        fprintf(stderr, "Error: Expected command after pipe\n");
        exit(EXIT_FAILURE);
      }
      // parse(&temp->pipe_right_child, l);
      head = temp;
    } else if (r == TOKEN_REDIRECTION) {
      if (head->type == AST_COMMAND) {
        struct ast *temp = ast_new();
        temp->type = AST_REDIRECTION;
        temp->redir_type = t.value;
        temp->command = head;
        head = temp;
      } else {
        return 0; // ERROR
      }
    }
  }

  *out = head;

  return 1;
}
