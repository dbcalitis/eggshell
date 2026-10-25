#include <errno.h>
#include <fcntl.h>
#include <locale.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

#include "common.h"
#include "memory.h"
#include "tokenizer.h"

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

char *egg_args[BUFF_SIZE];
Int egg_nargs = 0;

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

Int
main(
        void)
{
    setlocale(LC_ALL, "");
    atexit(disable_raw_mode);

    enable_raw_mode();

    curr_path = getcwd(NULL, 0);

    Node *current_history = NULL;
    atexit(clear_history);

    printf("\033[1;1H\033[2J"); // Clears the screen
    printf("%s\n\r > ", curr_path);
    fflush(stdout);

    Tokenizer tokenizer = Cast(Tokenizer){ 0 };
    reset_tokenizer(&tokenizer);

    Arena arena = Cast(Arena){ 0 };
    if (arena_init(&arena, 1024 * 1024) == -1)
    {
        return 1;
    }

    // Shell input loop
    char c = '\0';
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

            // TODO: daria: parsing, execution
            // Reset tokenizer
            reset_tokenizer(&tokenizer);
            tokenizer.input = Cast(const U8 *)&display_line[0];
            tokenizer.current_byte = Cast(const U8 *)&display_line[0];
            
            // DEBUG: daria: loop tokenize
            for (;;)
            {
                Token *current_token = Cast(Token *)arena_alloc(
                        &arena, sizeof(*current_token), _Alignof(Token));
                if (current_token == NULL)
                {
                    printf("TOKEN ALLOCATION = NULL\n\r");
                }

                Unused(tokenize(&tokenizer, current_token));
                // print token results
                printf("token: \"%.*s\" : %d\n\r",
                        current_token->string.length,
                        current_token->string.data,
                        current_token->type);

                if (current_token->type == TOKEN_ERROR)
                {
                    printf("TokenError\n\r");
                    break;
                }
                if (current_token->type == TOKEN_EOF)
                {
                    printf("TokenEOF\n\r");
                    break;
                }
            }

            arena_reset(&arena);
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

        U64 display_length = strlen(display_line);

        if (display_length + 1 < sizeof display_line)
        {
            display_line[display_length] = c;
            display_line[display_length + 1] = '\0';
        }
    }

    arena_destroy(&arena);

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
