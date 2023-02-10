#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <time.h>
#define MAX_LEN 20
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
char *get_params(int argc, char *argv[]);
char *get_line(char line[], FILE *file);
void show_status();
int main(int argc, char *argv[])
{
    // char status = "DISCONNECTED";
    struct cfg user_cfg = get_cfg(argc, argv);
    show_status();
}

void show_status()
{
    time_t current_time;
    struct tm *time_info;

    time(&current_time);
    time_info = localtime(&current_time);

    printf("%02d:%02d:%02d\n", time_info->tm_hour, time_info->tm_min, time_info->tm_sec);
}
/* struct cfg get_cfg(int argc, char *argv[])
{
    struct cfg user_cfg;
    char* result;
    char* file_name = get_params(argc, argv);
    char line[MAX_LEN];
    FILE* file = fopen(file_name, "r");
     for (int i = 0; i < 4; i++)
    {
        //printf("%i", i);
        result = get_line(line, file);
        switch (i)
        {
        case 0:
            strcpy(user_cfg.id, result);
            break;
        case 1:
            strcpy(user_cfg.mac, result);
        case 2:
            strcpy(user_cfg.nms_id, result);
        case 3:
            strcpy(user_cfg.nms_udp_port, result);
        default:
            break;
        }
        printf("%s\n", result);
    }
 */
struct cfg get_cfg(int argc, char *argv[])
{
    struct cfg user_cfg;
    char line[MAX_LEN];
    char *parsed_line;
    char *file_name = get_params(argc, argv);
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

char *get_params(int argc, char *argv[])
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
