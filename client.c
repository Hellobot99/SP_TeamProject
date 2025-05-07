#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/select.h>

#define BUFFER_SIZE 10240

#define INIT 1
#define SELECT 2
#define END 3

typedef struct{
    int user_id;
    int stage;
    int health;
    int level;
    int exp;
    int gold;
    int attack;
    int defense;
    //int item[10];
    char user_name[100];
} user_status;

typedef struct {
    int cmd;
    char buffer[BUFFER_SIZE];
    user_status status;
} stocPacket;

typedef struct {
    int cmd;
    int select;
    user_status status;
} ctosPacket;

void user_interface();

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <ip> <port>\n", argv[0]);
        return 1;
    }

    int port = atoi(argv[2]);
    int sock = 0, fd_num, fd_max;
    struct sockaddr_in serv_addr;

    char name[100];
    char input[BUFFER_SIZE];

    stocPacket stocpacket;
    ctosPacket ctospacket;

    // 1. 소켓 생성
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Socket creation error");
        return -1;
    }

    // 2. 서버 주소 설정
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    serv_addr.sin_addr.s_addr = inet_addr(argv[1]);

    // 3. 서버에 연결
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Connection failed");
        return -1;
    }

    printf("Connected to server!\n");

    printf("Enter your name: ");
    scanf("%s", ctospacket.status.user_name);

    printf("Press Enter to start the game...\n");
    getchar();

    ctospacket.cmd = INIT;
    write(sock, &ctospacket, sizeof(ctospacket));
    ctospacket.cmd = SELECT;

    // 4. 메시지 보내고 받기
    while (1) {
        read(sock, &stocpacket, sizeof(stocpacket));
        if (stocpacket.cmd == SELECT) {
            printf("%s\n", stocpacket.buffer);
            printf("Select an option: ");
            scanf("%d", &ctospacket.select);
            ctospacket.status = stocpacket.status; // 현재 유저 상태 저장
            ctospacket.cmd = SELECT;
            write(sock, &ctospacket, sizeof(ctospacket));
        } 
        else if (stocpacket.cmd == END) {
            printf("Game Over!\n");
            break;
        }
    }

    // 5. 소켓 닫기
    close(sock);
    return 0;
}

void user_interface(){

}
