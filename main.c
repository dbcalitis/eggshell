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

#include <stdint.h>
#include <errno.h>

#define Int int
#define S64 ssize_t
#define U8 uint8_t
#define U16 uint16_t
#define U32 uint32_t
#define U64 uint64_t

#define Cast(x) (x)
#define Unused(x) (void)(x)

#define static_arr_size(arr) (sizeof(arr) / sizeof(*(arr)))

#define BUFF_SIZE 100
#define MAX_ARGS 20

// To reset from terminal raw mode
struct termios orig_termios;

typedef struct node Node;
struct node
{
  struct node *next;
  struct node *prev;
  char line[BUFF_SIZE];
};

typedef struct linked_list Linked_List;
struct linked_list
{
  Node *head;
  Node *tail;
  Int length;
};

Linked_List history = {.head = NULL, .tail = NULL, .length = 0};

char *curr_path = NULL;

char *egg_args[BUFF_SIZE]; // edit the amount later
Int egg_nargs = 0;

enum token_type
{
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

enum ast_type
{
    AST_COMMAND,
    AST_STRING_LITERAL,
    AST_PIPE,
    AST_REDIRECTION,
    AST_CAPTURE_STRING // $()
};

typedef struct ast AST;
struct ast
{
    U8 type;

    union
    {
        struct
        {
            char *command_name;
            AST **args;
            Int nargs;
        };

        struct
        {
            AST *pipe_left_child;
            AST *pipe_right_child;
        };

        struct
        {
            char *redir_type;
            AST *command;
            char *target;
        };

        // edit later
        struct
        {
            char *string_literal;
        };
    };
};

typedef struct token Token;
struct token
{
  enum token_type type;
  char *value;
};

AST *
ast_new(void)
{
  AST *tree = Cast(AST *)calloc(1, sizeof *tree);
  if (tree == NULL)
  {
      perror("calloc");
      exit(EXIT_FAILURE);
  }

  return tree;
}

void
ast_free(AST *node)
{
    if (node == NULL) { return; }

    switch ((enum ast_type) node->type)
    {
        case AST_COMMAND:
        {
            free(node->command_name);

            for (Int i = 0; i < node->nargs; i++)
            {
                ast_free(node->args[i]);
            }

            free(node->args);
            break;
        }
        case AST_STRING_LITERAL:
        {
            free(node->string_literal);
            break;
        }
        case AST_PIPE:
        {
            ast_free(node->pipe_left_child);
            ast_free(node->pipe_right_child);
            break;
        }
        case AST_REDIRECTION:
        {
            free(node->redir_type);
            free(node->target);
            ast_free(node->command);
            break;
        }
        case AST_CAPTURE_STRING:
        {
            free(node->string_literal);
            break;
        }
        default:
        {
            break;
        }
    }

    free(node);
}

char input_buffer[BUFF_SIZE] = "\0";
char display_line[BUFF_SIZE] = "\0";

void disable_raw_mode(void);
void enable_raw_mode(void);

void add_history(const char *line);
Int get_history(Node **n, Int dir);
void clear_history(void);

Int exec_from_path(char **args /*, Int nargs*/);

Int egg_exit(char **args, Int nargs) { return 0; }
Int egg_history(char **args, Int nargs);
Int egg_cd(char **args, Int nargs);

Int egg_num_builtins(void);

Int egg_execute_cmd(AST *head);

static char *builtin_str[] = {
    "cd",
    "history",
    "exit"
};

static Int (*builtin_func[])(char **, Int) = {
    egg_cd,
    egg_history,
    egg_exit
};

enum token_type lex(Token *t, const char **line);
Int parse(AST **out, const char *line);
// PERF(daria): memory leaks from ast

Int
main(
        void)
{
    setlocale(LC_ALL, "");
    atexit(disable_raw_mode);

    // Clears the screen
    printf("\033[1;1H\033[2J");

    enable_raw_mode();

    S64 length = 0;

    curr_path = getcwd(NULL, 0);

    Node *current_history = NULL;
    atexit(clear_history);

    printf("%s\n\r > ", curr_path);
    fflush(stdout);

    // Shell input loop
    char c = '\0';

    printf("\033[1;1H\033[2J");
    printf("%s\n\r > ", curr_path);
    fflush(stdout);

    for (;;)
    {
        // Redraw the current input line.
        // Raw mode disabled terminal echo, so we print it ourselves
        printf("\r\033[2K > %s", display_line);
        fflush(stdout);
        
        S64 result = read(STDIN_FILENO, &c, 1);

        if (result == 0) break;

        if (result < 0)
        {
            if (errno == EINTR) { continue; }
            
            perror("read");
            break;
        }

        // CTRL-Q
        if (c == 17) break;

        if (c == '\033')
        {
            char sequence = '\0';

            if (read(STDIN_FILENO, &sequence, 1) == 1
                    && sequence == '[')
            {
                read(STDIN_FILENO, &sequence, 1);
            }

            continue;
        }

        // Backspace
        if (c == 127)
        {
            current_history = NULL;

            U64 display_length = strlen(display_line);
            if (display_length > 0) { display_line[display_length - 1] = '\0'; }

            continue;
        }

        // Enter
        if (c == '\r')
        {
            current_history = NULL;

            if (display_line[0] != '\0') { add_history(display_line); }

            printf("\r\n");
            

            // TODO: daria: lexing, parsing, execution

            display_line[0] = '\0';
            printf("%s\n\r > ", curr_path);
            fflush(stdout);

            continue;
        }

        // CTRL-K - go to older history
        if (c == 11)
        {
            if (get_history(&current_history, 1))
            {
                snprintf(
                        display_line,
                        sizeof display_line,
                        "%s",
                        current_history->line);
            }
            continue;
        }

        // CTRL-J - go to newer history
        if (c == 10)
        {
            if (get_history(&current_history, 0))
            {
                snprintf(
                        display_line,
                        sizeof display_line,
                        "%s",
                        current_history->line);
            }
            continue;
        }

        // TAB - TODO: autocompletion
        if (c == '\t') { continue; }

        U64 length = strlen(display_line);

        if (length + 1 < sizeof display_line)
        {
            display_line[length] = c;
            display_line[length + 1] = '\0';
        }
    }

    return 0;
}

void
disable_raw_mode()
{
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

void
enable_raw_mode()
{
    tcgetattr(STDIN_FILENO, &orig_termios);

    // disables echo and canonical mode
    struct termios raw = orig_termios;
    raw.c_iflag &= Cast(tcflag_t) ~(IGNBRK | PARMRK | IGNCR | BRKINT | INPCK | ISTRIP | ICRNL | IXON);
    raw.c_oflag &= Cast(tcflag_t) ~(OPOST);
    raw.c_lflag &= Cast(tcflag_t) ~(ECHO | ECHONL | IEXTEN | ICANON | ISIG);
    raw.c_cflag |= (CS8);

    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;

    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

void add_history(const char *line)
{
    Node *n = (Node *)malloc(sizeof(Node));
    n->prev = NULL;
    n->next = NULL;
    strcpy(n->line, line);

    if (history.length == 0)
    {
        history.head = n;
        history.tail = n;
    }
    else
    {
        history.tail->next = n;
        n->prev = history.tail;
        history.tail = n;

        if (history.length == 5)
        {
            Node *temp = history.head;
            history.head = history.head->next;
            free(temp);
            history.head->prev = NULL;
            history.length--;
        }
    }

    history.length++;
}

Int
get_history(Node **n, Int dir)
{
    if (dir == 1) // go to older history
    {
        if (*n == NULL) {
            if (history.tail != NULL)
            {
                *n = history.tail;
                return 1;
            }
        }
        else if ((*n)->prev != NULL)
        {
            *n = (*n)->prev;
            return 1;
        }
    }
    else // go to newer history
    {
        if (*n != NULL)
        {
            if ((*n)->next != NULL)
            {
                *n = (*n)->next;
                return 1;
            }
        }
    }
    return 0;
}

void
clear_history()
{
    Node *temp = history.head;
    while (history.head != NULL)
    {
        temp = history.head;
        history.head = temp->next;
        free(temp);
    }

    history.length = 0;
}

Int
exec_from_path(
        char **args
        /*Int nargs*/)
{
    printf("\r\n\r");

    disable_raw_mode();
    Int pid = fork();

    if (pid == 0)
    {
        Unused(execvp(args[0], args));
        perror("execvp");
        exit(0);
    }
    else
    {
        Unused(wait(NULL));
    }

    enable_raw_mode();

    return 1;
}

Int
egg_history(
        char **args,
        Int nargs)
{
    if (nargs == 1)
    {
        Node *curr = history.head;
        printf("\r\n\nPast Inputs:");
        while (curr != NULL) {
            printf("\n\r- %s \r", curr->line);
            curr = curr->next;
        }
    }
    else if (nargs > 1)
    {
        if (strcmp(args[1], "clear") == 0)
        {
            clear_history();
        }
    }

    return 1;
}

Int
egg_cd(
        char **args,
        Int nargs)
{
    Int result;
    if (nargs == 1)
    {
        result = chdir(getenv("HOME"));
    }
    else
    {
        result = chdir(args[1]);
    }

    if (result == -1)
    {
        printf("\r\n");
        perror("chdir");
    }
    else
    {
        curr_path = getcwd(NULL, 0);
    }

    return 1;
}

Int
egg_num_builtins() 
{ 
    return sizeof(builtin_str) / sizeof(char *);
}

Int
egg_execute_cmd(AST *head)
{
    switch (head->type)
    {
        case AST_COMMAND: {
            char *args[head->nargs + 2];
            args[0] = head->command_name;
            size_t nargs = 1;

            args[0] = head->command_name;

            for (Int i = 0; i < head->nargs; i++)
            {
                if (head->args[i]->type == AST_STRING_LITERAL)
                {
                args[nargs++] = head->args[i]->string_literal;
                }
            }

            args[nargs] = NULL;

            // TODO(daria): add builtins
            printf("\n\r");
            pid_t pid = fork();

            if (pid == 0)
            {
                execvp(head->command_name, args); // fix
                perror("execvp");
                exit(EXIT_FAILURE);
            }
            else if (pid > 0)
            {
                Int status;
                waitpid(pid, &status, 0);

                if (WIFEXITED(status))
                {
                    printf("\n\rChild exited w/ status: %d", WIFEXITED(status));
                }
            }
            else
            {
              perror("fork");
            }
            break;
        }
        case AST_REDIRECTION: {
            if (strcmp(head->redir_type, ">") == 0)
            {
                Int og_stdout = dup(STDOUT_FILENO);
                Int fd = open(head->target, O_WRONLY | O_CREAT | O_TRUNC, 0644);

                if (fd < 0)
                {
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
            }
            else if (strcmp(head->redir_type, "<") == 0)
            {
                Int og_stdin = dup(STDIN_FILENO);
                Int fd = open(head->target, O_RDONLY);

                if (fd < 0)
                {
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
            break;
        }
        case AST_PIPE: {
            Int pipefd[2];
            pipe(pipefd);

            pid_t pid1 = fork();

            if (pid1 == 0)
            {
                close(pipefd[0]);
                dup2(pipefd[1], STDOUT_FILENO);
                close(pipefd[1]);
                egg_execute_cmd(head->pipe_left_child);
                exit(EXIT_SUCCESS);
            }

            pid_t pid2 = fork();
            if (pid2 == 0)
            {
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
            break;
        }
    }

  return 1;
}

enum token_type
lex(
        Token *t,
        const char **line)
{
    const char **s = line;

    // ignore whitespace
    while (isspace(**line)) { (*line)++; }

    if (**s == '\0')
    {
        *t = (Token){.type = TOKEN_EOF, .value = "\0"};
        return TOKEN_EOF;
    }
    else if (**s == '>')
    {
        *t = (Token){.type = TOKEN_REDIRECTION};
        t->value = (char *)calloc(sizeof(char), 2);
        t->value[0] = '>';
        t->value[1] = '\0';
        (*line)++;
        return TOKEN_REDIRECTION;
    }
    else if (**s == '<')
    {
        *t = (Token){.type = TOKEN_REDIRECTION};
        t->value = (char *)calloc(sizeof(char), 2);
        t->value[0] = '<';
        t->value[1] = '\0';
        (*line)++;
        return TOKEN_REDIRECTION;
    }
    else if (**s == '|')
    {
        *t = (Token){.type = TOKEN_PIPE};
        t->value = (char *)calloc(sizeof(char), 2);
        t->value[0] = '|';
        t->value[1] = '\0';
        (*line)++;
        return TOKEN_PIPE;
    }
    else if (**s == '(') 
    {
        *t = (Token){.type = TOKEN_LPAREN};
        t->value = (char *)calloc(sizeof(char), 2);
        t->value[0] = '(';
        t->value[1] = '\0';
        (*line)++;
        return TOKEN_LPAREN;
    }
    else if (**s == ')')
    {
        *t = (Token){.type = TOKEN_RPAREN};
        t->value = (char *)calloc(sizeof(char), 2);
        t->value[0] = ')';
        t->value[1] = '\0';
        (*line)++;
        return TOKEN_RPAREN;
    }
    else if (**s == '&')
    {
        *t = (Token){.type = TOKEN_BACKGROUND};
        t->value = (char *)calloc(sizeof(char), 2);
        t->value[0] = '&';
        t->value[1] = '\0';
        (*line)++;
        return TOKEN_BACKGROUND;
    }
    else if (**s == '$')
    {
        *t = (Token){.type = TOKEN_ENV_VAR,
            .value = calloc(sizeof(char), 10)};

        size_t len = strlen(t->value);
        size_t size = 10;

        // add parentheses
        while (isalpha(**s))
        {
            if (len + 1 > size - 1)
            {
                size += 5;
                t->value = realloc(t->value, size);
                t->value[size - 1] = '\0';
            }

            t->value[len++] = **s;
            (*s)++;
        }

        return TOKEN_ENV_VAR;
    }
    else
    {
        U64 length = 0;
        U64 capacity = 16;
        
        char *value = malloc(capacity);

        if (value == NULL)
        {
            t->type = TOKEN_ERROR;
            t->value = NULL;
            return TOKEN_ERROR;
        }

        while (**line != '\0'
                && !isspace(Cast(unsigned char)**line)
                && **line != '|'
                && **line != '<'
                && **line != '>')
        {
            if (length + 1 >= capacity)
            {
                capacity *= 2;

                char *temp = realloc(value, capacity);

                if (temp == NULL)
                {
                    free(value);
                    t->type = TOKEN_ERROR;
                    t->value = NULL;
                    return TOKEN_ERROR;
                }

                value = temp;
            }

            value[length++] = *(*line)++;
        }

        value[length] = '\0';
        
        t->type = TOKEN_STRING;
        t->value = value;

        return TOKEN_STRING;
    }

    // error cannot identify token
    return TOKEN_EOF;
}

Int
parse(
        AST **out,
        const char *line)
{
    AST *head = ast_new();
    size_t isfirst = 1;

    const char *l = line;
    Token t;
    enum token_type r;
    U64 size = 3; // size of ast.args

    while ((r = lex(&t, &l)) != TOKEN_EOF)
    {
        if (isfirst && r == TOKEN_STRING)
        {
            isfirst = 0;
            head->type = AST_COMMAND;
            head->command_name = t.value;
            head->args = (AST **)calloc(sizeof(AST *), size);
            head->nargs = 0;
        }
        else if (!isfirst && r == TOKEN_STRING)
        {
            if (head->type == AST_REDIRECTION)
            {
                head->target = t.value;
            }
            else
            {
                if (Cast(U64)(head->nargs + 1) > size)
                {
                    size += 2;
                    head->args = realloc(head->args, size);
                }
                head->args[head->nargs] = ast_new();
                head->args[head->nargs]->type = AST_STRING_LITERAL;
                head->args[head->nargs]->string_literal = t.value;
                head->nargs++;
            }
        }
        else if (r == TOKEN_PIPE)
        {
            AST *temp = ast_new();
            temp->type = AST_PIPE;
            temp->pipe_left_child = head;

            // printf("\r\n\nParsing right child");
            temp->pipe_right_child = ast_new();

            r = lex(&t, &l);

            if (r == TOKEN_STRING)
            {
                temp->pipe_right_child->type = AST_COMMAND;
                temp->pipe_right_child->command_name = t.value;

                U64 right_size = 3;
                temp->pipe_right_child->args = (AST **)calloc(sizeof(AST *), right_size);
                temp->pipe_right_child->nargs = 0;

                // HACK(daria): pipe right child should be done with recursion
                while ((r = lex(&t, &l)) != TOKEN_EOF && r == TOKEN_STRING)
                {
                    if (Cast(U64)(temp->pipe_right_child->nargs + 1) > size)
                    {
                        right_size += 2;
                        temp->pipe_right_child->args = realloc(temp->pipe_right_child->args, right_size);
                    }
                    temp->pipe_right_child->args[temp->pipe_right_child->nargs] = ast_new();
                    temp->pipe_right_child->args[temp->pipe_right_child->nargs]->type = AST_STRING_LITERAL;
                    temp->pipe_right_child->args[temp->pipe_right_child->nargs]->string_literal = t.value;
                    temp->pipe_right_child->nargs++;
                }
            }
            else
            {
                fprintf(stderr, "Error: Expected command after pipe\n");
                exit(EXIT_FAILURE);
            }

            // parse(&temp->pipe_right_child, l);
            head = temp;
        }
        else if (r == TOKEN_REDIRECTION)
        {
            if (head->type == AST_COMMAND)
            {
                AST *temp = ast_new();
                temp->type = AST_REDIRECTION;
                temp->redir_type = t.value;
                temp->command = head;
                head = temp;
            }
            else { return 0; } // ERROR 
        }
    }

    *out = head;

    return 1;
}
