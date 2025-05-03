#include <ctype.h>
#include <termios.h>
#include <unistd.h>

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

struct command {
  char *args[MAX_ARGS];
  int nargs;
  char *input_file;
  char *output_file;
  int is_pipe;
  int in_bg;
};

struct command commands[20];
char *args[BUFF_SIZE]; // edit the amount later
int nargs = 0;

void disable_raw_mode();
void enable_raw_mode();

void add_history(const char *line);
int get_history(struct node **n, int dir);
void clear_history();

void preprocess_input(const char *input, char *output);
int tokenize(char **args, char *input);

int exec_from_path(char **args, int nargs);

int sh_exit(char **args, int nargs);
int sh_history(char **args, int nargs);
int sh_cd(char **args, int nargs);

int sh_num_builtins();

int sh_execute(char **args, int nargs);

char *builtin_str[] = {"cd", "history", "exit", "path"};
int (*builtin_func[])(char **, int) = {&sh_cd, &sh_history, &sh_exit,
                                       &exec_from_path};
void clear_args() {
  for (int i = 0; i < BUFF_SIZE; i++) {
    args[i] = NULL;
  }

  nargs = 0;
}

// TODO(daria): edit tokenize, split on ; && ||
// TODO(daria): pipes
// TODO(daria): redirection

int main() {
  setlocale(LC_ALL, "");

  // Clears the screen
  printf("\e[1;1H\e[2J");

  enable_raw_mode();

  char input[BUFF_SIZE] = "\0";
  char line[BUFF_SIZE] = "\0";
  int length = 0;
  clear_args();

  curr_path = getcwd(NULL, 0);

  struct node *current_history = NULL;
  atexit(clear_history);

  printf("%s\n\r > ", curr_path);

  // Shell input loop
  while (length != -1 && input[0] != 17) { // CTRLQ
    length = read(STDIN_FILENO, input, sizeof(input));

    // Ignore Esc characters; includes arrow keys
    if (input[0] == '\33') {
      continue;
    }

    if (input[0] == 127) { // BSPACE
      current_history = NULL;
      line[strlen(line) - 1] = '\0';
    } else if (input[0] == 13) { // ENTER
      current_history = NULL;
      nargs = tokenize(args, line);

      sh_execute(args, nargs);
      add_history(line);

      line[0] = '\0';
      clear_args();

      printf("\r\n\r%s\n\r", curr_path);
    } else if (input[0] == 11) { // CTRLK Go to older history
      if (get_history(&current_history, 1) == 1) {
        strcpy(line, current_history->line);
      }
    } else if (input[0] == 10) { // CTRLJ Go to newer history
      if (get_history(&current_history, 0)) {
        strcpy(line, current_history->line);
      }
    } else if (input[0] == 9) { // TAB for autocompletion
      // TODO(daria): make autocomplete with bash
    } else {
      if (strlen(line) + strlen(input) < BUFF_SIZE) {
        strcat(line, input);
      }
    }

    // clears current display line
    printf("\033[1G\033[2K");

    input[length] = '\0';
    printf("\r > %s", line);
    fflush(stdout);
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

void preprocess_input(const char *input, char *output) {
  int j = 0;

  for (int i = 0; input[i] != '\0'; i++) {
    if (input[i] == '<' || input[i] == '>' || input[i] == '|' ||
        input[i] == '&' || input[i] == ';') {
      output[j++] = ' ';
      output[j++] = input[i];

      // for append ">>" and "&&"
      if (input[i + 1] == input[i] && input[i] != '<') {
        output[j++] = input[i++];
      }

      output[j++] = ' ';
    } else {
      output[j++] = input[i];
    }
  }

  output[j] = '\0';
}

int tokenize(char **args, char *input) {
  char processed[BUFF_SIZE];
  preprocess_input(input, processed);

  char *token = strtok(input, " ");
  int n = 0;

  while (token != NULL && n < BUFF_SIZE) {
    args[n++] = token;
    token = strtok(NULL, " ");
  }

  /*
  int cmd_i = 0;
  int cmd_start = 0;

  for (int i = 0; i < n; i++) {
    if (strchr(args[i], '>')) {
      commands[cmd_i] = (struct command){.nargs = i - cmd_start,
                                         .input_file = NULL,
                                         .output_file = args[i + 1],
                                         .is_pipe = 0};

      printf("\n\r > : %s", args[i + 1]);

      for (int j = 0; j < i - cmd_start; j++) {
        commands[cmd_i].args[j] = args[cmd_start + j];
      }

      cmd_start = i + 1;
      cmd_i++;
    } else if (strchr(args[i], '<')) {
      commands[cmd_i] = (struct command){.nargs = i - cmd_start,
                                         .input_file = args[i + 1],
                                         .output_file = NULL,
                                         .is_pipe = 0};

      printf("\n\r < : %s", args[i + 1]);

      for (int j = 0; j < i - cmd_start; j++) {
        commands[cmd_i].args[j] = args[cmd_start + j];
      }

      cmd_start = i + 1;
      cmd_i++;
    } else if (strchr(args[i], '|')) {
      commands[cmd_i] = (struct command){
          .input_file = NULL, .output_file = NULL, .is_pipe = 1};

      cmd_start = i + 1;
      cmd_i++;
    } else if (strchr(args[i], '&')) {
      if (cmd_i == 0) {
        commands[cmd_i].in_bg = 1;
      } else {
        commands[cmd_i - 1].in_bg = 1;
      }
    }
  }
  */

  return n;
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

int sh_exit(char **args, int nargs) { exit(0); }

int sh_history(char **args, int nargs) {
  if (nargs == 1) {
    struct node *curr = history.head;
    printf("\r\n\nPast Inputs:\r");

    while (curr != NULL) {
      printf("\n- %s \r", curr->line);
      curr = curr->next;
    }
  } else if (nargs > 1) {
    if (strcmp(args[1], "clear") == 0) {
      clear_history();
    }
  }

  return 1;
}

int sh_cd(char **args, int nargs) {
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

int sh_num_builtins() { return sizeof(builtin_str) / sizeof(char *); }

int sh_execute(char **args, int nargs) {
  if (nargs == 0) {
    return 1;
  }

  int builtin = 0;

  for (int i = 0; i < sh_num_builtins(); i++) {
    if (strcmp(args[0], builtin_str[i]) == 0) {
      builtin = 1;
      return (*builtin_func[i])(args, nargs);
      printf("\n\r%s", args[0]);
    }
  }

  if (builtin != 1) {
    exec_from_path(args, nargs);
  }

  return 1;
}
