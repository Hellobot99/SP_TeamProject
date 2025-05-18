#define _GNU_SOURCE
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
#define MAX_STAGE    100
#define INIT 1
#define SELECT 2
#define END 3
#define CHAT 4
#define RESULT 5
#define FILE_NAME "game_scenarios.txt"

typedef struct {
    int user_id;
    int stage;
    int health;
    int level;
    int exp;
    int gold;
    int attack;
    int defense;
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
    char buffer[BUFFER_SIZE];
    user_status status;
} ctosPacket;

typedef struct {
    char text[256];
    char result_text[256];
    int health, exp, gold, attack, defense, level, stage;
    int next_scenario;
} Choice;

typedef struct {
    int number;
    char description[512];
    Choice choices[3];
} Scenario;

typedef struct {
    int stage;
    int client_fds[MAX_CLIENTS];
    int selected[MAX_CLIENTS];
    user_status statuses[MAX_CLIENTS];
    int client_count;
    time_t start_time;
    int active;
    int all_selected;
} CoopEvent;

Scenario scenarios[MAX_STAGE];
CoopEvent coop_event;
int client_inited[MAX_CLIENTS] = {0}; // 각 클라이언트의 INIT 여부

void init_user_status(user_status *status, int user_id);
void change_status(user_status *status, Choice select);
int calculate_majority(int selected[], int count);
Scenario* load_scenarios(const char *filename, int *count);
void reset_event();

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        return 1;
    }

    int scenario_count;
    load_scenarios(FILE_NAME, &scenario_count);

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr;
    fd_set read_fds, temp_fds;
    int fd_max, client_fds[MAX_CLIENTS], client_count = 0;
    socklen_t addrlen = sizeof(addr);

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(atoi(argv[1]));

    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));
    bind(server_fd, (struct sockaddr*)&addr, sizeof(addr));
    listen(server_fd, 5);

    FD_ZERO(&read_fds);
    FD_SET(server_fd, &read_fds);
    fd_max = server_fd;
    int count_temp = 0;

    printf("Server started on port %s\n", argv[1]);
    reset_event();

    while (1) {
        temp_fds = read_fds;
        select(fd_max + 1, &temp_fds, NULL, NULL, &(struct timeval){1, 0});    

        for (int i = 0; i <= fd_max; i++) {
            if (FD_ISSET(i, &temp_fds)) {
                if (i == server_fd) {
                    int client_fd = accept(server_fd, (struct sockaddr*)&addr, &addrlen);
                    FD_SET(client_fd, &read_fds);
                    if (client_fd > fd_max) fd_max = client_fd;
                    client_fds[client_count++] = client_fd;
                    printf("Client connected\n");

                    // 새 클라이언트 등록
                    coop_event.client_fds[coop_event.client_count] = client_fd;
                    init_user_status(&coop_event.statuses[coop_event.client_count], client_fd);
                    coop_event.selected[coop_event.client_count] = 0;
                    client_inited[coop_event.client_count] = 0;
                    coop_event.client_count++;
                }
                else {
                    ctosPacket ctospacket;
                    int n = read(i, &ctospacket, sizeof(ctospacket));
                    if (n <= 0) {
                        close(i); FD_CLR(i, &read_fds);
                        continue;
                    }

                    if (ctospacket.cmd == INIT) {
                        // 닉네임만 갱신
                        int idx = -1;
                        for (int j = 0; j < coop_event.client_count; j++) {
                            if (coop_event.client_fds[j] == i) {
                                idx = j;
                                break;
                            }
                        }
                        if (idx != -1) {
                            strncpy(coop_event.statuses[idx].user_name, ctospacket.status.user_name, 99);
                            client_inited[idx] = 1;
                        }

                        // 모든 클라이언트가 INIT을 보냈는지 확인
                        int all_inited = 1;
                        for (int j = 0; j < coop_event.client_count; j++) {
                            if (!client_inited[j]) {
                                all_inited = 0;
                                break;
                            }
                        }

                        if (all_inited && coop_event.client_count > 0 && !coop_event.active) {
                            coop_event.stage = coop_event.statuses[0].stage;
                            coop_event.start_time = time(NULL);
                            coop_event.active = 1;

                            // 모든 클라이언트에게 SELECT 패킷 전송
                            for (int j = 0; j < coop_event.client_count; j++) {
                                stocPacket pkt = { .cmd = SELECT, .status = coop_event.statuses[j] };
                                Scenario scen = scenarios[coop_event.statuses[j].stage];
                                snprintf(pkt.buffer, sizeof(pkt.buffer), "#%d\n%s\n1. %s\n2. %s\n3. %s",
                                    scen.number, scen.description, scen.choices[0].text, scen.choices[1].text, scen.choices[2].text);
                                write(coop_event.client_fds[j], &pkt, sizeof(pkt));
                            }
                        }
                    } else if (ctospacket.cmd == SELECT) {
                        for (int j = 0; j < coop_event.client_count; j++) {
                            if (coop_event.client_fds[j] == i) {
                                coop_event.selected[j] = ctospacket.select;

                                stocPacket pkt = { .cmd = CHAT, .status = coop_event.statuses[j] };
                                snprintf(pkt.buffer, sizeof(pkt.buffer),
                                    "선택 완료. 다른 유저를 기다리는 중..\n");
                                write(i, &pkt, sizeof(pkt));

                                // --- 선택 사실을 다른 플레이어에게 알림 ---
                                for (int k = 0; k < coop_event.client_count; k++) {
                                    if (k != j) {
                                        stocPacket chatpkt = { .cmd = CHAT };
                                        snprintf(chatpkt.buffer, sizeof(chatpkt.buffer),
                                            "%s님이 %d번 선택지를 골랐습니다.", ctospacket.status.user_name, ctospacket.select);
                                        write(coop_event.client_fds[k], &chatpkt, sizeof(chatpkt));
                                    }
                                }
                            }
                        }
                        // --- 모든 클라이언트가 선택을 했는지 확인 ---
                        int all_selected = 1;
                        for (int j = 0; j < coop_event.client_count; j++) {
                            if (coop_event.selected[j] == 0) {
                                all_selected = 0;
                                break;
                            }
                        }
                        coop_event.all_selected = all_selected;

                        coop_event.stage = scenarios[coop_event.stage].choices->next_scenario;

                        if (coop_event.all_selected && coop_event.active) {
                            int final_choice = calculate_majority(coop_event.selected, coop_event.client_count);
                            for (int j = 0; j < coop_event.client_count; j++) {
                                stocPacket pkt = { .cmd = RESULT, .status = coop_event.statuses[j] };    
                                Scenario scen = scenarios[coop_event.statuses[j].stage];   
                                sprintf(pkt.buffer, "결과 : [%d] %s",final_choice,scen.choices->result_text);                     
                                write(coop_event.client_fds[j], &pkt, sizeof(pkt));
                                coop_event.statuses[j].stage = coop_event.stage;
                            }                            

                            sleep(10);

                            for (int j = 0; j < coop_event.client_count; j++) {
                                stocPacket pkt = { .cmd = SELECT, .status = coop_event.statuses[j] };
                                Scenario scen = scenarios[coop_event.statuses[j].stage];
                                snprintf(pkt.buffer, sizeof(pkt.buffer), "#%d\n%s\n1. %s\n2. %s\n3. %s",
                                    scen.number, scen.description, scen.choices[0].text, scen.choices[1].text, scen.choices[2].text);
                                write(coop_event.client_fds[j], &pkt, sizeof(pkt));
                            }

                            coop_event.all_selected = 0;
                            for (int j = 0; j < coop_event.client_count; j++) {
                                coop_event.selected[j] = 0;                                
                        }

                        }
                    } else if (ctospacket.cmd == CHAT) {
                        ctospacket.buffer[strcspn(ctospacket.buffer, "\n")] = '\0';
                        for (int j = 0; j < coop_event.client_count; j++) {
                            if (coop_event.client_fds[j] != i) {
                                stocPacket chatpkt = { .cmd = CHAT };
                                snprintf(chatpkt.buffer, sizeof(chatpkt.buffer), "%s: %s", ctospacket.status.user_name, ctospacket.buffer);
                                write(coop_event.client_fds[j], &chatpkt, sizeof(chatpkt));
                            }
                        }
                    }
                }
            }
        }
    }
    return 0;
}

void reset_event() {
    coop_event.client_count = 0;
    coop_event.active = 0;
    coop_event.all_selected = 0;
    memset(coop_event.client_fds, 0, sizeof(coop_event.client_fds));
    memset(coop_event.selected, 0, sizeof(coop_event.selected));
}

void init_user_status(user_status *status, int user_id) {
    status->user_id = user_id;
    status->stage = 0;
    status->health = 100;
    status->level = 1;
    status->exp = 0;
    status->gold = 0;
    status->attack = 10;
    status->defense = 5;
    strcpy(status->user_name, "");
}

void change_status(user_status *status, Choice select) {
    status->health += select.health;
    status->exp += select.exp;
    status->gold += select.gold;
    status->attack += select.attack;
    status->defense += select.defense;
    status->level = select.level > 0 ? select.level : status->level;
}

int calculate_majority(int selected[], int count) {
    int vote[4] = {0};
    for (int i = 0; i < count; i++) vote[selected[i]]++;
    int max_vote = 1;
    for (int i = 2; i <= 3; i++) if (vote[i] > vote[max_vote]) max_vote = i;
    return max_vote;
}

Scenario* load_scenarios(const char *filename, int *count) {
    FILE *fp = fopen(filename, "r");
    if (!fp) exit(1);
    char line[1024]; int current = -1;
    while (fgets(line, sizeof(line), fp)) {
        if (line[0] == '#') {
            current++;
            scenarios[current].number = atoi(line + 1);
            fgets(scenarios[current].description, sizeof(scenarios[current].description), fp);
            for (int i = 0; i < 3; i++) {
                fgets(scenarios[current].choices[i].text, sizeof(scenarios[current].choices[i].text), fp);
                fgets(scenarios[current].choices[i].result_text, sizeof(scenarios[current].choices[i].result_text), fp);
                fgets(line, sizeof(line), fp);
                sscanf(line, "gold: %d, exp: %d, defense: %d, health: %d, level: %d, attack: %d, stage: %d, next: %d",
                    &scenarios[current].choices[i].gold, &scenarios[current].choices[i].exp,
                    &scenarios[current].choices[i].defense, &scenarios[current].choices[i].health,
                    &scenarios[current].choices[i].level, &scenarios[current].choices[i].attack,
                    &scenarios[current].choices[i].stage, &scenarios[current].choices[i].next_scenario);
                fgets(line, sizeof(line), fp);
            }
        }
    }
    fclose(fp);
    *count = current + 1;
    return scenarios;
}