#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[]) {
  char command[1024];
  printf("$ ");
  // printf("Enter you command: \n");
  fgets(command, sizeof(command), stdin);

  command[strlen(command) - 1] = '\0';

  printf("%s: command not found\n", command);
  setbuf(stdout, NULL);

  return 0;
}
