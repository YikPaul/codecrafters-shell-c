#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
    fgets(command, sizeof(command), stdin);

    command[strlen(command) - 1] = '\0';
    if (strcmp(command, "exit") == 0) {
      break;
    } else if (strncmp(command, "echo ", 5) == 0) {
      printf("%s\n", command + 5);
      continue;
    } else if (strcmp(command, "cls") == 0 || strcmp(command, "clear") == 0) {
      printf("/033[H/033/2J");
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
      system(command);
    }
  }

  return 0;
}
