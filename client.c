#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <termios.h>
#include <time.h>
#include <fcntl.h>
#include <ncurses.h>
#include <stdarg.h>
#include <locale.h>
#include <ncursesw/ncurses.h>
#include <wchar.h>

#define BUFFER_SIZE 10240
#define INIT 1
#define SELECT 2
#define END 3
#define CHAT 4
#define RESULT 5
#define ENDING 6

#define TIMEOUT_SECONDS 30
#define CHAT_WIDTH 40

typedef struct
{
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

typedef struct
{
    int cmd;
    char buffer[BUFFER_SIZE];
    user_status status;
} stocPacket;

typedef struct
{
    int cmd;
    int select;
    char buffer[BUFFER_SIZE];
    user_status status;
} ctosPacket;

WINDOW *game_win, *chat_win, *input_win;
int chat_line = 1;

void init_windows()
{
    setlocale(LC_ALL, "");
    initscr();
    cbreak();
    noecho();
    curs_set(0);
    refresh();

    int input_height = 5;
    int chat_height = LINES - input_height;
    int game_width = COLS - CHAT_WIDTH;

    game_win = newwin(chat_height, game_width, 0, 0);
    chat_win = newwin(chat_height, CHAT_WIDTH, 0, game_width);
    input_win = newwin(input_height, COLS, chat_height, 0);

    box(input_win, 0, 0);
    box(game_win, 0, 0);
    box(chat_win, 0, 0);
    mvwprintw(input_win, 1, 2, "입력: ");
    mvwprintw(chat_win, 0, 2, " CHAT ");
    wrefresh(input_win);
    wrefresh(game_win);
    wrefresh(chat_win);
}

void print_game(const char *format, ...)
{
    werase(game_win);
    box(game_win, 0, 0);

    char buf[BUFFER_SIZE];
    va_list args;
    va_start(args, format);
    vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);

    int max_x = getmaxx(game_win) - 4;
    int y = 2;
    char *p = buf;
    while (*p) {
        char *newline = strchr(p, '\n');
        int len = newline ? (newline - p) : strlen(p);
        int start = 0;
        while (start < len) {
            int width = 0, end = start;
            while (end < len && width < max_x) {
                unsigned char c = p[end];
                int char_width = 1;
                if (c >= 0x80) {
                    if ((c & 0xE0) == 0xC0) char_width = 2;
                    else if ((c & 0xF0) == 0xE0) char_width = 3;
                }
                int w = (char_width == 3) ? 2 : 1;
                if (width + w > max_x) break;
                width += w;
                end += char_width;
            }
            char temp[BUFFER_SIZE];
            strncpy(temp, p + start, end - start);
            temp[end - start] = '\0';
            mvwprintw(game_win, y++, 3, "%s", temp);
            start = end;
        }
        if (len == 0) y++; // 빈 줄도 줄바꿈
        p = newline ? newline + 1 : p + len;
    }

    wrefresh(game_win);
}

void print_chat(const char *msg)
{
    wchar_t wmsg[BUFFER_SIZE];
    mbstowcs(wmsg, msg, BUFFER_SIZE);

    int max_lines = getmaxy(chat_win) - 2; // -2: 테두리/타이틀 고려

    if (chat_line >= max_lines) {
        werase(chat_win);
        box(chat_win, 0, 0);
        mvwprintw(chat_win, 0, 2, " CHAT ");
        chat_line = 1;
    }
    mvwaddwstr(chat_win, chat_line++, 2, wmsg);
    wrefresh(chat_win);
}

void clear_input()
{
    werase(input_win);
    box(input_win, 0, 0);
    mvwprintw(input_win, 1, 2, "입력: ");
    wrefresh(input_win);
}

void clear_game()
{
    werase(game_win);
    box(game_win, 0, 0);
    wrefresh(game_win);
}

void destroy_windows()
{
    clear_game();
    delwin(input_win);
    delwin(game_win);
    delwin(chat_win);
    endwin();
}

int calculate_score(user_status stat)
{
    int score = 0;
    score += (stat.attack + stat.defense + stat.gold/10 + stat.exp/10);
    score *= stat.level;
    return score;
}

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        printf("Usage: %s <ip> <port>\\n", argv[0]);
        return 1;
    }

    int port = atoi(argv[2]);
    int sock = 0;
    struct sockaddr_in serv_addr;
    stocPacket stocpacket;
    ctosPacket ctospacket;
    fd_set read_fds;
    char chat_input[BUFFER_SIZE];

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        perror("Socket creation error");
        return -1;
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    serv_addr.sin_addr.s_addr = inet_addr(argv[1]);

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        perror("Connection failed");
        return -1;
    }

    init_windows();
    print_game("========================================\n"
               "  서버에 연결되었습니다!\n"
               "========================================\n");

    clear_input();
    mvwprintw(input_win, 1, 2, "닉네임을 입력하세요: ");
    wrefresh(input_win);
    echo();
    wmove(input_win, 1, 22);
    wgetnstr(input_win, ctospacket.status.user_name, 100);
    noecho();
    clear_input();
    print_game("아무 키나 누르면 게임이 시작됩니다...");
    wgetch(game_win);
    
    ctospacket.cmd = INIT;
    write(sock, &ctospacket, sizeof(ctospacket));
    print_game("다른 플레이어가 준비 중 ...");
    clear_input();
    
    ctospacket.cmd = SELECT;

    char my_nickname[100] = {0};

    // 닉네임 입력 후
    strncpy(my_nickname, ctospacket.status.user_name, sizeof(my_nickname));

    while (1)
    {
        FD_ZERO(&read_fds);
        FD_SET(0, &read_fds);
        FD_SET(sock, &read_fds);

        if (select(sock + 1, &read_fds, NULL, NULL, NULL) > 0)
        {
            if (FD_ISSET(sock, &read_fds))
            {
                read(sock, &stocpacket, sizeof(stocpacket));
                if (stocpacket.cmd == SELECT)
                {
                    print_game("%s", stocpacket.buffer);

                    // 시나리오 띄운 후 한 번만 윈도우 다시 그리기
                    wrefresh(game_win);

                    time_t start = time(NULL);
                    int input = -1;

                    while (1)
                    {
                        int elapsed = time(NULL) - start;
                        int remaining = TIMEOUT_SECONDS - elapsed;
                        int game_height = getmaxy(game_win);

                        // 남은 시간/선택 프롬프트를 맨 위(y=1)에 출력
                        mvwprintw(game_win, 1, 1, " 남은 시간: %2d초 | 선택지를 입력 (1~3): ", remaining);

                        wrefresh(game_win);

                        FD_ZERO(&read_fds);
                        FD_SET(0, &read_fds);
                        FD_SET(sock, &read_fds);

                        struct timeval timeout = {0, 200000};
                        select(sock + 1, &read_fds, NULL, NULL, &timeout);

                        if (FD_ISSET(sock, &read_fds))
                        {
                            read(sock, &stocpacket, sizeof(stocpacket));
                            if (stocpacket.cmd == CHAT)
                            {
                                print_chat(stocpacket.buffer);
                            }
                        }

                        // 입력창에서 실시간 입력 표시
                        if (FD_ISSET(0, &read_fds))
                        {
                            char input_buf[BUFFER_SIZE] = {0};
                            int pos = 0;
                            int ch;

                            clear_input();
                            wmove(input_win, 1, 8);
                            wrefresh(input_win);

                            while (1)
                            {
                                ch = wgetch(input_win);

                                if (ch == '\n' || ch == '\r')
                                {
                                    input_buf[pos] = '\0';
                                    break;
                                }
                                else if ((ch == KEY_BACKSPACE || ch == 127 || ch == 8) && pos > 0)
                                {
                                    pos--;
                                    input_buf[pos] = '\0';
                                }
                                else if (pos < BUFFER_SIZE - 1 && ch >= 32 && ch <= 126)
                                {
                                    input_buf[pos++] = ch;
                                    input_buf[pos] = '\0';
                                }

                                clear_input();
                                mvwprintw(input_win, 1, 8, "%s", input_buf);
                                wrefresh(input_win);
                            }

                            strcpy(chat_input, input_buf);

                            size_t len = strlen(chat_input);
                            // 입력이 1~3 중 한 글자면 선택지로 처리
                            if (len == 1 && chat_input[0] >= '1' && chat_input[0] <= '3')
                            {
                                input = chat_input[0] - '0';
                                print_game("\n선택지 %d번 선택됨\n", input);
                                break;
                            }
                            else if (len > 0 && !(len == 1 && chat_input[0] >= '1' && chat_input[0] <= '3'))
                            {
                                if (strcmp(input_buf, "exit") == 0)
                                {
                                ctospacket.cmd = END;
                                write(sock, &ctospacket, sizeof(ctospacket));
                                print_game("게임을 종료합니다.\n");
                                break;
                                }
                                ctospacket.cmd = CHAT;
                                strncpy(ctospacket.buffer, chat_input, BUFFER_SIZE - 1);
                                strncpy(ctospacket.status.user_name, my_nickname, sizeof(ctospacket.status.user_name));
                                write(sock, &ctospacket, sizeof(ctospacket));

                                char mychat[BUFFER_SIZE + 100];
                                snprintf(mychat, sizeof(mychat), "%s : %s", my_nickname, chat_input);
                                print_chat(mychat);

                                clear_input();
                            }
                        }

                        if (remaining <= 0)
                        {
                            input = rand() % 3 + 1;
                            print_game("\n시간 초과. 선택지 %d 자동 선택됨.\n", input);
                            break;
                        }
                    }

                    // 선택지 전송 부분
                    ctospacket.select = input;
                    ctospacket.cmd = SELECT;
                    strncpy(ctospacket.status.user_name, my_nickname, sizeof(ctospacket.status.user_name));
                    write(sock, &ctospacket, sizeof(ctospacket));
                }
                else if (stocpacket.cmd == RESULT)
                {
                    print_game("%s", stocpacket.buffer);                
                }
                else if (stocpacket.cmd == CHAT)
                {
                    print_chat(stocpacket.buffer);
                }
                else if (stocpacket.cmd == END)
                {
                    break;
                }
                else if(stocpacket.cmd == ENDING)
                {
                    char ending_msg[BUFFER_SIZE * 2];
                    snprintf(ending_msg, sizeof(ending_msg),
                        "******************************** 엔딩 *******************************\n\n\n\n%s\n\n\n게임이 종료되었습니다. 종료하려면 아무 키나 누르세요.\n",
                        stocpacket.buffer);

                    print_game("%s", ending_msg);
                    wgetch(game_win); // ncurses 모드에서 입력 받기
                    clear_game();

                    ctospacket.cmd = END;
                    write(sock, &ctospacket, sizeof(ctospacket));
                    break;
                }
            }
        }
    }

    destroy_windows();
    close(sock);
    printf("게임이 종료되었습니다.\n");
    return 0;
}