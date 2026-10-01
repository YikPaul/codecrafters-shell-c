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
    } else if (strncmp(command, "cd ", 3) == 0) {
      chdir(command + 3);
      if (chdir(command + 3) != 0) {
        printf("cd: %s: no such file or directory\n", command + 3);
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
