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
// TIMERS AND THRESHOLDS
#define T 1
#define P 2
#define Q 3
#define U 2
#define N 6
#define O 2

#define R 2
#define S 3
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
void show_status(char text[], int status);
void connection_phase(int status, struct cfg user_cfg);
struct pdu_udp generate_pdu(struct cfg user_cfg, int pdu_type, char random_number[], char data[]);
void copyElements(char *src, char *dest, int start, int numElements);
void alive_phase(int sockfd, int status, struct cfg user_cfg, struct sockaddr_in server_address, char random_num[], char tcp_port[]);

int main(int argc, char *argv[])
{
    int status = DISCONNECTED;
    // int debug = check_debug_mode(argc, argv);
    struct cfg user_cfg = get_cfg(argc, argv);
    show_status("MSG.  =>  Equip passa a l'estat:", status);
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
void show_status(char text[], int status)
{
    time_t current_time;
    struct tm *time_info;

    time(&current_time);
    time_info = localtime(&current_time);

    printf("%02d:%02d:%02d %s", time_info->tm_hour, time_info->tm_min, time_info->tm_sec, text);
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
    perror("An error ocurred while opening .cfg file");
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
// ESTABLISH CONNECTION WITH THE SERVER AND REGISTER PHASE
void connection_phase(int status, struct cfg user_cfg)
{
    int sockfd;
    struct sockaddr_in client_address, server_address;

    // Create a UDP socket, open a communication channel
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
    {
        perror("socket() failed");
        exit(-1);
    }

    // Bind client's channel to a port and address
    memset(&client_address, 0, sizeof(client_address));
    client_address.sin_family = AF_INET;
    client_address.sin_addr.s_addr = htonl(0);
    client_address.sin_port = htons(0);

    if (bind(sockfd, (struct sockaddr *)&client_address, sizeof(client_address)) != 0)
    {
        perror("bind() failed");
        exit(-1);
    }

    // If address from the config file is localhost, then it is resolved as 127.0.0.1 otherwise the ip specified is established
    char *address;
    if (strcmp((char *)user_cfg.nms_id, "localhost") == 0)
    {
        address = "127.0.0.1";
    }
    else
    {
        address = (char *)user_cfg.nms_id;
    }

    // Set the server port and address to be able to send the packages
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = inet_addr(address);
    server_address.sin_port = htons(atoi((const char *)user_cfg.nms_udp_port));

    // Create PDU REGISTER_REQ package
    struct pdu_udp pdu_reg_req = generate_pdu(user_cfg, REGISTER_REQ, "000000", "");
    unsigned char pdu_package[78] = {"\n"};
    pdu_package[0] = REGISTER_REQ;
    strcpy((char *)&pdu_package[1], (const char *)pdu_reg_req.system_id);
    strcpy((char *)&pdu_package[1 + 7], (const char *)pdu_reg_req.mac_address);
    strcpy((char *)&pdu_package[1 + 7 + 13], (const char *)pdu_reg_req.random_number);
    strcpy((char *)&pdu_package[1 + 7 + 13 + 7], (const char *)pdu_reg_req.data);

    int interval = T;
    int packets_sent = 0;
    int process_made = 1;
    show_status("DEBUG =>  Registre equip, intent:  ", -1);
    printf("%d\n", process_made);

    // Set the status to WAIT_REG_RESPONSE
    status = WAIT_REG_RESPONSE;
    while (status != REGISTERED && status != DISCONNECTED && process_made <= O)
    {
        // Send register package to the server
        if (sendto(sockfd, pdu_package, 78, 0, (struct sockaddr *)&server_address, sizeof(server_address)) < 0)
        {
            perror("sendto() failed");
            exit(-1);
        }
        packets_sent++;

        // Wait for a response from the server
        fd_set read_fds;
        FD_ZERO(&read_fds);        // clears the file descriptor set read_fds and initializes it to the empty set.
        FD_SET(sockfd, &read_fds); // Adds the sockfd file descriptor to the read_fds set
        struct timeval timeout = {interval, 0};
        int select_status = select(sockfd + 1, &read_fds, NULL, NULL, &timeout);
        if (select_status == -1)
        {
            perror("select() failed");
            exit(-1);
        }
        else if (select_status == 0) // Timeout occurred
        {
            if (packets_sent <= P)
            {
                // Increase the interval until q * T seconds
                interval = T * packets_sent;
            }
            else
            {
                interval = Q * T;
            }
            if (packets_sent >= N)
            {
                process_made += 1;
                show_status("INFO  =>  Fallida registre amb servidor: localhost\n", -1);
                if (process_made <= O)
                {
                    sleep(U); // wait U seconds and restart a new registration process
                    show_status("DEBUG =>  Registre equip, intent: ", -1);
                    printf("%d\n", process_made);
                    // Restart the registration process
                    packets_sent = 0;
                    interval = T;
                    status = WAIT_REG_RESPONSE;
                }
            }
            else
            {
                // Resend the registration package
                show_status("MSG.  =>  Client passa a l'estat:", status);
            }
        }
        else // Response received
        {
            int received_bytes = recvfrom(sockfd, pdu_package, 78, 0, (struct sockaddr *)&server_address, &(socklen_t){sizeof(server_address)});
            if (received_bytes < 0)
            {
                perror("recvfrom() failed");
                exit(-1);
            }
            pdu_package[received_bytes] = '\0';
            switch (pdu_package[0])
            {
            case REGISTER_ACK: // 0x02
                status = REGISTERED;
                char random_num[7];
                char port[5];
                copyElements((char *)pdu_package, random_num, 21, 6);
                copyElements((char *)pdu_package, port, 28, 4);
                printf("Random numero leido es: %s\n", random_num);
                // printf("Port number: %s\n", port);
                alive_phase(sockfd, status, user_cfg, server_address, random_num, port);
                break;
            case REGISTER_NACK: // 0x04
                process_made += 1;
                show_status("INFO  =>  Fallida registre amb servidor: localhost\n", -1);
                if (process_made <= O)
                {
                    sleep(U); // wait U seconds and restart a new registration process
                    show_status("DEBUG =>  Registre equip, intent: ", -1);
                    printf("%d\n", process_made);
                    // Restart the registration process
                    packets_sent = 0;
                    interval = T;
                    status = WAIT_REG_RESPONSE;
                }
                break;
            case REGISTER_REJ: // 0x06
                status = DISCONNECTED;
                char data[50];
                memcpy(data, &pdu_package[28], 50);
                printf("REGISTER REJECTED: %s", data);

                break;
            case ERROR: // 0x0F
                break;
            }
        }
    }
}
void copyElements(char *src, char *dest, int start, int numElements)
{
    int i;
    for (i = 0; i < numElements; i++)
    {
        dest[i] = src[start + i];
    }
}
struct pdu_udp generate_pdu(struct cfg user_cfg, int pdu_type, char random_number[], char data[])
{
    struct pdu_udp pdu;
    memset(&pdu, 0, sizeof(pdu)); // To ensure pdu structure is all initialized with zero
    pdu.pdu_type = pdu_type;
    strcpy((char *)pdu.system_id, (const char *)user_cfg.id);
    strcpy((char *)pdu.mac_address, (const char *)user_cfg.mac);
    strcpy((char *)pdu.random_number, (const char *)random_number);
    strcpy((char *)pdu.data, (const char *)data);
    return pdu;
}
void alive_phase(int sockfd, int status, struct cfg user_cfg, struct sockaddr_in server_address, char random_num[], char tcp_port[])
{
    // Create PDU ALIVE_INF package
    struct pdu_udp pdu_alive_inf = generate_pdu(user_cfg, ALIVE_INF, random_num, "");
    unsigned char pdu_package[78] = {"\n"};
    pdu_package[0] = ALIVE_INF;
    strcpy((char *)&pdu_package[1], (const char *)pdu_alive_inf.system_id);
    strcpy((char *)&pdu_package[1 + 7], (const char *)pdu_alive_inf.mac_address);
    strcpy((char *)&pdu_package[1 + 7 + 13], (const char *)pdu_alive_inf.random_number);
    strcpy((char *)&pdu_package[1 + 7 + 13 + 7], (const char *)pdu_alive_inf.data);

    if (sendto(sockfd, pdu_package, 78, 0, (struct sockaddr *)&server_address, sizeof(server_address)) < 0)
    {
        perror("sendto() failed");
        exit(-1);
    }

    while (status != DISCONNECTED)
    {
        fd_set read_fds;
        FD_ZERO(&read_fds);        // clears the file descriptor set read_fds and initializes it to the empty set.
        FD_SET(sockfd, &read_fds); // Adds the sockfd file descriptor to the read_fds set
        struct timeval timeout = {R, 0};
        int select_status = select(sockfd + 1, &read_fds, NULL, NULL, &timeout);
        if (select_status == -1)
        {
            perror("select() failed");
            exit(-1);
        }
        else if (select_status == 0) // Timeout occurred
        {
            int received_bytes = recvfrom(sockfd, pdu_package, 78, 0, (struct sockaddr *)&server_address, &(socklen_t){sizeof(server_address)});
            if (received_bytes < 0)
            {
                perror("recvfrom() failed");
                exit(-1);
            }
            pdu_package[received_bytes] = '\0';
            switch (pdu_package[0])
            {
            case ALIVE_ACK:
                /* code */
                break;
            
            default:
                break;
            }
        }
        else // Response received
        {
        }
    }
}