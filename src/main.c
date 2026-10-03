/*
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

void locate_x(char *command) {

  char *PATH = strdup(getenv("PATH"));
  for (char *p = strtok(PATH, ":"); p != NULL; p = strtok(NULL, ":")) {
    char fp[1028];
    sprintf(fp, "%s/%s", p, command);
    if (access(fp, X_OK) == 0) {
      printf("%s is %s\n", command, fp);
      return;
    }
  }
  printf("%s: not found\n", command);
}
int main(int argc, char *argv[]) {

  setbuf(stdout, NULL);

  while (1) {

    char command[1024];
    printf("$ ");
    if (fgets(command, sizeof(command), stdin) == NULL) {
      break;
    }

    // Strip the trailing newline, if there is one
    if (strlen(command) > 0 && command[strlen(command) - 1] == '\n') {
      command[strlen(command) - 1] = '\0';
    }
    // Ignore empty lines
    if (strlen(command) == 0) {
      continue;
    }
    if (strcmp(command, "exit") == 0) {
      break;
    } else if (strncmp(command, "echo ", 5) == 0) {
      printf("%s\n", command + 5);
      continue;
    } else if (strcmp(command, "cls") == 0 || strcmp(command, "clear") == 0) {
      printf("\033[2J\033[H");
      continue;
    } else if (strcmp(command, "pwd") == 0) {
      char pwd[1024];
      if (getcwd(pwd, sizeof(pwd)) != NULL) {
        printf("%s\n", pwd);
      }
      continue;
    }

    else if (strncmp(command, "cd ", 3) == 0) {
      char *dir = command + 3;

      if (strcmp(dir, "~") == 0) {
        dir = getenv("HOME");
      }

      if (chdir(dir) != 0) {
        printf("cd: %s: No such file or directory\n", command + 3);
      }

      continue;
    }

    if (strncmp(command, "type ", 5) == 0) {
      if (strcmp(command + 5, "echo") == 0 ||
          strcmp(command + 5, "exit") == 0 ||
          strcmp(command + 5, "type") == 0 || strcmp(command + 5, "cls") == 0 ||
          strcmp(command + 5, "clear") == 0 ||
          strcmp(command + 5, "pwd") == 0) {
        printf("%s is a shell builtin\n", command + 5);
      } else {
        locate_x(command + 5);
      }
    } else {
      pid_t pid = fork();
      if (pid < 0) {
        perror("fork");
      } else if (pid == 0) {
        char *args[100];
        int i = 0;

        char *token = strtok(command, " ");

        // Nothing to run if the line was all whitespace
        if (token == NULL) {
          exit(0);
        }

        while (token != NULL && i < 99) {
          args[i++] = token;
          token = strtok(NULL, " ");
        }

        args[i] = NULL;

        execvp(args[0], args);

        printf("%s: command not found\n", args[0]);
        exit(127);

      } else if (pid > 0) {
        int status;
        waitpid(pid, &status, 0);
      }
    }
  }

  return 0;
}
*/

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

  for (int i = 0; command[i] != '\0'; i++) {

    char c = command[i];

    /*
     * SINGLE QUOTE
     *
     * ' changes between normal mode
     * and single-quote mode.
     */
    if (c == '\'') {

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
     * WHITESPACE OUTSIDE QUOTES
     *
     * A space separates arguments only
     * when we are NOT inside single quotes.
     */
    if (isspace((unsigned char)c) && !in_single_quotes) {

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

/*
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
 * echo 'hello    world'
 *
 * becomes:
 *
 * args[0] = "echo"
 * args[1] = "hello    world"
 */
int parse_command(char *command, char *args[]) {

  int argc = 0;

  int out = 0;
  int start = 0;

  int arg_started = 0;
  int in_single_quotes = 0;

  for (int i = 0; command[i] != '\0'; i++) {

    char c = command[i];

    /*
     * Single quote:
     *
     * Turn single-quote mode on/off.
     *
     * The quote itself is not copied
     * into the argument.
     */
    if (c == '\'') {

      in_single_quotes = !in_single_quotes;

      /*
       * This is important for empty quotes:
       *
       * ''
       *
       * Even though there is no character
       * inside, it still represents an argument.
       */
      arg_started = 1;
    }

    /*
     * Space outside quotes means:
     * "this argument is finished."
     */
    else if (isspace((unsigned char)c) && !in_single_quotes) {

      if (arg_started) {

        command[out++] = '\0';

        args[argc++] = command + start;

        arg_started = 0;
      }
    }

    /*
     * Normal character.
     */
    else {

      if (!arg_started) {
        start = out;
        arg_started = 1;
      }

      command[out++] = c;
    }
  }

  /*
   * Save the final argument.
   */
  if (arg_started) {

    command[out++] = '\0';

    args[argc++] = command + start;
  }

  /*
   * execvp() expects the array to end with NULL.
   */
  args[argc] = NULL;

  return argc;
}

int main(int argc, char *argv[]) {

  setbuf(stdout, NULL);

  while (1) {

    char command[1024];

    printf("$ ");

    if (fgets(command, sizeof(command), stdin) == NULL) {
      break;
    }

    /*
     * Remove trailing newline.
     */
    if (strlen(command) > 0 && command[strlen(command) - 1] == '\n') {

      command[strlen(command) - 1] = '\0';
    }

    /*
     * Ignore completely empty lines.
     */
    if (strlen(command) == 0) {
      continue;
    }

    /*
     * Parse the command into arguments.
     */
    char *args[100];

    int arg_count = parse_command(command, args);

    if (arg_count == 0) {
      continue;
    }

    /*
     * exit
     */
    if (strcmp(args[0], "exit") == 0) {
      break;
    }

    /*
     * echo
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
     * cls / clear
     */
    else if (strcmp(args[0], "cls") == 0 || strcmp(args[0], "clear") == 0) {

      printf("\033[2J\033[H");

      continue;
    }

    /*
     * pwd
     */
    else if (strcmp(args[0], "pwd") == 0) {

      char pwd[1024];

      if (getcwd(pwd, sizeof(pwd)) != NULL) {
        printf("%s\n", pwd);
      }

      continue;
    }

    /*
     * cd
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
     * type
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
     * External command.
     */
    else {

      pid_t pid = fork();

      if (pid < 0) {

        perror("fork");

      } else if (pid == 0) {

        /*
         * Run external command.
         *
         * args already contains:
         *
         * args[0] = command
         * args[1] = argument
         * args[2] = argument
         * ...
         * args[n] = NULL
         */
        execvp(args[0], args);

        /*
         * execvp only returns if something went wrong.
         */
        printf("%s: command not found\n", args[0]);

        exit(127);

      } else {

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
*/
