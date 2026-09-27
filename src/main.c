#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void locate_x(char *name) {

  char *path_env = getenv("PATH");
  if (path_env == NULL) {
    printf("%s: not found\n", name);
    return;
  }

  char *PATH = strdup(path_env);
  for (char *p = strtok(PATH, ":"); p != NULL; p = strtok(NULL, ":")) {
    char fp[1024];
    snprintf(fp, sizeof(fp), "%s/%s", p, name);
    if (access(fp, X_OK) == 0) {
      printf("%s is %s\n", name, fp);
      free(PATH);
      return;
    }
  }
  free(PATH);
  printf("%s: not found\n", name);
}

int main(int argc, char *argv[]) {

  setbuf(stdout, NULL);

  while (1) {

    char command[1024];
    printf("$ ");
    if (fgets(command, sizeof(command), stdin) == NULL) {
      break;
    }

    command[strcspn(command, "\n")] = '\0';
    if (strcmp(command, "exit") == 0) {
      break;
    } else if (strncmp(command, "echo ", 5) == 0) {
      printf("%s\n", command + 5);
      continue;
    } else if (strcmp(command, "cls") == 0 || strcmp(command, "clear") == 0) {
      printf("\033[H\033[2J");
    }
    if (strncmp(command, "type ", 5) == 0) {
      if (strcmp(command + 5, "echo") == 0 ||
          strcmp(command + 5, "exit") == 0 ||
          strcmp(command + 5, "type") == 0 || strcmp(command + 5, "cls") == 0 ||
          strcmp(command + 5, "clear") == 0) {
        printf("%s is a shell builtin\n", command + 5);
      } else {
        locate_x(command + 5);
      }
    } else {
      printf("%s: command not found\n", command);
    }
  }

  return 0;
}
