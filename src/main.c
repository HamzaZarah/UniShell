// Inkludiere die benötigten Header-Dateien
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <pthread.h>
#include <limits.h>

//NEW----
#include <termios.h>  // Include the termios library

// ANSI color codes
#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"

// Definiere Konstanten für maximale Eingabelänge und Befehlstrennzeichen
#define MAX_CMD_LENGTH 1024
#define CMD_DELIMITER " \t\r\n\a"

// Definiere Konstanten für Pomodoro-, Bewegungs- und Wassererinnerungszeiten
#define POMODORO_TIME 2 * 60
#define MOVE_REMINDER_TIME 3 * 60
#define WATER_REMINDER_TIME 6 * 60

// Definiere Variablen für den Status der einzelnen Funktionen (Laufen/Nicht Laufen)
__attribute__((unused)) int pomodoro_running = 0;
__attribute__((unused)) int water_running = 0;
__attribute__((unused)) int move_running = 0;

// Definiere Thread-Objekte für Pomodoro-, Wasser- und Bewegungserinnerungen
pthread_t pomodoro_thread;
pthread_t water_thread;
pthread_t move_thread;

//----------------------------------------------------------------
// Funktion zum Anzeigen des benutzerdefinierten Prompts
void show_prompt() {
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        perror("getcwd() error");
        return;
    }
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char hostname[1024];
    gethostname(hostname, 1024);
    char *username = getenv("USER");

    // Format und Ausgabe des Prompts
    printf(ANSI_COLOR_GREEN "[%02d:%02d] " ANSI_COLOR_RED "UNISHELL - %s@%s [%s]" ANSI_COLOR_GREEN"\n >> ", t->tm_hour, t->tm_min, username, hostname, cwd);
    printf(ANSI_COLOR_RESET);
    fflush(stdout);//NEW: Stellt sicher, dass die Ausgabe sofort angezeigt wird
}
//----------------------------------------------------------------

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
        if (elapsed_time % 31 == 0)
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
        if (elapsed_time % 91 == 0)
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
            printf("Water Reminder time is up!\n");
            printf("Water Reminder stopped.\n");
            printf("Waiting for new command\n");
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
        if (elapsed_time % 15 == 0)
        {
            int remaining_time = move_time - elapsed_time;

            if (remaining_time >= 3600)
            {
                printf("\nPlease Move!\nMove Reminder Time Left: %d hours %d minutes %d seconds left\n",
                       remaining_time / 3600, (remaining_time % 3600) / 60, remaining_time % 60);
            }
            else if (remaining_time >= 60)
            {
                printf("\nPlease Move!\nMove Reminder Time Left: %d minutes %d seconds left\n",
                       remaining_time / 60, remaining_time % 60);
            }
            else
            {
                printf("\nPlease Move!\nMove Reminder Time Left: %d seconds left\n", remaining_time);
            }
        }

        // Check if the Water time.
        if (elapsed_time == MOVE_REMINDER_TIME)
        {
            printf("Move Reminder time is up!\n");
            printf("Move Reminder stopped.\n");
            printf("Waiting for new command\n");
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

void shell_loop()
{
    char *line;
    char **args;
    int status = 1;
    // Initialize pthread

    do
    {
        show_prompt();
        line = malloc(MAX_CMD_LENGTH);

        fgets(line, MAX_CMD_LENGTH, stdin);

        args = parse_command(line);

        if (args[0] == NULL)
        {
            printf("Please enter a command\n");
        }
        else if (strcmp(args[0], "start_pomodoro") == 0)
        {
            start_pomodoro();
        }
        else if (strcmp(args[0], "stop_pomodoro") == 0)
        {
            stop_pomodoro();
        }
        else if (strcmp(args[0], "start_water") == 0)
        {
            start_water();
        }
        else if (strcmp(args[0], "stop_water") == 0)
        {
            stop_water();
        }
        else if (strcmp(args[0], "start_move") == 0)
        {
            start_move();
        }
        else if (strcmp(args[0], "stop_move") == 0)
        {
            stop_move();
        }
        else if (strcmp(args[0], "help") == 0)
        {
            printf(ANSI_COLOR_RED "\n");
            printf("╔═══════════════════════════════════════HELP═════════════════════════════════════╗\n");
            printf("║                                                                                ║\n");
            printf("║                     " ANSI_COLOR_RESET "               COMMANDS               " ANSI_COLOR_RED "                     ║\n");
            printf("║                                                                                ║\n");
            printf("║ " ANSI_COLOR_GREEN "start_pomodoro " ANSI_COLOR_RESET "- Start the Pomodoro Timer                                      " ANSI_COLOR_RED "║\n");
            printf("║ " ANSI_COLOR_GREEN "stop_pomodoro  " ANSI_COLOR_RESET "- Stop the Pomodoro Timer                                       " ANSI_COLOR_RED "║\n");
            printf("║ " ANSI_COLOR_GREEN "start_water    " ANSI_COLOR_RESET "- Start the Water Reminder Timer                                " ANSI_COLOR_RED "║\n");
            printf("║ " ANSI_COLOR_GREEN "stop_water     " ANSI_COLOR_RESET "- Stop the Water Reminder Timer                                 " ANSI_COLOR_RED "║\n");
            printf("║ " ANSI_COLOR_GREEN "start_move     " ANSI_COLOR_RESET "- Start the Movement Reminder Timer                             " ANSI_COLOR_RED "║\n");
            printf("║ " ANSI_COLOR_GREEN "stop_move      " ANSI_COLOR_RESET "- Stop the Movement Reminder Timer                              " ANSI_COLOR_RED "║\n");
            printf("║ " ANSI_COLOR_GREEN "start_all      " ANSI_COLOR_RESET "- Start all timers                                              " ANSI_COLOR_RED "║\n");
            printf("║ " ANSI_COLOR_GREEN "stop_all       " ANSI_COLOR_RESET "- Stop all timers                                               " ANSI_COLOR_RED "║\n");
            printf("║ " ANSI_COLOR_GREEN "exit           " ANSI_COLOR_RESET "- Exit the program                                              " ANSI_COLOR_RED "║\n");
            printf("║ " ANSI_COLOR_GREEN "help           " ANSI_COLOR_RESET "- Show this help again                                          " ANSI_COLOR_RED "║\n");
            printf("║                                                                                ║\n");
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
        free(line);
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
    printf("║ " ANSI_COLOR_GREEN "start_pomodoro " ANSI_COLOR_RESET "- Start the Pomodoro Timer                                      " ANSI_COLOR_RED "║\n");
    printf("║ " ANSI_COLOR_GREEN "stop_pomodoro  " ANSI_COLOR_RESET "- Stop the Pomodoro Timer                                       " ANSI_COLOR_RED "║\n");
    printf("║ " ANSI_COLOR_GREEN "start_water    " ANSI_COLOR_RESET "- Start the Water Reminder Timer                                " ANSI_COLOR_RED "║\n");
    printf("║ " ANSI_COLOR_GREEN "stop_water     " ANSI_COLOR_RESET "- Stop the Water Reminder Timer                                 " ANSI_COLOR_RED "║\n");
    printf("║ " ANSI_COLOR_GREEN "start_move     " ANSI_COLOR_RESET "- Start the Movement Reminder Timer                             " ANSI_COLOR_RED "║\n");
    printf("║ " ANSI_COLOR_GREEN "stop_move      " ANSI_COLOR_RESET "- Stop the Movement Reminder Timer                              " ANSI_COLOR_RED "║\n");
    printf("║ " ANSI_COLOR_GREEN "start_all      " ANSI_COLOR_RESET "- Start all timers                                              " ANSI_COLOR_RED "║\n");
    printf("║ " ANSI_COLOR_GREEN "stop_all       " ANSI_COLOR_RESET "- Stop all timers                                               " ANSI_COLOR_RED "║\n");
    printf("║ " ANSI_COLOR_GREEN "exit           " ANSI_COLOR_RESET "- Exit the program                                              " ANSI_COLOR_RED "║\n");
    printf("║ " ANSI_COLOR_GREEN "help           " ANSI_COLOR_RESET "- Show this help again                                          " ANSI_COLOR_RED "║\n");
    printf("║                                                                                ║\n");
    printf("║ " ANSI_COLOR_YELLOW "Please enter a command:                                                        " ANSI_COLOR_RED "║\n");
    printf("╚════════════════════════════════════════════════════════════════════════════════╝\n");
    printf(ANSI_COLOR_RESET "\n");

    //printf("Welcome to Hamza and Albert's Halal Shell !\n If you need help type 'help'\n ");
    // Starte die Shell-Schleife
    shell_loop();

    // Gebe EXIT_SUCCESS zurück, um das Programm ordnungsgemäß zu beenden
    return EXIT_SUCCESS;
}
