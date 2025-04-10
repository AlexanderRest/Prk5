#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <sys/types.h>
#include <stdbool.h>

volatile sig_atomic_t guessed_number = 0;
volatile sig_atomic_t attempts = 0;
volatile sig_atomic_t game_over = 0;
volatile sig_atomic_t current_guess = 0;
pid_t other_pid;

void player1_handler(int sig, siginfo_t *info, void *context) {
    if (sig == SIGRTMIN) {
        current_guess = info->si_value.sival_int;
        attempts++;
        if (current_guess == guessed_number) {
            union sigval value;
            value.sival_int = 1; // correct guess
            sigqueue(other_pid, SIGUSR1, value);
            game_over = 1;
        } else {
            union sigval value;
            value.sival_int = 0; // wrong guess
            sigqueue(other_pid, SIGUSR2, value);
        }
    }
}

void player2_handler(int sig, siginfo_t *info, void *context) {
    if (sig == SIGUSR1) {
        printf("Player2: Correct! Number guessed in %d attempts.\n", attempts);
        game_over = 1;
    } else if (sig == SIGUSR2) {
        printf("Player2: Wrong guess (%d). Try again.\n", current_guess);
    }
}

void setup_signal_handlers(void (*handler)(int, siginfo_t *, void *), int sig) {
    struct sigaction sa;
    sa.sa_sigaction = handler;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    sigaction(sig, &sa, NULL);
}

void play_as_player1(int max_number) {
    srand(time(NULL) ^ getpid());
    guessed_number = rand() % max_number + 1;
    printf("Player1: I've guessed a number between 1 and %d. Let's play!\n", max_number);
    
    while (!game_over) {
        pause();
    }
    
    printf("Player1: Game over. The number was %d.\n", guessed_number);
}

void play_as_player2(int max_number) {
    int guess;
    srand(time(NULL) ^ getpid());
    
    while (!game_over) {
        guess = rand() % max_number + 1;
        printf("Player2: Guessing %d...\n", guess);
        
        union sigval value;
        value.sival_int = guess;
        sigqueue(other_pid, SIGRTMIN, value);
        
        pause(); // Wait for response
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
    
    pid_t pid = fork();
    
    if (pid == -1) {
        perror("fork");
        return 1;
    }
    
    if (pid == 0) { // Child process (Player2)
        other_pid = getppid();
        setup_signal_handlers(player2_handler, SIGUSR1);
        setup_signal_handlers(player2_handler, SIGUSR2);
        
        for (int i = 0; i < 10; i++) {
            game_over = 0;
            attempts = 0;
            printf("\nRound %d: Player2 is guessing...\n", i+1);
            play_as_player2(max_number);
            sleep(1); // Small delay before switching roles
        }
    } else { // Parent process (Player1)
        other_pid = pid;
        setup_signal_handlers(player1_handler, SIGRTMIN);
        
        for (int i = 0; i < 10; i++) {
            game_over = 0;
            attempts = 0;
            printf("\nRound %d: Player1 is guessing...\n", i+1);
            play_as_player1(max_number);
            sleep(1); // Small delay before switching roles
        }
        
        // Send termination signal to child
        kill(pid, SIGTERM);
        wait(NULL); // Wait for child to terminate
    }
    
    return 0;
}
