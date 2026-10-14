#define _POSIX_C_SOURCE 200809L
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

/*
 * Only use readline when its headers are actually available, so the
 * program still compiles on systems without the readline dev package.
 * CodeCrafters has readline, so completion is enabled there.
 */
#if defined(__has_include)
#if __has_include(<readline/readline.h>)
#define USE_READLINE
#endif
#endif

#ifdef USE_READLINE
#include <readline/readline.h>
#include <readline/history.h>

static char *completion_generator(const char *text, int state) {
  static int idx;
  static size_t len;
  const char *builtins[] = {"echo", "exit", NULL};
  if (state == 0) {
    idx = 0;
    len = strlen(text);
  }
  while (builtins[idx]) {
    const char *cmd = builtins[idx++];
    if (strncmp(cmd, text, len) == 0) {
      char *result = malloc(strlen(cmd) + 2);
      if (result == NULL) {
        return NULL;
      }
      strcpy(result, cmd);
      return result;
    }
  }
  return NULL;
}

static char **completion_callback(const char *text, int start, int end) {
  (void)start;
  (void)end;
  return rl_completion_matches(text, completion_generator);
}
#endif

void locate_x(char *command) {
  char *path_env = getenv("PATH");

  if (path_env == NULL) {
    printf("%s: not found\n", command);
    return;
  }

  char *PATH = strdup(path_env);

  for (char *p = strtok(PATH, ":"); p != NULL; p = strtok(NULL, ":")) {

    char fp[1028];

    sprintf(fp, "%s/%s", p, command);

    if (access(fp, X_OK) == 0) {
      printf("%s is %s\n", command, fp);
      free(PATH);
      return;
    }
  }

  free(PATH);

  printf("%s: not found\n", command);
}

/*
 * Return the canonical token for a redirection operator.
 * fd is 1 (stdout) or 2 (stderr); append selects >> over >.
 */
char *redirect_op(int fd, int append) {
  if (fd == 2) {
    return append ? "2>>" : "2>";
  }
  return append ? ">>" : ">";
}

/*
 * Parse the command line.
 * Example:
 *     echo 'hello world'
 * becomes:
 *     args[0] = "echo"
 *     args[1] = "hello world"
 *
 * Redirection operators are emitted as their own arguments
 * (">", "2>", ">>", "2>>", ...) so that main() can handle them.
 */
int parse_command(char *command, char *args[]) {
  int argc = 0;
  int out = 0;
  int start = 0;
  int arg_started = 0;
  int in_single_quotes = 0;
  int in_double_quotes = 0;
  for (int i = 0; command[i] != '\0'; i++) {
    char c = command[i];

    /*
     *this is for handling the backslash
     *this treats every character outside the qoutes literally
     inside the quotes ,the backslash is treated literally
     */
    if (c == '\\' && !in_single_quotes && !in_double_quotes) {
      if (!arg_started) {
        start = out;
        arg_started = 1;
      }
      i++;
      if (command[i] != '\0') {
        command[out++] = command[i];
      }
      continue;
    }
    /*
     *let us  now implemment the backslash inside the doble quotes
     *here only two functions are performed ie it only escapes the backslash
     * itself and the double quotes
     *
     *
     **/
    if (c == '\\' && in_double_quotes) {
      if (!arg_started) {
        start = out;
        arg_started = 1;
      }
      char next = command[i + 1];
      if (next == '"' || next == '\\') {
        command[out++] = next;
        i++;
      } else {

        command[out++] = c;
      }
      continue;
    }

    /*
     * SINGLE QUOTE
     * ' changes between normal mode
     * and single-quote mode.
     */
    if (c == '\'' && !in_double_quotes) {
      /*
       * If this is the first thing in an
       * argument, remember where the
       * argument starts.
       */
      if (!arg_started) {
        start = out;
        arg_started = 1;
      }

      in_single_quotes = !in_single_quotes;

      /*
       * Don't copy the quote itself.
       */
      continue;
    }
    /*
     *handling of the double quotes
     */
    if (c == '"' && !in_single_quotes) {
      if (!arg_started) {
        start = out;
        arg_started = 1;
      }
      in_double_quotes = !in_double_quotes;
      continue;
    }

    /*
     * WHITESPACE OUTSIDE QUOTES
     * A space separates arguments only
     * when we are NOT inside single quotes.
     */
    if (isspace((unsigned char)c) && !in_single_quotes && !in_double_quotes) {
      if (arg_started) {
        command[out++] = '\0';
        args[argc++] = command + start;
        arg_started = 0;
      }
      continue;
    }

    /*
     * NORMAL CHARACTER
     */
    if (!arg_started) {
      start = out;
      arg_started = 1;
    }
    command[out++] = c;
  }

  /*
   * Save the final argument.
   */
  if (arg_started) {
    command[out++] = '\0';
    args[argc++] = command + start;
  }

  /*
   * execvp() requires NULL at the end.
   */
  args[argc] = NULL;
  return argc;
}

/*
 * =========================
 * REDIRECTION HANDLING
 * =========================
 */
static int saved_stdout = -1;
static int saved_stderr = -1;

static int redirect_to(const char *path, int fd, int append) {
  int flags = O_WRONLY | O_CREAT | (append ? O_APPEND : O_TRUNC);
  int newfd = open(path, flags, 0644);

  if (newfd == -1) {
    perror("open");
    return -1;
  }

  int saved = dup(fd);

  if (saved == -1) {
    perror("dup");
    close(newfd);
    return -1;
  }

  if (dup2(newfd, fd) == -1) {
    perror("dup2");
    close(newfd);
    close(saved);
    return -1;
  }

  close(newfd);

  if (fd == STDOUT_FILENO) {
    saved_stdout = saved;
  } else if (fd == STDERR_FILENO) {
    saved_stderr = saved;
  }

  return 0;
}

/*
 * Apply the requested redirections to the shell process itself.
 * They are undone later by restore_redirections(), which lets
 * builtins keep writing to the terminal once they're done.
 */
int apply_redirections(char *out_file, int out_append, char *err_file,
                       int err_append) {
  if (out_file != NULL) {
    redirect_to(out_file, STDOUT_FILENO, out_append);
  }
  if (err_file != NULL) {
    redirect_to(err_file, STDERR_FILENO, err_append);
  }
  return (out_file != NULL || err_file != NULL) ? 1 : 0;
}

void restore_redirections(void) {
  if (saved_stdout != -1) {
    dup2(saved_stdout, STDOUT_FILENO);
    close(saved_stdout);
    saved_stdout = -1;
  }
  if (saved_stderr != -1) {
    dup2(saved_stderr, STDERR_FILENO);
    close(saved_stderr);
    saved_stderr = -1;
  }
}

int main(void) {

  setbuf(stdout, NULL);

  while (1) {

    char *line;

#ifdef USE_READLINE
    rl_attempted_completion_function = completion_callback;
    line = readline("$ ");
#else
    char command_buf[1024];
    printf("$ ");
    if (fgets(command_buf, sizeof(command_buf), stdin) == NULL) {
      break;
    }
    size_t len = strlen(command_buf);
    if (len > 0 && command_buf[len - 1] == '\n') {
      command_buf[len - 1] = '\0';
    }
    line = command_buf;
#endif

    char command[1024];
    if (line == NULL) {
      break;
    }
    strncpy(command, line, sizeof(command) - 1);
    command[sizeof(command) - 1] = '\0';
#ifdef USE_READLINE
    free(line);
#endif

    /*
     * Ignore empty input.
     */
    if (strlen(command) == 0) {
      continue;
    }

    /*
     * Parse command.
     */
    char *args[100];

    int arg_count = parse_command(command, args);

    if (arg_count == 0) {
      continue;
    }

    /*
     * =========================
     * EXTRACT REDIRECTIONS
     * =========================
     * Walk the arguments, pull out any redirection operators
     * (`>`, `>>`, `1>`, `1>>`, `2>`, `2>>`) in both space and
     * attached forms (e.g. `2>/file`).
     */
    char *out_file = NULL;
    char *err_file = NULL;
    int out_append = 0;
    int err_append = 0;
    int w = 0;

    for (int i = 0; i < arg_count; i++) {
      char *a = args[i];

      /* Check for attached form like "2>/file", "2>>file", ">file", ">>file" */
      char *gt = strchr(a, '>');
      if (gt != NULL && gt != a && (gt[-1] == '1' || gt[-1] == '2')) {
        int is_stderr = (gt[-1] == '2');
        int append = (gt[1] == '>');
        if (append) {
          /* filename starts after >> */
          if (gt[2] != '\0') {
            char *fname = gt + 2;
            if (is_stderr) {
              err_file = fname;
              err_append = 1;
            } else {
              out_file = fname;
              out_append = 1;
            }
          } else if (i + 1 < arg_count) {
            /* space after >> */
            if (is_stderr) {
              err_file = args[i + 1];
              err_append = 1;
            } else {
              out_file = args[i + 1];
              out_append = 1;
            }
            i++;
          }
        } else {
          if (gt[1] != '\0') {
            char *fname = gt + 1;
            if (is_stderr) {
              err_file = fname;
              err_append = 0;
            } else {
              out_file = fname;
              out_append = 0;
            }
          } else if (i + 1 < arg_count) {
            if (is_stderr) {
              err_file = args[i + 1];
              err_append = 0;
            } else {
              out_file = args[i + 1];
              out_append = 0;
            }
            i++;
          }
        }
        continue;
      } else if (gt != NULL && gt == a) {
        int append = (gt[1] == '>');
        if (append) {
          if (gt[2] != '\0') {
            out_file = gt + 2;
            out_append = 1;
          } else if (i + 1 < arg_count) {
            out_file = args[i + 1];
            out_append = 1;
            i++;
          }
        } else {
          if (gt[1] != '\0') {
            out_file = gt + 1;
            out_append = 0;
          } else if (i + 1 < arg_count) {
            out_file = args[i + 1];
            out_append = 0;
            i++;
          }
        }
        continue;
      }

      int is_redir = strcmp(a, ">") == 0 || strcmp(a, "1>") == 0 ||
                     strcmp(a, "2>") == 0 || strcmp(a, ">>") == 0 ||
                     strcmp(a, "1>>") == 0 || strcmp(a, "2>>") == 0;

      if (is_redir) {
        if (i + 1 >= arg_count) {
          break; /* dangling operator, ignore */
        }

        int is_stderr = (a[0] == '2');
        int append = (strstr(a, ">>") != NULL);

        if (is_stderr) {
          err_file = args[i + 1];
          err_append = append;
        } else {
          out_file = args[i + 1];
          out_append = append;
        }

        i++; /* skip the filename */
        continue;
      }

      args[w++] = a;
    }

    args[w] = NULL;
    arg_count = w;

    if (arg_count == 0) {
      continue;
    }

    /*
     * Apply redirections once for every kind of command.
     * Builtins write through the shell's own file descriptors,
     * so the same mechanism works for them too.
     */
    int applied =
        apply_redirections(out_file, out_append, err_file, err_append);

    /*
     * =========================
     * EXIT
     * =========================
     */
    if (strcmp(args[0], "exit") == 0) {
      if (applied) {
        restore_redirections();
      }
      break;
    }

    /*
     * =========================
     * ECHO
     * =========================
     */
    else if (strcmp(args[0], "echo") == 0) {

      for (int i = 1; i < arg_count; i++) {

        if (i > 1) {
          printf(" ");
        }

        printf("%s", args[i]);
      }

      printf("\n");
    }

    /*
     * =========================
     * CLEAR / CLS
     * =========================
     */
    else if (strcmp(args[0], "cls") == 0 || strcmp(args[0], "clear") == 0) {

      printf("\033[2J\033[H");
    }

    /*
     * =========================
     * PWD
     * =========================
     */
    else if (strcmp(args[0], "pwd") == 0) {

      char pwd[1024];

      if (getcwd(pwd, sizeof(pwd)) != NULL) {
        printf("%s\n", pwd);
      }
    }

    /*
     * =========================
     * CD
     * =========================
     */
    else if (strcmp(args[0], "cd") == 0) {

      if (arg_count < 2) {
        /* nothing to do */
      } else {
        char *dir = args[1];

        /*
         * cd ~
         */
        if (strcmp(dir, "~") == 0) {
          dir = getenv("HOME");
        }

        if (chdir(dir) != 0) {

          printf("cd: %s: No such file or directory\n", args[1]);
        }
      }
    }

    /*
     * =========================
     * TYPE
     * =========================
     */
    else if (strcmp(args[0], "type") == 0) {

      if (arg_count >= 2) {
        if (strcmp(args[1], "echo") == 0 || strcmp(args[1], "exit") == 0 ||
            strcmp(args[1], "type") == 0 || strcmp(args[1], "cls") == 0 ||
            strcmp(args[1], "clear") == 0 || strcmp(args[1], "pwd") == 0) {

          printf("%s is a shell builtin\n", args[1]);

        } else {

          locate_x(args[1]);
        }
      }
    }

    /*
     * =========================
     * EXTERNAL COMMAND
     * =========================
     */
    else {

      pid_t pid = fork();

      if (pid < 0) {

        perror("fork");

      }

      else if (pid == 0) {
        /*
         * Redirections were already applied to this process by
         * the parent, so the child inherits them across execvp().
         *
         * Run external program.
         */
        execvp(args[0], args);

        /*
         * execvp() only gets here
         * if the command failed.
         */
        printf("%s: command not found\n", args[0]);

        exit(127);
      }

      else {

        /*
         * Parent waits for child.
         */
        int status;

        waitpid(pid, &status, 0);
      }
    }

    /*
     * Undo redirections so the next prompt and builtins
     * write to the terminal again.
     */
    if (applied) {
      restore_redirections();
    }
  }

  return 0;
}
