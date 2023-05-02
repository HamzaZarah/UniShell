#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>
#include <time.h>

#define MAX_CMD_LENGTH 1024
#define CMD_DELIMITER " \t\r\n\a"
#define POMODORO_TIME 2 * 60
#define MOVE_REMINDER_TIME 4 * 60
#define WATER_REMINDER_TIME 6 * 60

typedef enum
{
  POMODORO,
  MOVE,
  WATER
} reminder_type;
struct reminder
{
  time_t last_time;
  int interval;
  char *message;
  reminder_type type;
};

int pomodoro_running = 0;

char **parse_command(char *line)
{
  int bufsize = 64;
  int position = 0;
  char **tokens = malloc(bufsize * sizeof(char *));
  char *token;

  if (!tokens)
  {
    fprintf(stderr, "Allocation error\n");
    exit(EXIT_FAILURE);
  }

  token = strtok(line, CMD_DELIMITER);
  while (token != NULL)
  {
    tokens[position] = token;
    position++;

    if (position >= bufsize)
    {
      bufsize += 64;
      tokens = realloc(tokens, bufsize * sizeof(char *));
      if (!tokens)
      {
        fprintf(stderr, "Allocation error\n");
        exit(EXIT_FAILURE);
      }
    }

    token = strtok(NULL, CMD_DELIMITER);
  }
  tokens[position] = NULL;
  return tokens;
}

int execute_command(char **args)
{
  pid_t pid;
  int status;

  if (args[0] == NULL)
  {
    return 1;
  }

  pid = fork();
  if (pid == 0)
  {
    if (execvp(args[0], args) == -1)
    {
      perror("Error");
    }
    exit(EXIT_FAILURE);
  }
  else if (pid < 0)
  {
    perror("Error");
  }
  else
  {
    do
    {
      waitpid(pid, &status, WUNTRACED);
    } while (!WIFEXITED(status) && !WIFSIGNALED(status));
  }

  return 1;
}

void check_reminders(struct reminder *r)
{
  if (r->type == POMODORO && !pomodoro_running)
  { // UPDATED
    return;
  }

  time_t current_time = time(NULL);
  if (difftime(current_time, r->last_time) >= r->interval)
  {
    printf("\n%s\n> ", r->message);
    r->last_time = current_time;
  }
}

void shell_loop()
{
  char *line;
  char **args;
  int status;
  struct reminder pomodoro = {0, POMODORO_TIME, "Pomodoro ended. Take a break!", POMODORO};                         // UPDATED
  struct reminder move_reminder = {0, MOVE_REMINDER_TIME, "Move reminder: Time to stand up and stretch!", MOVE};    // UPDATED
  struct reminder water_reminder = {0, WATER_REMINDER_TIME, "Water reminder: Don't forget to drink water!", WATER}; // UPDATED

  do
  {
    printf("> ");
    line = malloc(MAX_CMD_LENGTH);
    fgets(line, MAX_CMD_LENGTH, stdin);
    args = parse_command(line);

    if (strcmp(args[0], "start_pomodoro") == 0)
    {
      pomodoro.last_time = time(NULL);
      pomodoro_running = 1;
    }
    else if (strcmp(args[0], "stop_pomodoro") == 0)
    {
      pomodoro_running = 0;
      printf("Pomodoro stopped.\n");
    }
    else if (strcmp(args[0], "exit") == 0)
    { // NEW
      status = 0;
    }
    else
    {
      status = execute_command(args);
    }

    check_reminders(&pomodoro);
    check_reminders(&move_reminder);
    check_reminders(&water_reminder);

    free(line);
    free(args);
  } while (status);
}

int main(int argc, char **argv)
{
  signal(SIGCHLD, SIG_IGN); // Ignore child signals to avoid zombie processes
  shell_loop();
  return EXIT_SUCCESS;
}
