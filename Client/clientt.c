#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/time.h>
#include <fcntl.h>

#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <pthread.h>

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
// DECLARATION
struct cfg get_client_cfg(char *file_name);
char *get_line(char line[], FILE *file);
void show_status(char text[], int status);
void connection_phase(int status, struct cfg user_cfg, int debug, char *boot_name, char random_number[7], int process_made);
struct pdu_udp generate_pdu_udp(struct cfg user_cfg, int pdu_type, char random_number[], char data[]);
void generate_pdu_udp_array(struct pdu_udp pdu, unsigned char pdu_package[], int array_size);
void copyElements(char *src, char *dest, int start, int numElements);
void alive_phase(int sockfd, int status, struct cfg user_cfg, struct sockaddr_in server_address, struct pdu_udp received_reg_pdu, int debug, char *boot_name, int process_made);
struct pdu_udp unpack_pdu_udp(char pdu_package[]);
char *extractElements(char *src, int start, int numElements);
int check_equal_pdu_udp(struct pdu_udp pdu1, struct pdu_udp pdu2);
char *commands(int command);
int known_command(char command[]);
void command_phase(struct cfg user_config, char *command, struct pdu_udp received_reg_pdu, struct sockaddr_in server_address, char *boot_name, int debug);
void send_cfg(struct cfg user_config, struct pdu_udp received_reg_pdu, struct sockaddr_in server_address, char *boot_name, int debug);
long int get_file_size(const char *filename);
char *search_arg(int argc, char *argv[], char *option, char *name);
void send_file_by_lines(int sockfd, struct cfg user_config, struct pdu_udp received_reg_pdu, struct sockaddr_in server_address, char *boot_name);
struct pdu_tcp generate_pdu_tcp(struct cfg user_cfg, int pdu_type, char random_number[], char data[]);
void get_cfg(struct cfg user_config, struct pdu_udp received_reg_pdu, struct sockaddr_in server_address, char *boot_name);
void get_file_by_lines(int sockfd, struct cfg user_config, struct pdu_udp received_reg_pdu, struct sockaddr_in server_address, char *boot_name);
void generate_pdu_tcp_array(struct pdu_tcp pdu, unsigned char pdu_package[], int array_size);
struct pdu_tcp unpack_pdu_tcp(char pdu_package[]);

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
char *commands(int command)
{
    switch (command)
    {
    case REGISTER_REQ:
        return "REGISTER_REQ";
        break;
    case REGISTER_ACK:
        return "REGISTER_ACK";
        break;
    case REGISTER_NACK:
        return "REGISTER_NACK";
        break;
    case REGISTER_REJ:
        return "REGISTER_REJ";
        break;
    case ERROR:
        return "ERROR";
        break;
    case ALIVE_INF:
        return "ALIVE_INF";
        break;
    case ALIVE_ACK:
        return "ALIVE_ACK";
        break;
    case ALIVE_NACK:
        return "ALIVE_NACK";
        break;
    case ALIVE_REJ:
        return "ALIVE_REJ";
        break;
    case SEND_FILE:
        return "SEND_FILE";
        break;
    case SEND_DATA:
        return "SEND_DATA";
        break;
    case SEND_ACK:
        return "SEND_ACK";
        break;
    case SEND_NACK:
        return "SEND_NACK";
        break;
    case SEND_REJ:
        return "SEND_NACK";
        break;
    case SEND_END:
        return "SEND_END";
        break;
    case GET_REJ:
        return "GET_REJ";
        break;
    case GET_END:
        return "GET_END";
        break;
    }
    return "";
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
//
// ESTABLISH CONNECTION WITH THE SERVER AND REGISTER PHASE
//
void connection_phase(int status, struct cfg user_cfg, int debug, char *boot_name, char random_number[7], int process_made)
{
    int sockfd;
    struct sockaddr_in client_address, server_address;
    struct hostent *ent;
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

    // Set the server port and address to be able to send the packages
    ent = gethostbyname((char *)user_cfg.nms_id);
    if (!ent)
    {
        printf("Error! No trobat: %s \n", user_cfg.nms_id);
        exit(-1);
    }

    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    // memcpy(&server_address.sin_addr, ent->h_addr_list[0], ent->h_length);
    server_address.sin_addr.s_addr = (((struct in_addr *)ent->h_addr_list[0])->s_addr);
    server_address.sin_port = htons(atoi((const char *)user_cfg.nms_udp_port));

    // Create PDU REGISTER_REQ package
    struct pdu_udp pdu_reg_req = generate_pdu_udp(user_cfg, REGISTER_REQ, random_number, "");
    unsigned char pdu_package[UDP_PKG_SIZE];
    // generate_pdu_udp_array(pdu_reg_req, pdu_package, UDP_PKG_SIZE);
    int interval = T;
    int packets_sent = 0;
    // int process_made = 1;
    status = WAIT_REG_RESPONSE;
    while (status != REGISTERED && status != DISCONNECTED && process_made <= O)
    {
        if (debug == 1)
        {
            show_status("DEBUG =>  Registre equip, intent:  ", -1);
            printf("%d\n", process_made);
        }
        generate_pdu_udp_array(pdu_reg_req, pdu_package, UDP_PKG_SIZE);
        // Send register package to the server
        if (sendto(sockfd, pdu_package, UDP_PKG_SIZE, 0, (struct sockaddr *)&server_address, sizeof(server_address)) < 0)
        {
            perror("sendto() failed");
            exit(-1);
        }
        if (debug == 1)
        {
            show_status(" DEBUG =>  Enviat: ", -1);
            printf("bytes=%d, comanda=%s id=%s, mac=%s, alea=%s  dades=%s\n", UDP_PKG_SIZE, commands(pdu_reg_req.pdu_type), pdu_reg_req.system_id, pdu_reg_req.mac_address, pdu_reg_req.random_number, pdu_reg_req.data);
        }
        show_status("MSG.  =>  Client passa a l'estat:", status);

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
                if (process_made <= O)
                {
                    sleep(U); // wait U seconds and restart a new registration process
                    if (debug == 1)
                    {
                        show_status("INFO  =>  Fallida registre amb servidor: localhost\n", -1);
                        show_status("DEBUG =>  Registre equip, intent: ", -1);
                        printf("%d\n", process_made);
                    }

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
            int has_received_pkg = recvfrom(sockfd, pdu_package, UDP_PKG_SIZE, 0, (struct sockaddr *)&server_address, &(socklen_t){sizeof(server_address)});
            if (has_received_pkg < 0)
            {
                perror("recvfrom() failed");
                exit(-1);
            }
            pdu_package[has_received_pkg] = '\0';
            struct pdu_udp received_reg_pdu = unpack_pdu_udp((char *)pdu_package);
            if (debug == 1)
            {
                show_status(" DEBUG =>  Rebut: ", -1);
                printf("bytes=%d, comanda=%s, id=%s, mac=%s, alea=%s  dades=%s\n", UDP_PKG_SIZE, commands(received_reg_pdu.pdu_type), received_reg_pdu.system_id, received_reg_pdu.mac_address, received_reg_pdu.random_number, received_reg_pdu.data);
            }
            switch (pdu_package[0])
            {
            case REGISTER_ACK: // 0x02
                status = REGISTERED;
                show_status("MSG.  =>  Equip passa a l'estat: ", status);
                show_status("INFO  =>  Acceptada subscripció amb servidor: ", -1);
                printf("%s\n \t(id: %s, mac: %s, alea:%s, port tcp: %s)\n", user_cfg.nms_id, received_reg_pdu.system_id, received_reg_pdu.mac_address, received_reg_pdu.random_number, user_cfg.nms_udp_port);

                alive_phase(sockfd, status, user_cfg, server_address, received_reg_pdu, debug, boot_name, process_made);
                break;
            case REGISTER_NACK: // 0x04

                if (debug == 1)
                {
                    show_status("INFO  =>  Petició de registre errònia, motiu:", -1);
                    printf("%s\n", received_reg_pdu.data);
                    show_status("INFO  =>  Fallida registre amb servidor: localhost\n", -1);
                }

                sleep(U); // wait U seconds and restart a new registration process
                packets_sent = 0;
                interval = T;
                status = WAIT_REG_RESPONSE;
                process_made += 1;
                break;
            case REGISTER_REJ: // 0x06
                status = DISCONNECTED;
                char data[50];
                memcpy(data, &pdu_package[28], 50);
                show_status("INFO  =>  Petició de registre rebutjada, motiu: ", -1);
                printf("%s\n", data);
                show_status("INFO  =>  Registre rebutjat per el servidor: ", -1);
                printf("%s\n", user_cfg.nms_id);
                break;
            case ERROR: // 0x0F
                // process_made += 1;
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
struct pdu_udp generate_pdu_udp(struct cfg user_cfg, int pdu_type, char random_number[], char data[])
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
void generate_pdu_udp_array(struct pdu_udp pdu, unsigned char pdu_package[], int array_size)
{
    pdu_package[0] = pdu.pdu_type;
    strcpy((char *)&pdu_package[1], (const char *)pdu.system_id);
    strcpy((char *)&pdu_package[1 + 7], (const char *)pdu.mac_address);
    strcpy((char *)&pdu_package[1 + 7 + 13], (const char *)pdu.random_number);
    strcpy((char *)&pdu_package[1 + 7 + 13 + 7], (const char *)pdu.data);
}
struct pdu_udp unpack_pdu_udp(char pdu_package[])
{
    struct pdu_udp pdu;
    memset(&pdu, 0, sizeof(pdu)); // To ensure pdu structure is all initialized with zero
    pdu.pdu_type = pdu_package[0];
    strcpy((char *)pdu.system_id, extractElements(pdu_package, 1, 6));
    strcpy((char *)pdu.mac_address, extractElements(pdu_package, 1 + 7, 12));
    strcpy((char *)pdu.random_number, extractElements(pdu_package, 1 + 7 + 13, 6));
    strcpy((char *)pdu.data, extractElements(pdu_package, 1 + 7 + 13 + 7, 49));
    return pdu;
}
struct pdu_tcp generate_pdu_tcp(struct cfg user_cfg, int pdu_type, char random_number[], char data[])
{
    struct pdu_tcp pdu;
    memset(&pdu, 0, sizeof(pdu)); // To ensure pdu structure is all initialized with zero
    pdu.pdu_type = pdu_type;
    strcpy((char *)pdu.system_id, (const char *)user_cfg.id);
    strcpy((char *)pdu.mac_address, (const char *)user_cfg.mac);
    strcpy((char *)pdu.random_number, (const char *)random_number);
    strcpy((char *)pdu.data, (const char *)data);
    return pdu;
}
void generate_pdu_tcp_array(struct pdu_tcp pdu, unsigned char pdu_package[], int array_size)
{
    pdu_package[0] = pdu.pdu_type;
    strcpy((char *)&pdu_package[1], (const char *)pdu.system_id);
    strcpy((char *)&pdu_package[1 + 7], (const char *)pdu.mac_address);
    strcpy((char *)&pdu_package[1 + 7 + 13], (const char *)pdu.random_number);
    strcpy((char *)&pdu_package[1 + 7 + 13 + 7], (const char *)pdu.data);
}
struct pdu_tcp unpack_pdu_tcp(char pdu_package[])
{
    struct pdu_tcp pdu;
    memset(&pdu, 0, sizeof(pdu)); // To ensure pdu structure is all initialized with zero
    pdu.pdu_type = pdu_package[0];
    strcpy((char *)pdu.system_id, extractElements(pdu_package, 1, 6));
    strcpy((char *)pdu.mac_address, extractElements(pdu_package, 1 + 7, 12));
    strcpy((char *)pdu.random_number, extractElements(pdu_package, 1 + 7 + 13, 6));
    strcpy((char *)pdu.data, extractElements(pdu_package, 1 + 7 + 13 + 7, 149));
    return pdu;
}
int check_equal_pdu_udp(struct pdu_udp pdu1, struct pdu_udp pdu2)
{
    int equal = 1; // assume the PDUs are equal until proven otherwise
    equal &= strcmp(pdu1.system_id, pdu2.system_id) == 0;
    equal &= strcmp(pdu1.mac_address, pdu2.mac_address) == 0;
    equal &= strcmp(pdu1.random_number, pdu2.random_number) == 0;
    return equal;
}

void alive_phase(int sockfd, int status, struct cfg user_cfg, struct sockaddr_in server_address, struct pdu_udp received_reg_pdu, int debug, char *boot_name, int process_made)
{
    int flags = fcntl(sockfd, F_GETFL, 0);
    fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);
    if (debug == 1)
    {
        show_status("DEBUG =>  Creat procés per gestionar alives\n", -1);
        show_status("DEBUG =>  Establert temporitzador per enviament alives\n", -1);
    }

    time_t current_time, last_sent_pkg_time;
    // Create PDU ALIVE_INF package
    struct pdu_udp pdu_alive_inf = generate_pdu_udp(user_cfg, ALIVE_INF, received_reg_pdu.random_number, "");
    unsigned char pdu_package[UDP_PKG_SIZE];
    generate_pdu_udp_array(pdu_alive_inf, pdu_package, UDP_PKG_SIZE);

    int non_confirmated_alives = 0;

    while (status != DISCONNECTED && non_confirmated_alives < S)
    {
        struct timeval timeout = {R, 0};
        if (sendto(sockfd, pdu_package, UDP_PKG_SIZE, 0, (struct sockaddr *)&server_address, sizeof(server_address)) < 0)
        {
            perror("sendto() failed");
            exit(-1);
        }

        last_sent_pkg_time = time(NULL);
        if (debug == 1)
        {
            show_status(" DEBUG =>  Enviat: ", -1);
            printf("bytes=%d, comanda=%s id=%s, mac=%s, alea=%s  dades=%s\n", UDP_PKG_SIZE, commands(pdu_alive_inf.pdu_type), pdu_alive_inf.system_id, pdu_alive_inf.mac_address, pdu_alive_inf.random_number, pdu_alive_inf.data);
        }

        fd_set read_fds;
        FD_ZERO(&read_fds);   // clears the file descriptor set read_fds and initializes it to the empty set.
        FD_SET(0, &read_fds); // Adds the stdin file descriptor to the read_fds set

        int select_status = select(FD_SETSIZE, &read_fds, NULL, NULL, &timeout); // FD_SETSIZE to ensure that all file descriptors are examined.
        if (select_status == -1)
        {
            perror("select() failed");
            exit(-1);
        }
        else if (select_status != 0) // Command was detected in fd0(stdin)
        {
            char command[10];
            fgets(command, 10, stdin);           // Reading what the user entered from the fd0
            command[strcspn(command, "\n")] = 0; // Deleting the \n that fgets function sets by default at the end of the array where input was written
            if (known_command(command))
            {
                command_phase(user_cfg, command, received_reg_pdu, server_address, boot_name, debug); // tcp_phase
            }
            else
            {
                show_status("MSG  => Comanda incorrecta\n", -1); // Alive phase continues because an uknown command was entered
            }
        }
        else //// Timeout occurred
        {
            unsigned char pdu_received_alive[UDP_PKG_SIZE] = {"\n"};
            int has_received_pkg = recvfrom(sockfd, pdu_received_alive, UDP_PKG_SIZE, 0, (struct sockaddr *)&server_address, &(socklen_t){sizeof(server_address)});
            if (has_received_pkg < 0)
            {
                non_confirmated_alives += 1;
            }
            else
            {
                struct pdu_udp received_alive_pdu = unpack_pdu_udp((char *)pdu_received_alive);
                if (debug == 1)
                {
                    show_status(" DEBUG =>  Rebut: ", -1);
                    printf("bytes=%d, comanda=%s, id=%s, mac=%s, alea=%s  dades=%s\n", UDP_PKG_SIZE, commands(received_alive_pdu.pdu_type), received_alive_pdu.system_id, received_alive_pdu.mac_address, received_alive_pdu.random_number, received_alive_pdu.data);
                }
                if (check_equal_pdu_udp(received_alive_pdu, received_reg_pdu) == 0)
                {
                    show_status("INFO  =>  Error recepció paquet UDP. Dades servidor incorrecte", -1);
                    printf("\n \t(correcte: id= %s, mac= %s, alea=%s)\n", received_reg_pdu.system_id, received_reg_pdu.mac_address, received_reg_pdu.random_number);
                    non_confirmated_alives += 1;
                    printf("NON %d\n", non_confirmated_alives);
                }
                else
                {
                    switch (pdu_received_alive[0])
                    {
                    case ALIVE_ACK:
                        non_confirmated_alives = 0;
                        if (status == REGISTERED)
                        {
                            status = SEND_ALIVE;
                            show_status("MSG.  =>  Equip passa a l'estat: ", status);
                        }
                        if (check_equal_pdu_udp(received_alive_pdu, received_reg_pdu) != 0)
                        {
                            show_status("INFO  =>  Acceptat ALIVE ", -1);
                            printf("(Servidor id: %s, mac: %s, alea:%s)\n", received_alive_pdu.system_id, received_alive_pdu.mac_address, received_alive_pdu.random_number);
                        }
                        else
                        {
                            show_status("INFO  =>  Denegada subscripció amb servidor: ", -1);
                            printf("%s\n \t(id: %s, mac: %s, alea:%s, port tcp: %s)\n", user_cfg.nms_id, received_alive_pdu.system_id, received_alive_pdu.mac_address, received_alive_pdu.random_number, user_cfg.nms_udp_port);
                        }
                        break;
                    case ALIVE_REJ:
                        if (status == SEND_ALIVE)
                        {
                            printf("ALIVE_REJ\n");
                            status = DISCONNECTED;
                            connection_phase(status, user_cfg, debug, boot_name, received_reg_pdu.random_number, 2);
                        }

                    case ALIVE_NACK:
                        show_status("INFO => ALIVE_NACK rebut, es considera com no haver rebut resposta del servidor\n", -1);
                        non_confirmated_alives += 1;
                        break;
                    }
                }
            }
        }

        current_time = time(NULL);
        int sleep_time = R - (current_time - last_sent_pkg_time);
        if (sleep_time < 0)
        { // if the sleep calc is less than 0, then sleep() waits undefinitely which may occur in some cases
            sleep_time = 0;
        }
        sleep(sleep_time); // Calculate if select() already executed the Rs timeout
    }
    // After O sent ALIVE_INF without server response(ALIVE_ACK), REGISTER proccess is reseted
    status = DISCONNECTED;
    show_status("MSG.  =>  Equip passa a l'estat: DISCONNECTED (Sense resposta a 3 ALIVES)\n", -1);
    if (debug == 1)
    {
        show_status(" DEBUG =>  Cancelat temporitzador per enviament alives\n", -1);
        show_status("DEBUG =>  Finalitzat procés per gestionar alives\n", -1);
    }
    process_made += 1;
    connection_phase(status, user_cfg, debug, boot_name, received_reg_pdu.random_number, process_made);
}
char *extractElements(char *src, int start, int numElements)
{
    char *dest = malloc(numElements + 1); // +1 for the null terminator
    int i;
    for (i = 0; i < numElements; i++)
    {
        dest[i] = src[start + i];
    }
    dest[numElements] = '\0'; // add the null terminator
    return dest;
}
int known_command(char command[])
{
    return strcmp((const char *)command, "send-cfg") == 0 || strcmp((const char *)command, "get-cfg") == 0 || strcmp((const char *)command, "quit") == 0;
}

//
// COMMAND EXECUTION PHASE
//
void command_phase(struct cfg user_config, char *command, struct pdu_udp received_reg_pdu, struct sockaddr_in server_address, char *boot_name, int debug)
{
    if (strcmp((const char *)command, "send-cfg") == 0) // strcmp returns 0 if both strings are equal
    {
        if (debug == 1)
        {
            show_status("DEBUG =>  Creat procés per gestionar comanda sobre arxiu configuració ", -1);
        }

        show_status("MSG.  =>  Sol·licitud d'enviament d'arxiu de configuració al servidor ", -1);
        printf("(%s)\n", boot_name);
        send_cfg(user_config, received_reg_pdu, server_address, boot_name, debug);
    }
    else if (strcmp((const char *)command, "get-cfg") == 0)
    {
        show_status("MSG.  =>  Sol·licitud de recepció d'arxiu de configuració del servidor ", -1);
        printf("(%s)\n", user_config.id);
        get_cfg(user_config, received_reg_pdu, server_address, boot_name);
    }
    else
    {
        exit(0); // Client finished due to quit command
    }
}

void send_cfg(struct cfg user_config, struct pdu_udp received_reg_pdu, struct sockaddr_in server_address, char *boot_name, int debug)
{
    int sockfd_tcp;

    // socket create and verification
    sockfd_tcp = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd_tcp == -1)
    {
        printf("socket creation failed...\n");
        exit(0);
    }

    server_address.sin_port = htons(atoi(received_reg_pdu.data)); // Assign the port received from the REGISTER_ACK to the sockaddr_in for tcp file sharing

    if (connect(sockfd_tcp, (struct sockaddr *)&server_address, sizeof(server_address)) != 0) // connect the client socket to server socket
    {
        perror("connect() failed:");
        exit(-1);
    }
    long int file_size = get_file_size(boot_name);
    if (file_size == -1)
    {
        printf("Error: Could not open file.\n");
    }
    char data[150];
    unsigned char pdu_package[TCP_PKG_SIZE];
    snprintf(data, sizeof(data), "%s,%ld", "boot.cfg", file_size);
    struct pdu_tcp try_send_file_pdu = generate_pdu_tcp(user_config, SEND_FILE, received_reg_pdu.random_number, data);
    generate_pdu_tcp_array(try_send_file_pdu, pdu_package, TCP_PKG_SIZE);
    fd_set read_fds;
    FD_ZERO(&read_fds);            // clears the file descriptor set read_fds and initializes it to the empty set.
    FD_SET(sockfd_tcp, &read_fds); // Adds the sockfd file descriptor to the read_fds set
    struct timeval timeout = {W, 0};
    send(sockfd_tcp, pdu_package, sizeof(pdu_package), 0);
    int select_status = select(sockfd_tcp + 1, &read_fds, NULL, NULL, &timeout);
    if (select_status == -1)
    {
        perror("select() failed");
        exit(-1);
    }
    else if (select_status == 0) // Timeout occurred, we consider the communication with the server isn't working properly
    {
        close(sockfd_tcp);
        // alive_phase(sockfd_udp, REGISTERED, user_config, server_address, received_reg_pdu, debug, boot_name, 1);
    }
    else
    {
        int has_received_pkg = recv(sockfd_tcp, pdu_package, sizeof(pdu_package), 0);
        if (has_received_pkg < 0)
        {
            perror("recvfrom() failed");
            exit(-1);
        }
        pdu_package[has_received_pkg] = '\0';
        struct pdu_tcp serv_response = unpack_pdu_tcp((char *)pdu_package);
        if (serv_response.pdu_type == SEND_ACK)
        {
            printf("send file\n");
            send_file_by_lines(sockfd_tcp, user_config, received_reg_pdu, server_address, boot_name);
        }

        close(sockfd_tcp);
        show_status("MSG.  =>  Finalitzat enviament d'arxiu de configuració al servidor ", -1);
        printf("(%s)\n", boot_name);
    }
}
long int get_file_size(const char *filename)
{
    FILE *file = fopen(filename, "rb");
    if (file == NULL)
    {
        return -1;
    }
    fseek(file, 0, SEEK_END);
    long int size = ftell(file);
    fclose(file);
    return size;
}
void send_file_by_lines(int sockfd, struct cfg user_config, struct pdu_udp received_reg_pdu, struct sockaddr_in server_address, char *boot_name)
{
    FILE *fp;
    char line[150];

    fp = fopen(boot_name, "r");
    if (fp == NULL)
    {
        perror("Error opening file");
        exit(-1);
    }

    struct pdu_tcp pdu_line;
    char pdu_package[TCP_PKG_SIZE];

    while (fgets(line, 150, fp) != NULL)
    {
        // line[strcspn(line, "\n")] = '\0'; // Remove the newline character from the end of the line
        pdu_line = generate_pdu_tcp(user_config, SEND_DATA, received_reg_pdu.random_number, line);
        generate_pdu_tcp_array(pdu_line, (unsigned char *)pdu_package, TCP_PKG_SIZE);
        send(sockfd, pdu_package, sizeof(pdu_package), 0);
        show_status(" DEBUG =>  Enviat: ", -1);
        printf("bytes=%d, comanda=%s id=%s, mac=%s, alea=%s  dades=%s\n\n", TCP_PKG_SIZE, commands(pdu_line.pdu_type), pdu_line.system_id, pdu_line.mac_address, pdu_line.random_number, pdu_line.data);
    }
    pdu_line = generate_pdu_tcp(user_config, SEND_END, received_reg_pdu.random_number, "");
    generate_pdu_tcp_array(pdu_line, (unsigned char *)pdu_package, TCP_PKG_SIZE);
    send(sockfd, pdu_package, sizeof(pdu_package), 0);
    show_status(" DEBUG =>  Enviat: ", -1);
    printf("bytes=%d, comanda=%s id=%s, mac=%s, alea=%s  dades=%s\n\n", TCP_PKG_SIZE, commands(pdu_line.pdu_type), pdu_line.system_id, pdu_line.mac_address, pdu_line.random_number, pdu_line.data);

    close(sockfd);
    // Close the file send-cfg
    fclose(fp);
}
void get_cfg(struct cfg user_config, struct pdu_udp received_reg_pdu, struct sockaddr_in server_address, char *boot_name)
{
    int sockfd;

    // socket create and verification
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1)
    {
        printf("socket creation failed...\n");
        exit(0);
    }
    else
        printf("Socket successfully created..\n");

    server_address.sin_port = htons(atoi(received_reg_pdu.data)); // Assign the port received from the REGISTER_ACK to the sockaddr_in for tcp file sharing

    if (connect(sockfd, (struct sockaddr *)&server_address, sizeof(server_address)) != 0) // connect the client socket to server socket
    {
        perror("connect() failed:");
        exit(-1);
    }
    long int file_size = get_file_size(boot_name);
    if (file_size == -1)
    {
        printf("Error: Could not open file.\n");
    }
    char data[150];
    unsigned char pdu_package[TCP_PKG_SIZE];
    snprintf(data, sizeof(data), "%s,%ld", boot_name, file_size);
    struct pdu_tcp try_send_file_pdu = generate_pdu_tcp(user_config, GET_FILE, received_reg_pdu.random_number, data);
    generate_pdu_tcp_array(try_send_file_pdu, pdu_package, TCP_PKG_SIZE);
    fd_set read_fds;
    FD_ZERO(&read_fds);        // clears the file descriptor set read_fds and initializes it to the empty set.
    FD_SET(sockfd, &read_fds); // Adds the sockfd file descriptor to the read_fds set
    struct timeval timeout = {W, 0};
    send(sockfd, pdu_package, sizeof(pdu_package), 0);
    int select_status = select(sockfd + 1, &read_fds, NULL, NULL, &timeout);
    if (select_status == -1)
    {
        perror("select() failed");
        exit(-1);
    }
    else if (select_status == 0) // Timeout occurred, we consider the communication with the server isn't working properly
    {
        printf("TIMEOUT OCCURRED\n");
        close(sockfd);
    }
    else
    {
        int has_received_pkg = recv(sockfd, pdu_package, sizeof(pdu_package), 0);
        if (has_received_pkg < 0)
        {
            perror("recvfrom() failed");
            exit(-1);
        }
        pdu_package[has_received_pkg] = '\0';
        struct pdu_tcp serv_response = unpack_pdu_tcp((char *)pdu_package);
        if (serv_response.pdu_type == GET_ACK)
        {
            printf("send file\n");
            get_file_by_lines(sockfd, user_config, received_reg_pdu, server_address, boot_name);
        }

        close(sockfd);
    }
    show_status("MSG.  =>  Finalitzada l'obtenció arxiu de configuració del servidor ", -1);
    printf("(%s)\n", user_config.id);
}
void get_file_by_lines(int sockfd, struct cfg user_config, struct pdu_udp received_reg_pdu, struct sockaddr_in server_address, char *boot_name)
{
    FILE *fp;
    fp = fopen(boot_name, "w");
    if (fp == NULL)
    {
        perror("Error opening file");
        exit(-1);
    }

    char pdu_package[TCP_PKG_SIZE];
    int has_received_pkg;
    struct pdu_tcp pdu_line;
    struct timeval timeout = {W, 0};
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(sockfd, &read_fds);
    while (pdu_line.pdu_type != GET_END)
    {
        int ready = select(sockfd + 1, &read_fds, NULL, NULL, &timeout);
        if (ready < 0)
        {
            perror("select() failed");
            exit(-1);
        }
        else if (ready == 0)
        {
            close(sockfd);
            printf("Timeout: server didn't answer within 3 seconds\n");
        }
        else
        {
            has_received_pkg = recv(sockfd, pdu_package, sizeof(pdu_package), 0);
            pdu_line = unpack_pdu_tcp((char *)pdu_package);
            show_status(" DEBUG =>  Rebut: ", -1);
            printf("bytes=%d, comanda=%s id=%s, mac=%s, alea=%s  dades=%s\n\n", TCP_PKG_SIZE, commands(pdu_line.pdu_type), pdu_line.system_id, pdu_line.mac_address, pdu_line.random_number, pdu_line.data);

            if (has_received_pkg < 0)
            {
                perror("recvfrom() failed");
                exit(-1);
            }

            fprintf(fp, "%s", pdu_line.data);
        }
    }

    close(sockfd);
    // Close the file send-cfg get-cfg
    fclose(fp);
}