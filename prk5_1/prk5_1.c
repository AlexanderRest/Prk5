#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdbool.h>
#include <sys/time.h>

#define MAX_ROUNDS 10

typedef struct {
    int number;
    int attempts;
    struct timeval start_time;
} GameData;

volatile sig_atomic_t current_role = 0; // 0 - родитель загадывает, 1 - ребенок загадывает
volatile sig_atomic_t game_over = 0;
volatile sig_atomic_t current_guess = 0;
volatile sig_atomic_t response_received = 0;
volatile sig_atomic_t rounds_played = 0;
pid_t other_pid;

void thinker_handler(int sig, siginfo_t *info, void *context) {
    (void)context;
    if (sig == SIGRTMIN) {
        current_guess = info->si_value.sival_int;
        GameData* data = (GameData*)info->si_value.sival_ptr;
        data->attempts++;

        union sigval value;
        if (current_guess == data->number) {
            struct timeval end_time;
            gettimeofday(&end_time, NULL);
            long seconds = end_time.tv_sec - data->start_time.tv_sec;
            long microseconds = end_time.tv_usec - data->start_time.tv_usec;
            double elapsed = seconds + microseconds*1e-6;

            value.sival_int = data->attempts;
            printf("Угадано за %d попыток! Время: %.3f сек.\n", data->attempts, elapsed);
            sigqueue(other_pid, SIGUSR1, value);
            game_over = 1;
            current_role = !current_role;
        } else {
            value.sival_int = 0;
            sigqueue(other_pid, SIGUSR2, value);
        }
    }
}

void guesser_handler(int sig, siginfo_t *info, void *context) {
    (void)context;
    if (sig == SIGUSR1) {
        printf("Успех! Число угадано за %d попыток.\n", info->si_value.sival_int);
        game_over = 1;
        current_role = !current_role;
        rounds_played++;
        response_received = 1;
    } else if (sig == SIGUSR2) {
        printf("Не угадал. Пробую снова...\n");
        response_received = 1;
    }
}

void setup_signal_handlers(void (*handler)(int, siginfo_t *, void *), int sig) {
    struct sigaction sa;
    sa.sa_sigaction = handler;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    sigaction(sig, &sa, NULL);
}

void play_as_thinker(int max_number) {
    GameData data;
    data.number = rand() % max_number + 1;
    data.attempts = 0;
    gettimeofday(&data.start_time, NULL);

    printf("Загадал число от 1 до %d. Ожидаю попыток...\n", max_number);

    while (!game_over) {
        pause();
    }
}

void play_as_guesser(int max_number) {
    GameData data;
    data.attempts = 0;
    gettimeofday(&data.start_time, NULL);

    while (!game_over) {
        data.attempts++;
        data.number = rand() % max_number + 1;
        printf("Попытка %d: %d\n", data.attempts, data.number);

        union sigval value;
        value.sival_ptr = &data;
        response_received = 0;

        if (sigqueue(other_pid, SIGRTMIN, value) == -1) {
            perror("sigqueue");
            break;
        }

        // Ждем ответа
        while (!response_received && !game_over) {
            pause();
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <max_number>\n", argv[0]);
        return 1;
    }

    int max_number = atoi(argv[1]);
    if (max_number <= 1) {
        fprintf(stderr, "Max number must be greater than 1\n");
        return 1;
    }

    srand(time(NULL));

    // Блокируем сигналы перед fork
    sigset_t mask, oldmask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGRTMIN);
    sigaddset(&mask, SIGUSR1);
    sigaddset(&mask, SIGUSR2);
    sigprocmask(SIG_BLOCK, &mask, &oldmask);

    pid_t pid = fork();

    if (pid == -1) {
        perror("fork");
        return 1;
    }

    if (pid == 0) { // Дочерний процесс
        other_pid = getppid();

        while (rounds_played < MAX_ROUNDS) {
            if (current_role) {
                setup_signal_handlers(thinker_handler, SIGRTMIN);
                printf("\n=== Раунд %d ===\n", rounds_played + 1);
                printf("Я - Игрок 2 (загадывающий)\n");
                play_as_thinker(max_number);
            } else {
                setup_signal_handlers(guesser_handler, SIGUSR1);
                setup_signal_handlers(guesser_handler, SIGUSR2);
                printf("\n=== Раунд %d ===\n", rounds_played + 1);
                printf("Я - Игрок 2 (угадывающий)\n");
                play_as_guesser(max_number);
            }
        }
        _exit(0);
    } else { // Родительский процесс
        other_pid = pid;

        while (rounds_played < MAX_ROUNDS) {
            if (!current_role) {
                setup_signal_handlers(thinker_handler, SIGRTMIN);
                printf("\n=== Раунд %d ===\n", rounds_played + 1);
                printf("Я - Игрок 1 (загадывающий)\n");
                play_as_thinker(max_number);
            } else {
                setup_signal_handlers(guesser_handler, SIGUSR1);
                setup_signal_handlers(guesser_handler, SIGUSR2);
                printf("\n=== Раунд %d ===\n", rounds_played + 1);
                printf("Я - Игрок 1 (угадывающий)\n");
                play_as_guesser(max_number);
            }
        }

        kill(pid, SIGTERM);
        wait(NULL);
    }

    return 0;
}
