#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <netdb.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <arpa/inet.h>

#define SIZE 12

int sockfd;
char username[21];
char oppUsername[21];
char yourRole, oppRole; // 'X' or 'O'
char **board;
int gameOver = 1;

int getCode(char *buffer) {
    int code;
    sscanf(buffer, "%d", &code);
    return code;
}

char* getMessage(char *buffer, char *outMessage) {
    sscanf(buffer, "%*d %[^\t\n]", outMessage);
}

void pressEnterToContinue() {
    printf("Press Enter to continue");
    getchar();
}

void clearScreen() {
    printf("%c[2J%c[;H",(char) 27, (char) 27);
}

void clearBuffer() {
    int c;
    while (c != '\n' && c != EOF) c = getchar();
}

void clearBoard() {
    for (int i = 0; i < SIZE; i++) {
        for (int j = 0; j < SIZE; j++) {
            board[i][j] = ' ';
        }
    }
}

void draw_board() {
    printf("  ");
    for (int i = 0; i < SIZE; i++)
        printf(" %-2d ", i + 1);
    printf("\n");

    for (int i = 0; i < SIZE; i++) {
        printf("%-2d", i + 1);
        for (int j = 0; j < SIZE; j++) {
            printf(" %c ", board[i][j]);
            if (j != SIZE - 1) printf("|");
        }
        printf("\n");

        if (i == SIZE - 1) break;
        printf("  ");
        for (int j = 0; j < SIZE; j++) {
            printf("---");
            if (j != SIZE - 1) printf("+");
        }
        printf("\n");
    }
}

void login() {
    while (1) {
        printf("Enter a username: ");
        scanf("%20s", username);
        clearBuffer();
        char buffer[64], message[64];
        sprintf(buffer, "LOGIN %s", username);

        send(sockfd, buffer, strlen(buffer), 0);
        recv(sockfd, buffer, sizeof(buffer), 0);
        getMessage(buffer, message);
        printf("%s\n", message);
        if (getCode(buffer) == 210) break;
    }
}

int getMenuOption() {
    int option;
    clearScreen();
    printf("Logged in as %s\n", username);
    printf("-------- MENU --------\n");
    printf("1. New Game\n");
    printf("2. Quit\n");
    printf("\nSelect: ");
    while (1) {
        option = getchar();
        clearBuffer();
        if (option >= '1' && option <= '2') break;
        else printf("Invalid option, try again: ");
    }
    return option;
}

// return respond code
int findPlayer() {
    char buffer[64], request[64], message[64];
    strcpy(request, "FIND");
    send(sockfd, request, strlen(request), 0);

    fd_set read;
    struct timeval tv;
    int count_loop_finding = 0;
    while (1){
        clearScreen();
        printf("Finding player ");
        for (int i = 0; i <= count_loop_finding % 3; i++) printf(".");
        printf("\n");
        fflush(stdout); // fixbug not print immediately
        count_loop_finding++;

        tv.tv_sec = 1;
        tv.tv_usec = 0;
        FD_ZERO(&read); FD_SET(sockfd, &read);
        select(FD_SETSIZE, &read, NULL, NULL, &tv);
        if (FD_ISSET(sockfd, &read)) {
            recv(sockfd, buffer, sizeof(buffer), 0);
            sscanf(buffer, "%*d %s", oppUsername);
            return getCode(buffer);
        }
    }
}

void getOpponentMove(char *response) {
    int code, x, y;
    sscanf(response, "%d%d%d", &code, &x, &y);
    board[x][y] = oppRole;
}

void getYourMove() {
    char buffer[64];
    int x, y;
    printf("\nYour Turn (x y): ");
    while (1) {
        fgets(buffer, 64, stdin);
        if (sscanf(buffer, "%d%d", &x, &y) == 2) {
            x--; y--;
            if (0 <= x && x < SIZE && 0 <= y && y < SIZE && board[x][y] == ' ') {
                board[x][y] = yourRole;
                sprintf(buffer, "MOVE %d %d", x, y);
                send(sockfd, buffer, strlen(buffer), 0);
                break;
            }
        } 
        printf("Invalid Move, try again: ");
    }
}

void newGame() {
    int code;
    char buffer[64];
    int yourTurn;

    code = findPlayer();
    if (code == 120) {
        printf("TIMED OUT: Cannot find any player\n");
        pressEnterToContinue();
        return;
    }
    if (code == 220) {
        yourRole = 'X'; oppRole = 'O';
        yourTurn = 1;
        printf("Player found: %s, you are X\n", oppUsername);
    } else if (code == 221) {
        yourRole = 'O'; oppRole = 'X';
        yourTurn = 0;
        printf("Player found: %s, you are O\n", oppUsername);
    }
    printf("\n");
    pressEnterToContinue();

    clearBoard();
    gameOver = 0;
    while (!gameOver) {
        clearScreen();
        printf("You: %c\t\t%s: %c\n\n", yourRole, oppUsername, oppRole);
        draw_board();

        memset(buffer, 0, sizeof(buffer));
        if (yourTurn) {
            yourTurn = 0;
            getYourMove();
        } else {
            yourTurn = 1;
        }

        recv(sockfd, buffer, sizeof(buffer), 0);
        code = getCode(buffer);
        switch (code) {
        case 303: 
            gameOver = 1;
            printf("Opponent disconnected\n");
            pressEnterToContinue();
            break;
        case 342: 
            gameOver = 1;
            // re-draw board
            clearScreen();
            printf("You: %c\t\t%s: %c\n\n", yourRole, oppUsername, oppRole);
            draw_board();
            
            printf("You won!\n");
            pressEnterToContinue();
            break;
        case 343:
            gameOver = 1;
            getOpponentMove(buffer);
            // re-draw board
            clearScreen();
            printf("You: %c\t\t%s: %c\n\n", yourRole, oppUsername, oppRole);
            draw_board();

            printf("%s won!\n", oppUsername);
            pressEnterToContinue();
            break;
        case 330:
            getOpponentMove(buffer);
            break;
        case 230:
            break;
        }
    }
}

int main(int argc, char** argv) {
    sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    struct sockaddr_in saddr;
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(5000);
    saddr.sin_addr.s_addr = 0;

    // init board
    board = (char**) calloc(SIZE, sizeof(char*));
    for (int i = 0; i < SIZE; i++) {
        board[i] = (char*) calloc(SIZE, sizeof(char));
    }

    int conn = connect(sockfd, (struct sockaddr*) &saddr, sizeof(saddr));
    if (conn < 0) {
        printf("Unable to connect the server\n");
        return -1;
    }

    char buffer[64];
    recv(sockfd, buffer, sizeof(buffer), 0);
    if (getCode(buffer) != 200) return -1;
    printf("Welcome To Caro Game\n");

    login();
    int option;
    while (1) {
        option = getMenuOption();
        if (option == '2') {
            send(sockfd, "EXIT", 4, 0);
            break;
        }

        newGame();
    }
    
    close(sockfd);
}
