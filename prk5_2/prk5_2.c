#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <signal.h>
#include <stdbool.h>

#define FIFO1 "/tmp/guess_number_fifo1"
#define FIFO2 "/tmp/guess_number_fifo2"

typedef struct {
    int number;
    bool is_guess;
    bool game_over;
} Message;

void cleanup() {
    unlink(FIFO1);
    unlink(FIFO2);
}

void handle_signal(int sig) {
    cleanup();
    exit(0);
}

void play_as_player1(int max_number, int read_fd, int write_fd) {
    srand(time(NULL) ^ getpid());
    int secret_number = rand() % max_number + 1;
    printf("Player1: I've guessed a number between 1 and %d. Let's play!\n", max_number);
    
    Message msg;
    int attempts = 0;
    
    while (1) {
        read(read_fd, &msg, sizeof(Message));
        
        if (msg.game_over) {
            break;
        }
        
        attempts++;
        msg.is_guess = (msg.number == secret_number);
        msg.game_over = msg.is_guess;
        
        write(write_fd, &msg, sizeof(Message));
        
        if (msg.is_guess) {
            printf("Player1: Correct guess! The number was %d. Attempts: %d\n", secret_number, attempts);
            break;
        } else {
            printf("Player1: Received guess %d (wrong)\n", msg.number);
        }
    }
}

void play_as_player2(int max_number, int read_fd, int write_fd) {
    srand(time(NULL) ^ getpid());
    Message msg;
    int attempts = 0;
    
    while (1) {
        attempts++;
        msg.number = rand() % max_number + 1;
        msg.is_guess = false;
        msg.game_over = false;
        
        printf("Player2: Guessing %d...\n", msg.number);
        write(write_fd, &msg, sizeof(Message));
        
        read(read_fd, &msg, sizeof(Message));
        
        if (msg.game_over) {
            if (msg.is_guess) {
                printf("Player2: I won! Number %d guessed in %d attempts.\n", msg.number, attempts);
            }
            break;
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
    
    // Set up signal handler for cleanup
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
    
    // Create FIFOs
    mkfifo(FIFO1, 0666);
    mkfifo(FIFO2, 0666);
    
    pid_t pid = fork();
    
    if (pid == -1) {
        perror("fork");
        cleanup();
        return 1;
    }
    
    if (pid == 0) { // Child process (Player2)
        int fd_read = open(FIFO1, O_RDONLY);
        int fd_write = open(FIFO2, O_WRONLY);
        
        for (int i = 0; i < 10; i++) {
            printf("\nRound %d: Player2 is guessing...\n", i+1);
            play_as_player2(max_number, fd_read, fd_write);
            sleep(1);
        }
        
        // Send termination message
        Message msg = {0, false, true};
        write(fd_write, &msg, sizeof(Message));
        
        close(fd_read);
        close(fd_write);
    } else { // Parent process (Player1)
        int fd_write = open(FIFO1, O_WRONLY);
        int fd_read = open(FIFO2, O_RDONLY);
        
        for (int i = 0; i < 10; i++) {
            printf("\nRound %d: Player1 is guessing...\n", i+1);
            play_as_player1(max_number, fd_read, fd_write);
            sleep(1);
        }
        
        // Send termination message
        Message msg = {0, false, true};
        write(fd_write, &msg, sizeof(Message));
        
        close(fd_read);
        close(fd_write);
        
        // Wait for child to finish
        wait(NULL);
        cleanup();
    }
    
    return 0;
}
