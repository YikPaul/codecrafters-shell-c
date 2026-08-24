#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
  char command[1024];
  printf("Enter you command: \n");
  fgets(command, sizeof(command), stdin);
  printf("%s : command not found", command);
  setbuf(stdout, NULL);

  printf("$ ");

  return 0;
}
