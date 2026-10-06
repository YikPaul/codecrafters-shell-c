#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

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
 * Parse the command line.
 *
 * Example:
 *
 *     echo 'hello world'
 *
 * becomes:
 *
 *     args[0] = "echo"
 *     args[1] = "hello world"
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
     * SINGLE QUOTE
     *
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
     *
     *
     *
     * */
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
     *
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

int main(int argc, char *argv[]) {

  setbuf(stdout, NULL);

  while (1) {

    char command[1024];

    /*
     * Print shell prompt.
     */
    printf("$ ");

    /*
     * Read command.
     */
    if (fgets(command, sizeof(command), stdin) == NULL) {
      break;
    }

    /*
     * Remove newline.
     */
    size_t len = strlen(command);

    if (len > 0 && command[len - 1] == '\n') {
      command[len - 1] = '\0';
    }

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
     * EXIT
     * =========================
     */
    if (strcmp(args[0], "exit") == 0) {
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

      continue;
    }

    /*
     * =========================
     * CLEAR / CLS
     * =========================
     */
    else if (strcmp(args[0], "cls") == 0 || strcmp(args[0], "clear") == 0) {

      printf("\033[2J\033[H");

      continue;
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

      continue;
    }

    /*
     * =========================
     * CD
     * =========================
     */
    else if (strcmp(args[0], "cd") == 0) {

      if (arg_count < 2) {
        continue;
      }

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

      continue;
    }

    /*
     * =========================
     * TYPE
     * =========================
     */
    else if (strcmp(args[0], "type") == 0) {

      if (arg_count < 2) {
        continue;
      }

      if (strcmp(args[1], "echo") == 0 || strcmp(args[1], "exit") == 0 ||
          strcmp(args[1], "type") == 0 || strcmp(args[1], "cls") == 0 ||
          strcmp(args[1], "clear") == 0 || strcmp(args[1], "pwd") == 0) {

        printf("%s is a shell builtin\n", args[1]);

      } else {

        locate_x(args[1]);
      }

      continue;
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
  }

  return 0;
}
