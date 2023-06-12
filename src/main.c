// Inkludiere die benötigten Header-Dateien
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <pthread.h>
#include <limits.h>
#include <termios.h> 
#include <sys/wait.h> 
#include <readline/readline.h>
#include <readline/history.h>

// ANSI color codes
#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"

// Definiere Konstanten für maximale Eingabelänge und Befehlstrennzeichen
#define PROMPT_SIZE 4096
#define MAX_STR_SIZE 1023
#define PROMPT_SIZE 4096
#define MAX_CMD_LENGTH 1024
#define CMD_DELIMITER " \t\r\n\a"

// Definiere Konstanten für Pomodoro-, Bewegungs- und Wassererinnerungszeiten
int POMODORO_TIME = 25 * 60;
int MOVE_REMINDER_TIME = 60 * 60;
int WATER_REMINDER_TIME = 60 * 60;

// Definiere Variablen für den Status der einzelnen Funktionen (Laufen/Nicht Laufen)
__attribute__((unused)) int pomodoro_running = 0;
__attribute__((unused)) int water_running = 0;
__attribute__((unused)) int move_running = 0;

int pomodoro_time = 0;    // The time the pomodoro timer is set to
int elapsed_time_pomodoro = 0; // The time that has elapsed since the pomodoro timer started

int water_time = 0; // The time the water timer is set to
int elapsed_time_water = 0; // The time that has elapsed since the water timer started

int move_time = 0; // The time the move timer is set to
int elapsed_time_move = 0; // The time that has elapsed since the move timer started

// Definiere Thread-Objekte für Pomodoro-, Wasser- und Bewegungserinnerungen
pthread_t pomodoro_thread;
pthread_t water_thread;
pthread_t move_thread;

//declaration of  the mutexes and the start time for each thread
pthread_mutex_t pomodoro_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t water_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t move_mutex = PTHREAD_MUTEX_INITIALIZER;

time_t start_time_pomodoro = 0; 
time_t start_time_water = 0;
time_t start_time_move = 0;

//---------------------------------------
char *line_read = NULL;
char* show_prompt();

// Function to read a line
char *rl_gets()
{
    if (line_read)
    {
        free (line_read);
        line_read = NULL;
    }

    char* prompt = show_prompt();
    line_read = readline(prompt);
    free(prompt);

    if (line_read && *line_read)
        add_history (line_read);

    return line_read;
}
// cd-command
int cd_command(char **args) {
    if (args[1] == NULL) {
        fprintf(stderr, "Expected argument to \"cd\"\n");
    } else {
        if (chdir(args[1]) != 0) {
            perror("UniShell");
        }
    }
    return 1;
}
// change the default times
void set_timer(char *timer_type, int new_time_in_minutes) {
    // Convert minutes to seconds
    int new_time = new_time_in_minutes * 60;

    if (strcmp(timer_type, "POMODORO_TIME") == 0) {
        POMODORO_TIME = new_time;
    } else if (strcmp(timer_type, "MOVE_REMINDER_TIME") == 0) {
        MOVE_REMINDER_TIME = new_time;
    } else if (strcmp(timer_type, "WATER_REMINDER_TIME") == 0) {
        WATER_REMINDER_TIME = new_time;
    } else {
        fprintf(stderr, "Invalid timer type. Please enter a valid timer type (POMODORO_TIME, MOVE_REMINDER_TIME, WATER_REMINDER_TIME).\n");
    }
}
//---------------------------------------

// Funktion zum Anzeigen des benutzerdefinierten Prompts
char* show_prompt() {
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        perror("getcwd() error");
        return NULL;
    }
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char hostname[1024];
    gethostname(hostname, 1024);
    char *username = getenv("USER");

    // Allocate memory for the prompt
    char* prompt = malloc(sizeof(char) * PROMPT_SIZE);
    if (prompt == NULL) {
        perror("Failed to allocate memory for prompt");
        return NULL;
    }

    // Format and output of the prompts
    snprintf(prompt, PROMPT_SIZE, ANSI_COLOR_GREEN "[%02d:%02d] " ANSI_COLOR_RED "UNISHELL - %.1023s@%.1023s [%.1023s]" ANSI_COLOR_GREEN "\n >> " ANSI_COLOR_RESET, t->tm_hour, t->tm_min, username, hostname, cwd);

    return prompt;
}

// Funktion zum Parsen von Befehlen in der eingegebenen Zeile
char **parse_command(char *line)
{
    // Initialisiere Buffergröße und Positionsvariable
    int bufsize = 64;
    int position = 0;

    // Erstelle ein Token-Array und reserviere Speicherplatz
    char **tokens = malloc(bufsize * sizeof(char *));
    char *token;

    // Überprüfe, ob der Speicher erfolgreich reserviert wurde
    if (!tokens)
    {
        fprintf(stderr, "Allocation error\n");
        exit(EXIT_FAILURE);
    }

    // Trenne die Zeile in einzelne Befehle, basierend auf den Trennzeichen
    token = strtok(line, CMD_DELIMITER);
    while (token != NULL)
    {
        tokens[position] = token;
        position++;

        // Vergrößere den Speicher für das Token-Array, wenn nötig
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

// Funktion zum Ausführen von Befehlen
int execute_command(char **args)
{
    // Initialisiere Prozess-ID und Statusvariablen
    pid_t pid;
    int status;

    // Überprüfe, ob Befehle vorhanden sind
    if (args[0] == NULL)
    {
        return 1;
    }

    // Erstelle einen neuen Prozess
    pid = fork();
    if (pid == 0)
    {
        // Im Kindprozess: Führe den Befehl aus
        if (execvp(args[0], args) == -1)
        {
            perror("Error");
        }
        exit(EXIT_FAILURE);
    }
    else if (pid < 0)
    {
        // Fehler beim Forken des Prozesses
        perror("Error");
    }
    else
    {
        // Im Elternprozess: Warte auf den Kindprozess, um zu beenden
        do
        {
            waitpid(pid, &status, WUNTRACED);
        } while (!WIFEXITED(status) && !WIFSIGNALED(status));
    }

    return 1;
}

void *pomodoro_thread_func()
{
    int elapsed_time = 0;
    int pomodoro_time = POMODORO_TIME;

    printf("\n");

    while (elapsed_time < pomodoro_time)
    {
        sleep(1); // Sleep for 1 second
        elapsed_time++;

        // Change to elapsed_time % 30 == 0 für 30 sekunde zit reminders
        if (elapsed_time % 900 == 0)
        {
            int remaining_time = pomodoro_time - elapsed_time;

            if (remaining_time >= 3600)
            {
                printf("\nPomodoro Timer: %d hours %d minutes %d seconds left\n",
                       remaining_time / 3600, (remaining_time % 3600) / 60, remaining_time % 60);
            }
            else if (remaining_time >= 60)
            {
                printf("\nPomodoro Timer: %d minutes %d seconds left\n",
                       remaining_time / 60, remaining_time % 60);
            }
            else
            {
                printf("\nPomodoro Timer: %d seconds left\n", remaining_time);
            }
        }

        // Check if the Pomodoro time.
        if (elapsed_time == POMODORO_TIME)
        {
            printf("Pomodoro time is up!\n");
            printf("Pomodoro stopped.\n");
            printf("Waiting for new command\n");
            pthread_cancel(pthread_self());
            pthread_join(pthread_self(), NULL);
        }
    }
    return NULL;
}

void *water_thread_func()
{
    int elapsed_time = 0;
    int water_time = WATER_REMINDER_TIME;

    printf("\n");

    while (elapsed_time < water_time)
    {
        sleep(1); // Sleep for 1 second
        elapsed_time++;

        // Change to elapsed_time % 3600 == 0 for a reminder every hour
        if (elapsed_time % 900 == 0)
        {
            int remaining_time = water_time - elapsed_time;

            if (remaining_time >= 3600)
            {
                printf("\nPlease Drink Water!\nWater Reminder Time Left: %d hours %d minutes %d seconds left\n",
                       remaining_time / 3600, (remaining_time % 3600) / 60, remaining_time % 60);
            }
            else if (remaining_time >= 60)
            {
                printf("\nPlease Drink Water!\nWater Reminder Time Left: %d minutes %d seconds left\n",
                       remaining_time / 60, remaining_time % 60);
            }
            else
            {
                printf("\nPlease Drink Water!\nWater Reminder Time Left: %d seconds left\n", remaining_time);
            }
        }

        // Check if the Water time.
        if (elapsed_time == WATER_REMINDER_TIME)
        {
            printf(ANSI_COLOR_BLUE "\n");
            printf("╔════════════════════════════════════════════════════════WATER══════════════════════════════════════════════════════════╗\n");
            printf("║                                                                                                                       ║\n");
            printf("║                                                " ANSI_COLOR_RESET "WATER REMINDER IS UP" ANSI_COLOR_BLUE "                                                   ║\n");
            printf("║                                                                                                                       ║\n");
            printf("║ " ANSI_COLOR_CYAN  "DRINK                     " ANSI_COLOR_RESET "- WATER AND ONLY WATER                                                                      " ANSI_COLOR_BLUE "║\n");
            printf("║                                                                                                                       ║\n");
            printf("║⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿║\n");
            printf("║⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣀⣀⣀⣀⣸⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿║\n");
            printf("║⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡿⠋⠉⠉⠉⠻⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿║\n");
            printf("║⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡟⠁⠀⠀⠀⠀⠀⠘⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿║\n");
            printf("║⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⠀⠀⠀⠀⠀⠀⠀⠀⢸⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿║\n");
            printf("║⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣤⣤⣤⣤⣤⣤⣤⣤⣼⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿║\n");
            printf("║⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣥⣤⣤⣤⣤⣤⣤⣤⣼⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿║\n");
            printf("║⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣤⣤⣤⣤⣤⣤⣤⣤⣼⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿║\n");
            printf("║⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣍⣉⣉⣉⣉⣉⣉⣉⣹⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿║\n");
            printf("║⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⠉⠉⠉⠉⠉⠉⠉⠉⢹⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿║\n");
            printf("║⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⠀⠀⠀⠀⠀⠀⠀⠀⢸⣿⣿⣿⣿⡇⢀⣀⠀⠀⠀⢈⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿║\n");
            printf("║⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⠀⠀⠀⠀⠀⠀⠀⠀⢸⣿⣿⣿⣿⣿⠘⣿⠀⠀⠀⢸⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿║\n");
            printf("║⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⠀⠀⠀⠀⠀⠀⠀⠀⢸⣿⣿⣿⣿⣿⠀⣿⠀⠀⠀⢸⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿║\n");
            printf("║⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⠀⠀⠀⠀⠀⠀⠀⠀⢸⣿⣿⣿⣿⣿⠀⣿⠀⠀⠀⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿║\n");
            printf("║⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣷⣶⣶⣶⣶⣶⣶⣶⣿⣿⣿⣿⣿⣿⣶⣿⣶⣶⣶⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿║\n");
            printf("║                                                                                                                       ║\n");
            printf("║                                                                                                                       ║\n");
            printf("║ " ANSI_COLOR_YELLOW "Please enter a command:                                                                                               " ANSI_COLOR_BLUE "║\n");
            printf("╚═════════════════════════════════════════════════════════END═══════════════════════════════════════════════════════════╝\n");
            printf(ANSI_COLOR_RESET "\n");
            pthread_cancel(pthread_self());
            pthread_join(pthread_self(), NULL);
        }
    }
    return NULL;
}

void *move_thread_func()
{
    int elapsed_time = 0;
    int move_time = MOVE_REMINDER_TIME;

    printf("\n");

    while (elapsed_time < move_time)
    {
        sleep(1); // Sleep for 1 second
        elapsed_time++;

        // Change to elapsed_time % 5400 == 0 for a reminder every 1.5 hours
        if (elapsed_time % 900 == 0)
        {
            int remaining_time = move_time - elapsed_time;

            if (remaining_time >= 3600)
            {
                printf("\nMove Reminder Time Left: %d hours %d minutes %d seconds left\n",
                       remaining_time / 3600, (remaining_time % 3600) / 60, remaining_time % 60);
            }
            else if (remaining_time >= 60)
            {
                printf("\nMove Reminder Time Left: %d minutes %d seconds left\n",
                       remaining_time / 60, remaining_time % 60);
            }
            else
            {
                printf("\nMove Reminder Time Left: %d seconds left\n", remaining_time);
            }
        }

        // Check if the Water time.
        if (elapsed_time == MOVE_REMINDER_TIME)
        {
            printf(ANSI_COLOR_RED "\n");
            printf("╔═════════════════════════════════════════════════════════MOVE══════════════════════════════════════════════════════════╗\n");
            printf("║                                                                                                                       ║\n");
            printf("║                     " ANSI_COLOR_RESET "                      MOVE REMINDER IS UP - TIME TO MOVE                    " ANSI_COLOR_RED "                      ║\n");
            printf("║                                                                                                                       ║\n");
            printf("║ " ANSI_COLOR_GREEN "1. Neck Stretches           " ANSI_COLOR_RESET "- Tilt your head towards each shoulder and nod slowly forward and backward                " ANSI_COLOR_RED "║\n");
            printf("║ " ANSI_COLOR_GREEN "2. Shoulder Shrugs          " ANSI_COLOR_RESET "- Lift your shoulders towards your ears and then gently drop them down.                   " ANSI_COLOR_RED "║\n");
            printf("║ " ANSI_COLOR_GREEN "3. Hand and Wrist Stretches " ANSI_COLOR_RESET "- Extend each arm and gently pull your fingers back, first upwards, then downwards.       " ANSI_COLOR_RED "║\n");
            printf("║ " ANSI_COLOR_GREEN "4. Seated Spinal Twist      " ANSI_COLOR_RESET "- Seated, twist your upper body to one side, hold, and then switch sides.                 " ANSI_COLOR_RED "║\n");
            printf("║ " ANSI_COLOR_GREEN "5. Leg Extensions           " ANSI_COLOR_RESET "- Straighten one leg out while seated, hold, lower, and then switch legs.                 " ANSI_COLOR_RED "║\n");
            printf("║ " ANSI_COLOR_GREEN "6. Ankle Rolls              " ANSI_COLOR_RESET "- Lift one foot and slowly roll your ankle in a circular motion, then switch feet.        " ANSI_COLOR_RED "║\n");
            printf("║ " ANSI_COLOR_GREEN "7. Deep Breathing           " ANSI_COLOR_RESET "- Close your eyes and engage in deep, slow breaths, with a longer exhale than inhale.     " ANSI_COLOR_RED "║\n");
            printf("║                                                                                                                       ║\n");
            printf("║ " ANSI_COLOR_YELLOW "Please enter a command:                                                                                               " ANSI_COLOR_RED "║\n");
            printf("╚═════════════════════════════════════════════════════════END═══════════════════════════════════════════════════════════╝\n");
            printf(ANSI_COLOR_RESET "\n");
            pthread_cancel(pthread_self());
            pthread_join(pthread_self(), NULL);
        }
    }
    return NULL;
}

void start_pomodoro()
{
    if (POMODORO_TIME > 3600)
    {
        printf("Pomodoro started with a timer of %d hours, %d minutes and %d seconds\n", POMODORO_TIME / 3600, (POMODORO_TIME % 3600) / 60, POMODORO_TIME % 60);
    }
    else if (POMODORO_TIME > 60)
    {
        printf("Pomodoro started with a timer of %d minutes and %d seconds \n\n", POMODORO_TIME / 60, POMODORO_TIME % 60);
    }
    else
    {
        printf("Pomodoro started with a timer of %d seconds\n", POMODORO_TIME);
    }

    pomodoro_running = 1;
    if (pthread_create(&pomodoro_thread, NULL, pomodoro_thread_func, NULL) != 0)
    {
        fprintf(stderr, "Error creating pomodoro thread\n");
    }
}

void stop_pomodoro()
{
    pomodoro_running = 0;
    // Cancel the pomodoro thread
    pthread_cancel(pomodoro_thread);
    pthread_join(pomodoro_thread, NULL);
    printf("Pomodoro stopped.\n");
}

void start_water()
{
    // Es seit das es immer so isch will zit momentan obe hard coded isch!!
    if (WATER_REMINDER_TIME > 3600)
    {
        printf("Water Reminder started for the duration of %d hours, %d minutes and %d seconds\n", WATER_REMINDER_TIME / 3600, (WATER_REMINDER_TIME % 3600) / 60, WATER_REMINDER_TIME % 60);
    }
    else if (WATER_REMINDER_TIME > 60)
    {
        printf("Water Reminder started for the duration of %d minutes and %d seconds\n", WATER_REMINDER_TIME / 60, WATER_REMINDER_TIME % 60);
    }
    else
    {
        printf("Water Reminder started for the duration of %d seconds\n", WATER_REMINDER_TIME);
    }
    water_running = 1;
    if (pthread_create(&water_thread, NULL, water_thread_func, NULL) != 0)
    {
        fprintf(stderr, "Error creating Water Reminder thread\n");
    }
}

void stop_water()
{
    water_running = 0;
    // Cancel the pomodoro thread
    pthread_cancel(water_thread);
    pthread_join(water_thread, NULL);
    printf("Water Reminder stopped.\n");
}

void start_move()
{
    // Es seit das es immer so isch will zit momentan obe hard coded isch!!
    if (MOVE_REMINDER_TIME > 3600)
    {
        printf("Move Reminder started for the duration of %d hours, %d minutes and %d seconds\n", MOVE_REMINDER_TIME / 3600, (MOVE_REMINDER_TIME % 3600) / 60, MOVE_REMINDER_TIME % 60);
    }
    else if (MOVE_REMINDER_TIME > 60)
    {
        printf("Move Reminder started for the duration of %d minutes and %d seconds\n", MOVE_REMINDER_TIME / 60, MOVE_REMINDER_TIME % 60);
    }
    else
    {
        printf("Move Reminder started for the duration of %d seconds\n", MOVE_REMINDER_TIME);
    }
    move_running = 1;
    if (pthread_create(&move_thread, NULL, move_thread_func, NULL) != 0)
    {
        fprintf(stderr, "Error creating Move Reminder thread\n");
    }
}

void stop_move()
{
    move_running = 0;
    // Cancel the pomodoro thread
    pthread_cancel(move_thread);
    pthread_join(move_thread, NULL);
    printf("Move Reminder stopped.\n");
}

void* run_pomodoro(void* args) {
    // set the start time
    time(&start_time_pomodoro);

    for (int i = 0; i < POMODORO_TIME; i++) {
        sleep(1); // wait for 1 second
        // lock the mutex before modifying elapsed_time_pomodoro
        pthread_mutex_lock(&pomodoro_mutex);
        elapsed_time_pomodoro++;
        pthread_mutex_unlock(&pomodoro_mutex);
    }

    pthread_mutex_lock(&pomodoro_mutex);
    pomodoro_running = 0;
    pthread_mutex_unlock(&pomodoro_mutex);

    return NULL;
}
void* run_water(void* args) {
    // set the start time
    time(&start_time_water);

    for (int i = 0; i < WATER_REMINDER_TIME; i++) {
        sleep(1); // wait for 1 second
        // lock the mutex before modifying elapsed_time_water
        pthread_mutex_lock(&water_mutex);
        elapsed_time_water++;
        pthread_mutex_unlock(&water_mutex);
    }

    pthread_mutex_lock(&water_mutex);
    water_running = 0;
    pthread_mutex_unlock(&water_mutex);

    return NULL;
}
void* run_move(void* args) {
    // set the start time
    time(&start_time_move);

    for (int i = 0; i < MOVE_REMINDER_TIME; i++) {
        sleep(1); // wait for 1 second
        // lock the mutex before modifying elapsed_time_move
        pthread_mutex_lock(&move_mutex);
        elapsed_time_move++;
        pthread_mutex_unlock(&move_mutex);
    }

    pthread_mutex_lock(&move_mutex);
    move_running = 0;
    pthread_mutex_unlock(&move_mutex);

    return NULL;
}


void show_timers() {
    pthread_mutex_lock(&pomodoro_mutex);
    if (pomodoro_running) {
        int remaining_time_pomodoro = POMODORO_TIME - elapsed_time_pomodoro;
        int hours_pomodoro = remaining_time_pomodoro / 3600;
        int minutes_pomodoro = (remaining_time_pomodoro - (hours_pomodoro * 3600)) / 60;
        int seconds_pomodoro = remaining_time_pomodoro % 60;
        printf("Pomodoro Time Left: %02d:%02d:%02d\n", hours_pomodoro, minutes_pomodoro, seconds_pomodoro);
    } else {
        printf("Pomodoro timer is not running.\n");
    }
    pthread_mutex_unlock(&pomodoro_mutex);

    pthread_mutex_lock(&water_mutex);
    if (water_running) {
        int remaining_time_water = WATER_REMINDER_TIME - elapsed_time_water;
        int hours_water = remaining_time_water / 3600;
        int minutes_water = (remaining_time_water - (hours_water * 3600)) / 60;
        int seconds_water = remaining_time_water % 60;
        printf("Water Reminder Time Left: %02d:%02d:%02d\n", hours_water, minutes_water, seconds_water);
    } else {
        printf("Water reminder is not running.\n");
    }
    pthread_mutex_unlock(&water_mutex);

    pthread_mutex_lock(&move_mutex);
    if (move_running) {
        int remaining_time_move = MOVE_REMINDER_TIME - elapsed_time_move;
        int hours_move = remaining_time_move / 3600;
        int minutes_move = (remaining_time_move - (hours_move * 3600)) / 60;
        int seconds_move = remaining_time_move % 60;
        printf("Move Reminder Time Left: %02d:%02d:%02d\n", hours_move, minutes_move, seconds_move);
    } else {
        printf("Move reminder is not running.\n");
    }
    pthread_mutex_unlock(&move_mutex);
}

void shell_loop()
{
    char *line;
    char **args;
    int status = 1;

    do
    {
        show_prompt();
        //-----------------------------------------
        line = rl_gets();  // Use rl_gets instead of fgets
        if (line == NULL) 
        {
            // End of file (ctrl-d)
            printf("\n");
            exit(EXIT_SUCCESS);
        }

        args = parse_command(line);
        if (!args) 
        {
            fprintf(stderr, "Allocation error\n");
            exit(EXIT_FAILURE);
        }
        if (args[0] == NULL)
        {
            printf("Please enter a command\n");
        }
        else if (strcmp(args[0], "cd") == 0) {
            status = cd_command(args);
        }
        else if (strcmp(args[0], "set_timer") == 0) {
            if (args[1] == NULL || args[2] == NULL) {
                fprintf(stderr, "Expected timer type and new time for \"set_timer\"\n");
            } else {
                set_timer(args[1], atoi(args[2]));
            }
        }
        else if (strcmp(args[0], "start_pomodoro") == 0)
        {
            if (pomodoro_running)
            {
                printf("Pomodoro timer is already running\n");
            }
            else
            {
                start_pomodoro();
            }
        }
        else if (strcmp(args[0], "stop_pomodoro") == 0)
        {
            if (!pomodoro_running)
            {
                printf("Pomodoro timer is not running\n");
            }
            else
            {
                stop_pomodoro();
            }
        }
        else if (strcmp(args[0], "start_water") == 0)
        {
            if (water_running)
            {
                printf("Water reminder is already running\n");
            }
            else
            {
                start_water();
            }
        }
        else if (strcmp(args[0], "stop_water") == 0)
        {
            if (!water_running)
            {
                printf("Water reminder is not running\n");
            }
            else
            {
                stop_water();
            }
        }
        else if (strcmp(args[0], "start_move") == 0)
        {
            if (move_running)
            {
                printf("Move reminder is already running\n");
            }
            else
            {
                start_move();
            }
        }
        else if (strcmp(args[0], "stop_move") == 0)
        {
            if (!move_running)
            {
                printf("Move reminder is not running\n");
            }
            else
            {
                stop_move();
            }
        }
        else if (strcmp(line, "show_timers") == 0)
        {
            show_timers();
        }
        else if (strcmp(args[0], "help") == 0)
        {
            printf(ANSI_COLOR_RED "\n");
            printf("╔═══════════════════════════════════════HELP═════════════════════════════════════╗\n");
            printf("║                                                                                ║\n");
            printf("║                     " ANSI_COLOR_RESET "               COMMANDS               " ANSI_COLOR_RED "                     ║\n");
            printf("║                                                                                ║\n");
            printf("║ " ANSI_COLOR_GREEN "cd                " ANSI_COLOR_RESET "- Change the current directory                               " ANSI_COLOR_RED "║\n");
            printf("║ " ANSI_COLOR_GREEN "set_timer [T] [M] " ANSI_COLOR_RESET "- Set the duration for a specific timer in minutes           " ANSI_COLOR_RED "║\n");
            printf("║ " ANSI_COLOR_GREEN "                  " ANSI_COLOR_RESET "- [T]: Timer type either POMODORO_TIME, MOVE_REMINDER_TIME,  " ANSI_COLOR_RED "║\n");
            printf("║ " ANSI_COLOR_GREEN "                  " ANSI_COLOR_RESET "- or WATER_REMINDER_TIME.                                    " ANSI_COLOR_RED "║\n");
            printf("║ " ANSI_COLOR_GREEN "                  " ANSI_COLOR_RESET "- [M]: The new time in minutes.                              " ANSI_COLOR_RED "║\n");
            printf("║ " ANSI_COLOR_GREEN "start_pomodoro    " ANSI_COLOR_RESET "- Start the Pomodoro Timer                                   " ANSI_COLOR_RED "║\n");
            printf("║ " ANSI_COLOR_GREEN "stop_pomodoro     " ANSI_COLOR_RESET "- Stop the Pomodoro Timer                                    " ANSI_COLOR_RED "║\n");
            printf("║ " ANSI_COLOR_GREEN "start_water       " ANSI_COLOR_RESET "- Start the Water Reminder Timer                             " ANSI_COLOR_RED "║\n");
            printf("║ " ANSI_COLOR_GREEN "stop_water        " ANSI_COLOR_RESET "- Stop the Water Reminder Timer                              " ANSI_COLOR_RED "║\n");
            printf("║ " ANSI_COLOR_GREEN "start_move        " ANSI_COLOR_RESET "- Start the Movement Reminder Timer                          " ANSI_COLOR_RED "║\n");
            printf("║ " ANSI_COLOR_GREEN "stop_move         " ANSI_COLOR_RESET "- Stop the Movement Reminder Timer                           " ANSI_COLOR_RED "║\n");
            printf("║ " ANSI_COLOR_GREEN "show_timers       " ANSI_COLOR_RESET "- Show the status of all timers                              " ANSI_COLOR_RED "║\n");
            printf("║ " ANSI_COLOR_GREEN "start_all         " ANSI_COLOR_RESET "- Start all timers                                           " ANSI_COLOR_RED "║\n");
            printf("║ " ANSI_COLOR_GREEN "stop_all          " ANSI_COLOR_RESET "- Stop all timers                                            " ANSI_COLOR_RED "║\n");
            printf("║ " ANSI_COLOR_GREEN "exit              " ANSI_COLOR_RESET "- Exit the program                                           " ANSI_COLOR_RED "║\n");
            printf("║ " ANSI_COLOR_GREEN "help              " ANSI_COLOR_RESET "- Show this help again                                       " ANSI_COLOR_RED "║\n");
            printf("║                                                                                ║\n");
            printf("║ " ANSI_COLOR_YELLOW "Please enter a command:                                                        " ANSI_COLOR_RED "║\n");
            printf("╚════════════════════════════════════════END═════════════════════════════════════╝\n");
            printf(ANSI_COLOR_RESET "\n");
        }
        else if (strcmp(args[0], "start_all") == 0)
        {
            start_pomodoro();
            start_water();
            start_move();
        }
        else if (strcmp(args[0], "stop_all") == 0)
        {
            stop_pomodoro();
            stop_water();
            stop_move();
        }
        else if (strcmp(args[0], "exit") == 0)
        {
            status = 0;
        }
        else
        {
            status = execute_command(args);
            printf("waiting for input\n");
        }
        free(args);
    } while (status);
}

int main(int argc, char **argv)
{
    // Ignoriere SIGCHLD-Signale, um Zombie-Prozesse zu vermeiden
    signal(SIGCHLD, SIG_IGN);

    printf(ANSI_COLOR_RED "\
            ██╗   ██╗███╗   ██╗██╗███████╗██╗  ██╗███████╗██╗     ██╗             \n\
            ██║   ██║████╗  ██║██║██╔════╝██║  ██║██╔════╝██║     ██║             \n\
            ██║   ██║██╔██╗ ██║██║███████╗███████║█████╗  ██║     ██║             \n\
            ██║   ██║██║╚██╗██║██║╚════██║██╔══██║██╔══╝  ██║     ██║             \n\
            ╚██████╔╝██║ ╚████║██║███████║██║  ██║███████╗███████╗███████╗        \n\
             ╚═════╝ ╚═╝  ╚═══╝╚═╝╚══════╝╚═╝  ╚═╝╚══════╝╚══════╝╚══════╝        \n" ANSI_COLOR_RESET);

    printf(ANSI_COLOR_RED "\n");
    printf("╔════════════════════════════════════════════════════════════════════════════════╗\n");
    printf("║                       " ANSI_COLOR_RESET "Welcome to Your Personal Assistant" ANSI_COLOR_RED "                       ║\n");
    printf("║                                                                                ║\n");
    printf("║ " ANSI_COLOR_RESET "This program assists you in maintaining regular breaks                         " ANSI_COLOR_RED "║\n");
    printf("║ " ANSI_COLOR_RESET "and reminds you to drink enough water and move around.                         " ANSI_COLOR_RED "║\n");
    printf("║                                                                                ║\n");
    printf("║ " ANSI_COLOR_CYAN "Commands:                                                                      " ANSI_COLOR_RED "║\n");
    printf("║ " ANSI_COLOR_GREEN "cd                " ANSI_COLOR_RESET "- Change the current directory                               " ANSI_COLOR_RED "║\n");
    printf("║ " ANSI_COLOR_GREEN "set_timer [T] [M] " ANSI_COLOR_RESET "- Set the duration for a specific timer in minutes           " ANSI_COLOR_RED "║\n");
    printf("║ " ANSI_COLOR_GREEN "                  " ANSI_COLOR_RESET "- [T]: Timer type either POMODORO_TIME, MOVE_REMINDER_TIME,  " ANSI_COLOR_RED "║\n");
    printf("║ " ANSI_COLOR_GREEN "                  " ANSI_COLOR_RESET "- or WATER_REMINDER_TIME.                                    " ANSI_COLOR_RED "║\n");
    printf("║ " ANSI_COLOR_GREEN "                  " ANSI_COLOR_RESET "- [M]: The new time in minutes.                              " ANSI_COLOR_RED "║\n");
    printf("║ " ANSI_COLOR_GREEN "start_pomodoro    " ANSI_COLOR_RESET "- Start the Pomodoro Timer                                   " ANSI_COLOR_RED "║\n");
    printf("║ " ANSI_COLOR_GREEN "stop_pomodoro     " ANSI_COLOR_RESET "- Stop the Pomodoro Timer                                    " ANSI_COLOR_RED "║\n");
    printf("║ " ANSI_COLOR_GREEN "start_water       " ANSI_COLOR_RESET "- Start the Water Reminder Timer                             " ANSI_COLOR_RED "║\n");
    printf("║ " ANSI_COLOR_GREEN "stop_water        " ANSI_COLOR_RESET "- Stop the Water Reminder Timer                              " ANSI_COLOR_RED "║\n");
    printf("║ " ANSI_COLOR_GREEN "start_move        " ANSI_COLOR_RESET "- Start the Movement Reminder Timer                          " ANSI_COLOR_RED "║\n");
    printf("║ " ANSI_COLOR_GREEN "stop_move         " ANSI_COLOR_RESET "- Stop the Movement Reminder Timer                           " ANSI_COLOR_RED "║\n");
    printf("║ " ANSI_COLOR_GREEN "show_timers       " ANSI_COLOR_RESET "- Show the status of all timers                              " ANSI_COLOR_RED "║\n");
    printf("║ " ANSI_COLOR_GREEN "start_all         " ANSI_COLOR_RESET "- Start all timers                                           " ANSI_COLOR_RED "║\n");
    printf("║ " ANSI_COLOR_GREEN "stop_all          " ANSI_COLOR_RESET "- Stop all timers                                            " ANSI_COLOR_RED "║\n");
    printf("║ " ANSI_COLOR_GREEN "exit              " ANSI_COLOR_RESET "- Exit the program                                           " ANSI_COLOR_RED "║\n");
    printf("║ " ANSI_COLOR_GREEN "help              " ANSI_COLOR_RESET "- Show this help again                                       " ANSI_COLOR_RED "║\n");
    printf("║                                                                                ║\n");
    printf("║ " ANSI_COLOR_YELLOW "Please enter a command:                                                        " ANSI_COLOR_RED "║\n");
    printf("╚════════════════════════════════════════════════════════════════════════════════╝\n");
    printf(ANSI_COLOR_RESET "\n");

    // Starte die Shell-Schleife
    shell_loop();

    // Gebe EXIT_SUCCESS zurück, um das Programm ordnungsgemäß zu beenden
    return EXIT_SUCCESS;
}