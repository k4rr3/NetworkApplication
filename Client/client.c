#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <time.h>
#define MAX_LEN 20
//TIMERS AND THRESHOLDS
#define T 1
#define P 2
#define Q 3
#define U 2
#define N 6
#define O 2
struct cfg
{
    unsigned char id[7];
    unsigned char mac[13];
    unsigned char nms_id[13];
    unsigned char nms_udp_port[5];
};
struct pdu_udp
{
    unsigned char pdu_type;
    char system_id[7];
    char mac_address[13];
    char random_number[7];
    char data[50];
};
// DECLARATION
struct cfg get_cfg(int argc, char *argv[]);
char *get_file_name(int argc, char *argv[]);
char *get_line(char line[], FILE *file);
void show_status(int status);

int main(int argc, char *argv[])
{
    int status = 0; // 0 == DISCONNECTED
    int debug = check_debug_mode(argc, argv);
    struct cfg user_cfg = get_cfg(argc, argv);
    show_status(status);
    connection_phase(status, user_cfg);
}
int check_debug_mode(int argc, char *argv[]){
     for (int i = 0; i < argc; i++)
    {
        if ((strcmp("-d", argv[i]) == 0))
        {
            return 1;
        }
    }
    return 0;
}
void show_status(int status)
{
    time_t current_time;
    struct tm *time_info;

    time(&current_time);
    time_info = localtime(&current_time);

    printf("%02d:%02d:%02d MSG.  =>  Equip passa a l'estat: ", time_info->tm_hour, time_info->tm_min, time_info->tm_sec);
    switch (status)
    {
    case 0:
        printf("DISCONNECTED \n");
        break;
    case 1:
        printf("WAIT_REG_RESPONSE \n");
        break;
    case 2:
        printf("WAIT_DB_CHECK \n");
        break;
    case 3:
        printf("REGISTERED \n");
        break;
    case 4:
        printf("SEND_ALIVE \n");
        break;
    }
}

struct cfg get_cfg(int argc, char *argv[])
{
    struct cfg user_cfg;
    char line[MAX_LEN];
    char *parsed_line;
    char *file_name = get_file_name(argc, argv);
    FILE *file = fopen(file_name, "r");
    if (!file)
    {
        printf("Error opening file: %s\n", file_name);
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < 4; i++)
    {
        parsed_line = get_line(line, file);
        if (!parsed_line)
        {
            printf("Error reading line %d\n", i + 1);
            exit(EXIT_FAILURE);
        }

        switch (i)
        {
        case 0:
            strncpy((char *)user_cfg.id, (char *)parsed_line, MAX_LEN);
            break;
        case 1:
            strncpy((char *)user_cfg.mac, (char *)parsed_line, MAX_LEN);
            break;
        case 2:
            strncpy((char *)user_cfg.nms_id, (char *)parsed_line, MAX_LEN);
            break;
        case 3:
            strncpy((char *)user_cfg.nms_udp_port, (char *)parsed_line, MAX_LEN);
            break;
        }
        // printf("%s\n", parsed_line);
    }
    fclose(file);
    return user_cfg;
}

char *get_line(char line[], FILE *file)
{
    char *word;
    if ((word = fgets(line, MAX_LEN, file)) != NULL)
    {
        line[strlen(line) - 1] = '\0';
        word = strtok(line, " ");
        word = strtok(NULL, " ");
        return word;
    }
    perror("An error ocurred when opening .cfg file");
    exit(1);
}

char *get_file_name(int argc, char *argv[])
{
    for (int i = 0; i < argc; i++)
    {
        if ((strcmp("-c", argv[i]) == 0))
        {
            return argv[i + 1];
        }
    }
    return "client.cfg";
}

void connection_phase(int status, struct cfg user_cfg)
{   struct timeval timeout = {2, 0};
    int sock ;
    if (sock = socket(AF_INET, SOCK_DGRAM, 0) < 0)
    {
        perror("socket: ");
        exit(1);
    }
     setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(struct timeval));
     

}
