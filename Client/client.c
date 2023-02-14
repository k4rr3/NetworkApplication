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
#define BUFFER_SIZE 1024
#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 2023
#define MAX_LEN 20
// TIMERS AND THRESHOLDS
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
void connection_phase(int status, struct cfg user_cfg);
struct pdu_udp generate_pdu_request(struct cfg user_cfg);

int main(int argc, char *argv[])
{
    int status = 0; // 0 == DISCONNECTED
    // int debug = check_debug_mode(argc, argv);
    struct cfg user_cfg = get_cfg(argc, argv);
    show_status(status);
    connection_phase(status, user_cfg);
}
int check_debug_mode(int argc, char *argv[])
{
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

    printf("%02d:%02d:%02d_packagepdu_package.  =>  Equip passa a l'estat: ", time_info->tm_hour, time_info->tm_min, time_info->tm_sec);
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
        exit(-1);
    }

    for (int i = 0; i < 4; i++)
    {
        parsed_line = get_line(line, file);
        if (!parsed_line)
        {
            printf("Error reading line %d\n", i + 1);
            exit(-1);
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
{
    int sockfd;
    char buffer[BUFFER_SIZE];
    struct sockaddr_in server_address;

    // Create a UDP socket
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
    {
        perror("socket() failed");
        exit(-1);
    }

    // Set the server's address
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(atoi((const char *)user_cfg.nms_udp_port));
    char *address;
    if (strcmp((char *)user_cfg.nms_id, "localhost") == 0)
    {
        address = "127.0.0.1";
    }
    else
    {
        address = (char *)user_cfg.nms_id;
    }
    if (inet_pton(AF_INET, address, &server_address.sin_addr) != 1)
    {
        perror("inet_pton() failed");
        exit(-1);
    }
    // Create PDU REGISTER_REQ package
    struct pdu_udp pdu_reg_request = generate_pdu_request(user_cfg);
    unsigned char pdu_package[78] = {"\n"};
    pdu_package[0] = 0x00;
    //strcpy((char *)&pdu_package[0], (const char *)pdu_reg_request.pdu_type);
    strcpy((char *)&pdu_package[1], (const char *)pdu_reg_request.system_id);
    strcpy((char *)&pdu_package[1 + 7], (const char *)pdu_reg_request.mac_address);
    strcpy((char *)&pdu_package[1 + 7 + 13], (const char *)pdu_reg_request.random_number);
    strcpy((char *)&pdu_package[1 + 7 + 13 + 7], (const char *)pdu_reg_request.data);
    // strcpy((char*)&pdu_package[1], (const char*) user_cfg.)

    // Send data to the server
    if (sendto(sockfd, pdu_package, 78, 0, (struct sockaddr *)&server_address, sizeof(server_address)) < 0)
    {
        perror("sendto() failed");
        exit(-1);
    }

    /*  // Receive data from the server
     socklen_t server_address_len = sizeof(server_address);
     int received_bytes = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&server_address, &server_address_len);
     if (received_bytes < 0)
     {
         perror("recvfrom() failed");
         exit(-1);
     }
     buffer[received_bytes] = '\0';
     printf("Received message from server: %s\n", buffer); */

    // Close the socket
    close(sockfd);
}
struct pdu_udp generate_pdu_request(struct cfg user_cfg)
{
    struct pdu_udp pdu;
    memset(&pdu, 0, sizeof(pdu)); // To ensure pdu structure is all initialized with zero
    pdu.pdu_type = 0x00;
    strcpy((char *)pdu.system_id, (const char *)user_cfg.id);
    strcpy((char *)pdu.mac_address, (const char *)user_cfg.mac);
    strcpy((char *)pdu.random_number, (const char *)"0000000");
    strcpy((char *)pdu.data, (const char *)"");
    return pdu;
}
