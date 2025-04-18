#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <signal.h>
#include <time.h>
#include <string.h>
#include <sys/time.h>

#define MAX_ROUNDS 10
#define FIFO1 "/tmp/guess_number_fifo1"
#define FIFO2 "/tmp/guess_number_fifo2"

typedef struct {
    int number;
    int attempts;
    int is_guess;
    int game_over;
    struct timeval start_time;
} GameMessage;

volatile sig_atomic_t terminate = 0;

void cleanup() {
    unlink(FIFO1);
    unlink(FIFO2);
}

void handle_signal(int sig) {
    terminate = 1;
    cleanup();
}

void create_fifos() {
    mkfifo(FIFO1, 0666);
    mkfifo(FIFO2, 0666);
}

void play_thinker(int max_number, int read_fd, int write_fd) {
    GameMessage msg;
    msg.number = rand() % max_number + 1;
    msg.attempts = 0;
    msg.is_guess = 0;
    msg.game_over = 0;
    gettimeofday(&msg.start_time, NULL);

    printf("Загадал число от 1 до %d\n", max_number);
    write(write_fd, &msg, sizeof(GameMessage));

    while (!terminate) {
        if (read(read_fd, &msg, sizeof(GameMessage)) > 0) {
            if (msg.game_over) break;

            msg.attempts++;
            if (msg.number == msg.number) {
                struct timeval end_time;
                gettimeofday(&end_time, NULL);
                double elapsed = (end_time.tv_sec - msg.start_time.tv_sec) +
                               (end_time.tv_usec - msg.start_time.tv_usec) / 1000000.0;

                printf("Число %d угадано за %d попыток (время: %.3f сек.)\n",
                       msg.number, msg.attempts, elapsed);

                msg.is_guess = 1;
                msg.game_over = 1;
                write(write_fd, &msg, sizeof(GameMessage));
                break;
            } else {
                msg.is_guess = 0;
                write(write_fd, &msg, sizeof(GameMessage));
            }
        }
    }
}

void play_guesser(int max_number, int read_fd, int write_fd) {
    GameMessage msg;

    while (!terminate) {
        if (read(read_fd, &msg, sizeof(GameMessage)) > 0) {
            if (msg.game_over) break;

            msg.attempts++;
            msg.number = rand() % max_number + 1;
            printf("Попытка %d: %d\n", msg.attempts, msg.number);

            write(write_fd, &msg, sizeof(GameMessage));

            if (read(read_fd, &msg, sizeof(GameMessage)) > 0) {
                if (msg.is_guess) {
                    printf("Угадал число %d!\n", msg.number);
                    break;
                }
            }
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Использование: %s <макс_число>\n", argv[0]);
        return 1;
    }

    int max_number = atoi(argv[1]);
    if (max_number <= 1) {
        fprintf(stderr, "Число должно быть больше 1\n");
        return 1;
    }

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    srand(time(NULL));
    cleanup();
    create_fifos();

    pid_t pid = fork();

    if (pid == -1) {
        perror("fork");
        return 1;
    }

    if (pid == 0) { // Дочерний процесс (Игрок 2)
        int fd_read = open(FIFO1, O_RDONLY);
        int fd_write = open(FIFO2, O_WRONLY);

        for (int i = 0; i < MAX_ROUNDS && !terminate; i++) {
            printf("\n=== Раунд %d ===\n", i+1);

            if (i % 2 == 0) {
                printf("Я - Игрок 2 (угадываю)\n");
                play_guesser(max_number, fd_read, fd_write);
            } else {
                printf("Я - Игрок 2 (загадываю)\n");
                play_thinker(max_number, fd_read, fd_write);
            }
        }

        close(fd_read);
        close(fd_write);
        _exit(0);
    } else { // Родительский процесс (Игрок 1)
        int fd_write = open(FIFO1, O_WRONLY);
        int fd_read = open(FIFO2, O_RDONLY);

        for (int i = 0; i < MAX_ROUNDS && !terminate; i++) {
            printf("\n=== Раунд %d ===\n", i+1);

            if (i % 2 == 0) {
                printf("Я - Игрок 1 (загадываю)\n");
                play_thinker(max_number, fd_read, fd_write);
            } else {
                printf("Я - Игрок 1 (угадываю)\n");
                play_guesser(max_number, fd_read, fd_write);
            }
        }

        kill(pid, SIGTERM);
        wait(NULL);

        close(fd_read);
        close(fd_write);
        cleanup();
    }

    return 0;
}
