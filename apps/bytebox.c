#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX_CMD_LEN 256
#define MAX_ARGS 16

void print_prompt()
{
  printf("ByteBox> ");
  fflush(stdout);
}

int main()
{
  char  cmd[MAX_CMD_LEN];
  char* args[MAX_ARGS];
  int   arg_count;

  while (1)
  {
    print_prompt();

    // Read command line
    if (fgets(cmd, sizeof(cmd), stdin) == NULL)
    {
      break;
    }

    // Remove newline
    cmd[strcspn(cmd, "\n")] = 0;

    // Parse command
    arg_count       = 0;
    args[arg_count] = strtok(cmd, " ");
    while (args[arg_count] != NULL && arg_count < MAX_ARGS - 1)
    {
      arg_count++;
      args[arg_count] = strtok(NULL, " ");
    }
    args[arg_count] = NULL;

    if (arg_count == 0)
      continue;

    // Handle commands
    if (strcmp(args[0], "exit") == 0)
    {
      break;
    }
    else if (strcmp(args[0], "echo") == 0)
    {
      for (int i = 1; i < arg_count; i++)
      {
        printf("%s ", args[i]);
      }
      printf("\n");
    }
    else if (strcmp(args[0], "help") == 0)
    {
      printf("Available commands:\n");
      printf("  echo <text>  - Print text\n");
      printf("  help         - Show this help\n");
      printf("  exit         - Exit shell\n");
    }
    else
    {
      printf("Unknown command: %s\n", args[0]);
    }
  }

  printf("Goodbye!\n");
  return 0;
}