#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/time.h>
#include <fcntl.h>
#include <pthread.h>

#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>

#define MAX_LEN 20
#define UDP_PKG_SIZE 78
#define TCP_PKG_SIZE 178
// TIMERS AND THRESHOLDS
#define T 1
#define P 2
#define Q 3
#define U 2
#define N 6
#define O 2

#define R 2
#define S 3

#define W 3
// STATUS AND PDU ENUMS
enum status
{
    DISCONNECTED = 0xA0,
    WAIT_REG_RESPONSE = 0XA2,
    WAIT_DB_CHECK = 0xA4,
    REGISTERED = 0XA6,
    SEND_ALIVE = 0xA8
};
enum pdu_register
{
    REGISTER_REQ = 0x00,
    REGISTER_ACK = 0x02,
    REGISTER_NACK = 0x04,
    REGISTER_REJ = 0x06,
    ERROR = 0x0F
};
enum pdu_alive
{
    ALIVE_INF = 0x10,
    ALIVE_ACK = 0x12,
    ALIVE_NACK = 0x14,
    ALIVE_REJ = 0x16
};
enum pdu_send_cfg
{
    SEND_FILE = 0x20,
    SEND_DATA = 0x22,
    SEND_ACK = 0x24,
    SEND_NACK = 0x26,
    SEND_REJ = 0x28,
    SEND_END = 0x2A
};
enum pdu_get_cfg
{
    GET_FILE = 0x30,
    GET_DATA = 0x32,
    GET_ACK = 0x34,
    GET_NACK = 0x36,
    GET_REJ = 0x38,
    GET_END = 0x3A
};
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
struct pdu_tcp
{
    unsigned char pdu_type;
    char system_id[7];
    char mac_address[13];
    char random_number[7];
    char data[150];
};

struct cfg get_client_cfg(char *file_name);
char *search_arg(int argc, char *argv[], char *option, char *name);

int main(int argc, char *argv[])
{
    int status = DISCONNECTED;
    int debug = atoi(search_arg(argc, argv, "-d", "0"));
    char *boot_name = search_arg(argc, argv, "-f", "boot.cfg");
    char *file_name = search_arg(argc, argv, "-c", "client.cfg");
    struct cfg user_cfg = get_client_cfg(file_name);
    if (debug == 1)
    {
        show_status("DEBUG =>  Llegits paràmetres línia de comandes\n", -1);
        show_status("DEBUG =>  Llegits parametres arxius de configuració\n", -1);
        show_status("DEBUG =>  Inici bucle de servei equip : ", -1);
        printf("%s\n", user_cfg.id);
    }
    show_status("MSG.  =>  Equip passa a l'estat:", status);
    connection_phase(status, user_cfg, debug, boot_name, "000000", 1);
}
char *search_arg(int argc, char *argv[], char *option, char *name)
{
    for (int i = 0; i < argc; i++)
    {
        if ((strcmp(option, argv[i]) == 0))
        {
            if (argv[i + 1] != NULL)
            {
                return argv[i + 1];
            }
            else
            {
                return "1";
            }
        }
    }
    return name;
}
char *read_line(char line[], FILE *file)
{
    char *word;
    if ((word = fgets(line, MAX_LEN, file)) != NULL)
    {
        line[strlen(line) - 1] = '\0';
        word = strtok(line, " ");
        word = strtok(NULL, " ");
        return word;
    }
    perror("An error ocurred while opening .cfg file");
    exit(1);
}
struct cfg get_client_cfg(char *file_name)
{
    struct cfg user_cfg;
    char line[MAX_LEN];
    char *parsed_line;
    FILE *file = fopen(file_name, "r");
    if (!file)
    {
        printf("Error opening file: %s\n", file_name);
        exit(-1);
    }

    for (int i = 0; i < 4; i++)
    {
        parsed_line = read_line(line, file);
        if (!parsed_line)
        {
            printf("Error reading line %d\n", i + 1);
            exit(-1);
        }

        switch (i)
        {
        case 0:
            strncpy((char *)user_cfg.id, (char *)parsed_line, sizeof(user_cfg.id));
            break;
        case 1:
            strncpy((char *)user_cfg.mac, (char *)parsed_line, sizeof(user_cfg.mac));
            break;
        case 2:
            strncpy((char *)user_cfg.nms_id, (char *)parsed_line, sizeof(user_cfg.nms_id));
            break;
        case 3:
            strncpy((char *)user_cfg.nms_udp_port, (char *)parsed_line, sizeof(user_cfg.nms_udp_port));
            break;
        }
        // printf("%s\n", parsed_line);
    }
    fclose(file);
    return user_cfg;
}
void show_status(char text[], int status)
{
    switch (status)
    {
    case 0xA0:
        printf("DISCONNECTED \n");
        break;
    case 0xA2:
        printf("WAIT_REG_RESPONSE \n");
        break;
    case 0xA4:
        printf("WAIT_DB_CHECK \n");
        break;
    case 0xA6:
        printf("REGISTERED \n");
        break;
    case 0xA8:
        printf("SEND_ALIVE \n");
    default:
        break;
    }
}
void print_time(){
    time_t current_time;
    struct tm *time_info;
    time(&current_time);
    time_info = localtime(&current_time);
    printf("%02d:%02d:%02d %s", time_info->tm_hour, time_info->tm_min, time_info->tm_sec);
    
}