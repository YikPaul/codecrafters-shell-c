#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[]) {
  setbuf(stdout, NULL);

  while (1) {

    char command[1024];
    printf("$ ");
    // printf("Enter you command: \n");
    fgets(command, sizeof(command), stdin);

    command[strlen(command) - 1] = '\0';
    if (strcmp(command, "exit") == 0) {
      break;
    }

    printf("%s: command not found\n", command);
  }

  return 0;
}
