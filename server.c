#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <time.h>

#define BUFFER_SIZE 10240
#define MAX_CLIENTS  FD_SETSIZE

#define INIT 1
#define SELECT 2
#define END 3

#define FILE_NAME "game_scenarios.txt"
#define LOG_FILE "log.txt"

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

typedef struct {
    char text[256];
    char result_text[256];
    int health, exp, gold, attack, defense, level, stage;
    int next_scenario;  // 다음으로 이동할 시나리오 번호
} Choice;

typedef struct {
    int number;
    char description[512];
    Choice choices[3];
} Scenario;

void init_user_status(user_status *status, int user_id);
Scenario* file_read_scenario(const char *filename, int *scenario_count);
void  get_scenario(Scenario *scenario, int scenario_number);
void print_scenario(Scenario *scenario);
void print_all_scenarios(Scenario *scenarios, int count);
void save_log(const char *log);
void change_status(user_status *status, int select);

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        return 1;
    }

    int scenario_count = 0;

    Scenario *scenario;
    scenario = file_read_scenario(FILE_NAME, &scenario_count);
    //print_all_scenarios(scenario, scenario_count);

    int port = atoi(argv[1]);
    int server_fd, client_fd, fd_max, fd_num;
    int client_fds[MAX_CLIENTS];
    int client_count = 0, user_id = 1;

    struct sockaddr_in address;
    socklen_t addrlen = sizeof(address);
    stocPacket stocpacket;
    ctosPacket ctospacket;

    fd_set read_fds, temp_fds;

    // 1. 소켓 생성
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // 2. 옵션 설정
    int option = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));

    // 3. 주소 설정
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    // 4. 바인드
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    // 5. 리슨
    if (listen(server_fd, 1) < 0) {
        perror("listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d...\n", port);

    FD_ZERO(&read_fds);
    FD_SET(server_fd, &read_fds);
    fd_max = server_fd;

    while (1) {
        temp_fds = read_fds;
        fd_num = select(fd_max + 1, &temp_fds, NULL, NULL, NULL);
        if (fd_num < 0) {
            perror("select failed");
            exit(EXIT_FAILURE);
        }

        // 새로운 클라이언트 접속
        if (FD_ISSET(server_fd, &temp_fds)) {
            client_fd = accept(server_fd, (struct sockaddr *)&address, &addrlen);
            if (client_fd < 0) {
                perror("accept failed");
                continue;
            }

            if (client_count >= MAX_CLIENTS) {
                fprintf(stderr, "Too many clients.\n");
                close(client_fd);
                continue;
            }

            client_fds[client_count++] = client_fd;
            FD_SET(client_fd, &read_fds);
            if (client_fd > fd_max) fd_max = client_fd;

            printf("%d Client connected!\n", client_count);
        }

        // 기존 클라이언트 메시지 처리
        for (int i = 0; i < client_count; i++) {
            int cur_fd = client_fds[i];

            if (FD_ISSET(cur_fd, &temp_fds)) {
                int n = read(cur_fd, &ctospacket, sizeof(ctospacket));
                if (n <= 0) {
                    // 클라이언트가 연결을 종료했거나 오류 발생
                    close(cur_fd);
                    FD_CLR(cur_fd, &read_fds);
                    for (int j = i; j < client_count - 1; j++) {
                        client_fds[j] = client_fds[j + 1];
                    }
                    client_count--;
                    i--; // 리스트가 당겨졌으므로 인덱스 조정
                    printf("Client left, %d Client(s) remaining\n", client_count);
                } else {
                    // 클라이언트와 통신
                    if(ctospacket.cmd == INIT) {
                        init_user_status(&stocpacket.status,user_id);
                        user_id++;
                        strcpy(stocpacket.status.user_name, ctospacket.status.user_name);
                        strncpy(stocpacket.status.user_name, ctospacket.status.user_name, sizeof(stocpacket.status.user_name) - 1);
                        stocpacket.cmd = SELECT;

                        // 시나리오 번호에 따라 설명과 선택지 전송
                        int scenario_number = ctospacket.status.stage;
                        Scenario cur_scenario = scenario[scenario_number];
                        sprintf(stocpacket.buffer, "#%d\n%s\n1. %s\n2. %s\n3. %s",
                            cur_scenario.number,
                            cur_scenario.description,
                            cur_scenario.choices[0].text,
                            cur_scenario.choices[1].text,
                            cur_scenario.choices[2].text
                        );

                        write(cur_fd, &stocpacket, sizeof(stocpacket));
                    } else if (ctospacket.cmd == SELECT) {
                        // 선택 처리
                        char log_message[1024];
                        stocpacket.cmd = SELECT;
                        printf("User %s selected option %d\n", ctospacket.status.user_name, ctospacket.select);
                        sprintf(log_message,"User %s selected option %d [#%d -> #%d]\n", ctospacket.status.user_name, ctospacket.select,
                            ctospacket.status.stage, scenario[ctospacket.status.stage].choices[ctospacket.select - 1].next_scenario);
                        save_log(log_message);

                        stocpacket.status = ctospacket.status; // 선택한 유저 상태 저장
                        change_status(&stocpacket.status, ctospacket.select); // 선택에 따른 상태 변경
                        stocpacket.status.stage = scenario[stocpacket.status.stage].choices[ctospacket.select - 1].next_scenario; // 다음 시나리오 번호 업데이트

                        // 예시로 선택한 옵션에 따라 텍스트에서 결과 가져와서 stocpacket에 저장 후 전송 구현해야함
                        int scenario_number = stocpacket.status.stage;
                        Scenario cur_scenario = scenario[scenario_number];
                        sprintf(stocpacket.buffer, "#%d\n%s\n1. %s\n2. %s\n3. %s",
                            cur_scenario.number,
                            cur_scenario.description,
                            cur_scenario.choices[0].text,
                            cur_scenario.choices[1].text,
                            cur_scenario.choices[2].text
                        );

                        if(stocpacket.status.health <= 0) stocpacket.cmd = END;

                        write(cur_fd, &stocpacket, sizeof(stocpacket));
                    } else if (ctospacket.cmd == END) {
                        // 종료 처리
                        close(cur_fd);
                        FD_CLR(cur_fd, &read_fds);
                        for (int j = i; j < client_count - 1; j++) {
                            client_fds[j] = client_fds[j + 1];
                        }
                        client_count--;
                        i--; // 리스트가 당겨졌으므로 인덱스 조정
                        printf("Client left, %d Client(s) remaining\n", client_count);
                    }
                }
            }
        }
    }

    // 소켓 닫기
    for (int i = 0; i < client_count; i++) {
        close(client_fds[i]);
    }
    close(server_fd);
    printf("Server shutting down...\n");
    free(scenario);

    return 0;
}

void init_user_status(user_status *status, int user_id) {
    status->user_id = 0;
    status->stage = 0;
    status->health = 100;
    status->level = 1;
    status->exp = 0;
    status->gold = 0;
    status->attack = 10;
    status->defense = 5;
}

Scenario* file_read_scenario(const char *filename, int *scenario_count) {
    FILE *fp = fopen(filename, "r");
    if (fp == NULL) {
        perror("Failed to open scenario file");
        exit(EXIT_FAILURE);
    }

    Scenario *scenarios = malloc(sizeof(Scenario) * 100);
    char line[1024];
    int current = -1;

    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "#", 1) == 0) {
            current++;
            scenarios[current].number = atoi(line + 1);

            // 시나리오 설명
            fgets(line, sizeof(line), fp);
            line[strcspn(line, "\n")] = 0;
            strncpy(scenarios[current].description, line, sizeof(scenarios[current].description) - 1);

            for (int i = 0; i < 3; i++) {
                // 선택지 텍스트
                fgets(line, sizeof(line), fp);
                line[strcspn(line, "\n")] = 0;
                strncpy(scenarios[current].choices[i].text, line, sizeof(scenarios[current].choices[i].text) - 1);

                // 결과 파라미터
                fgets(line, sizeof(line), fp);
                sscanf(line,
                    "gold: %d, exp: %d, defense: %d, health: %d, level: %d, attack: %d, stage: %d, next: %d",
                    &scenarios[current].choices[i].gold,
                    &scenarios[current].choices[i].exp,
                    &scenarios[current].choices[i].defense,
                    &scenarios[current].choices[i].health,
                    &scenarios[current].choices[i].level,
                    &scenarios[current].choices[i].attack,
                    &scenarios[current].choices[i].stage,
                    &scenarios[current].choices[i].next_scenario
                );
            }
        }
    }

    fclose(fp);
    *scenario_count = current + 1;
    return scenarios;
}


void print_scenario(Scenario *scenario) {
    printf("Scenario %d: %s\n", scenario->number, scenario->description);
    for (int i = 0; i < 3; i++) {
        printf("Choice %d: %s\n", i + 1, scenario->choices[i].text);
    }
}

void print_all_scenarios(Scenario *scenarios, int count) {
    for (int i = 0; i < count; i++) {
        printf("Scenario #%d: %s\n", scenarios[i].number, scenarios[i].description);
        for (int j = 0; j < 3; j++) {
            printf("  Choice %d: %s\n", j + 1, scenarios[i].choices[j].text);
            printf("    → Result: HP %d, ATK %d, DEF %d, EXP %d, LV %d, Gold %d, Stage %d, Next %d\n",
                scenarios[i].choices[j].health,
                scenarios[i].choices[j].attack,
                scenarios[i].choices[j].defense,
                scenarios[i].choices[j].exp,
                scenarios[i].choices[j].level,
                scenarios[i].choices[j].gold,
                scenarios[i].choices[j].stage,
                scenarios[i].choices[j].next_scenario
            );
        }
        printf("--------------------------------------------------\n");
    }
}

void save_log(const char *log) {
    FILE *fp = fopen(LOG_FILE, "a");
    if (fp == NULL) {
        perror("Failed to open log file");
        return;
    }
    time_t now = time(NULL);
    char *timestamp = ctime(&now);
    timestamp[strcspn(timestamp, "\n")] = 0;
    fprintf(fp, "[%s] %s", timestamp, log);
    fclose(fp);
}

void change_status(user_status *status, int select) {
    status->health += status->health;
    status->exp += status->exp;
    status->gold += status->gold;
    status->attack += status->attack;
    status->defense += status->defense;
    if(status->exp >= 100) {
        status->level++;
        status->exp = status->exp - 100;
    }
    if(status->gold < 0) {
        status->gold = 0;
    }
}