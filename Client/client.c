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
// DECLARATION
void get_client_cfg(char *file_name);
char *read_line(char line[], FILE *file);
void show_status(char text[], int statuss);
void connection_phase(char random_number[7]);
struct pdu_udp generate_pdu_udp(int pdu_type, char random_number[], char data[]);
void generate_pdu_udp_array(struct pdu_udp pdu, unsigned char pdu_package[], int array_size);
void copyElements(char *src, char *dest, int start, int numElements);
void alive_phase();
struct pdu_udp unpack_pdu_udp(char pdu_package[]);
char *extractElements(char *src, int start, int numElements);
int check_equal_pdu_udp(struct pdu_udp pdu1, struct pdu_udp pdu2);
int check_equal_pdu_tcp(struct pdu_tcp pdu1, struct pdu_udp pdu2);
char *commands(int command);
void *command_phase();
int known_command(char command[]);
void send_cfg();
long int get_file_size(const char *filename);
char *search_arg(int argc, char *argv[], char *option, char *name);
void send_file_by_lines();
struct pdu_tcp generate_pdu_tcp(int pdu_type, char random_number[], char data[]);
void get_cfg();
int get_file_by_lines();
void generate_pdu_tcp_array(struct pdu_tcp pdu, unsigned char pdu_package[], int array_size);
struct pdu_tcp unpack_pdu_tcp(char pdu_package[]);
void *send_alive_inf();
void connect_tcp_socket();

struct cfg user_config;
struct pdu_udp received_reg_pdu, received_alive_pdu;
struct sockaddr_in client_address, server_address_udp, server_address_tcp;
char *boot_name, *file_name;
int debug, status, sockfd_udp = -1, sockfd_tcp = -1, registration_attempt = 1;
pthread_t thread_command, thread_send_alive;

int main(int argc, char *argv[])
{
    status = DISCONNECTED;
    debug = atoi(search_arg(argc, argv, "-d", "0"));
    boot_name = search_arg(argc, argv, "-f", "boot.cfg");
    file_name = search_arg(argc, argv, "-c", "client.cfg");
    get_client_cfg(file_name);
    if (debug == 1)
    {
        show_status("DEBUG =>  Llegits paràmetres línia de comandes\n", -1);
        show_status("DEBUG =>  Llegits parametres arxius de configuració\n", -1);
        show_status("DEBUG =>  Inici bucle de servei equip : ", -1);
        printf("%s\n", user_config.id);
    }
    show_status("MSG.  =>  Equip passa a l'estat:", status);
    connection_phase("000000");
}

void show_status(char text[], int statuss)
{
    time_t current_time;
    struct tm *time_info;
    time(&current_time);
    time_info = localtime(&current_time);
    printf("%02d:%02d:%02d %s", time_info->tm_hour, time_info->tm_min, time_info->tm_sec, text);
    switch (statuss)
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
void get_client_cfg(char *file_name)
{
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
            strncpy((char *)user_config.id, (char *)parsed_line, sizeof(user_config.id));
            break;
        case 1:
            strncpy((char *)user_config.mac, (char *)parsed_line, sizeof(user_config.mac));
            break;
        case 2:
            strncpy((char *)user_config.nms_id, (char *)parsed_line, sizeof(user_config.nms_id));
            break;
        case 3:
            strncpy((char *)user_config.nms_udp_port, (char *)parsed_line, sizeof(user_config.nms_udp_port));
            break;
        }
        // printf("%s\n", parsed_line);
    }
    fclose(file);
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
void connection_phase(char random_number[7])
{

    struct hostent *ent;
    // Create a UDP socket, open a communication channel
    sockfd_udp = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd_udp < 0)
    {
        perror("socket() failed");
        exit(-1);
    }

    // Bind client's channel to a port and address
    memset(&client_address, 0, sizeof(client_address));
    client_address.sin_family = AF_INET;
    client_address.sin_addr.s_addr = htonl(0);
    client_address.sin_port = htons(0);

    if (bind(sockfd_udp, (struct sockaddr *)&client_address, sizeof(client_address)) != 0)
    {
        perror("bind() failed");
        exit(-1);
    }

    // Set the server address to be able to send the packages
    ent = gethostbyname((char *)user_config.nms_id);
    if (!ent)
    {
        printf("Error! No trobat: %s \n", user_config.nms_id);
        exit(-1);
    }

    memset(&server_address_udp, 0, sizeof(server_address_udp));
    server_address_udp.sin_family = AF_INET;
    // memcpy(&server_address.sin_addr, ent->h_addr_list[0], ent->h_length);
    server_address_udp.sin_addr.s_addr = (((struct in_addr *)ent->h_addr_list[0])->s_addr);
    server_address_udp.sin_port = htons(atoi((const char *)user_config.nms_udp_port));

    // Create PDU REGISTER_REQ package
    struct pdu_udp pdu_reg_req = generate_pdu_udp(REGISTER_REQ, random_number, "");
    unsigned char pdu_package[UDP_PKG_SIZE];
    // generate_pdu_udp_array(pdu_reg_req, pdu_package, UDP_PKG_SIZE);
    int interval = T;
    int packets_sent = 0;
    // int process_made = 1;
    status = WAIT_REG_RESPONSE;
    while (status != REGISTERED && status != DISCONNECTED && registration_attempt <= O)
    {
        if (packets_sent == 0 && debug == 1)
        {
            show_status("DEBUG =>  Registre equip, intent:  ", -1);
            printf("%d\n", registration_attempt);
        }
        generate_pdu_udp_array(pdu_reg_req, pdu_package, UDP_PKG_SIZE);
        // Send register package to the server
        if (sendto(sockfd_udp, pdu_package, UDP_PKG_SIZE, 0, (struct sockaddr *)&server_address_udp, sizeof(server_address_udp)) < 0)
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
        FD_ZERO(&read_fds);            // clears the file descriptor set read_fds and initializes it to the empty set.
        FD_SET(sockfd_udp, &read_fds); // Adds the sockfd file descriptor to the read_fds set
        struct timeval timeout = {interval, 0};
        int select_status = select(sockfd_udp + 1, &read_fds, NULL, NULL, &timeout);
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
                registration_attempt += 1;
                if (debug == 1)
                {
                    show_status("INFO  =>  Fallida registre amb servidor: localhost\n", -1);
                    // show_status("DEBUG =>  Registre equip, intent: ", -1);
                    // printf("%d\n", process_made);
                }

                // Restart the registration process
                packets_sent = 0;
                interval = T;
                status = WAIT_REG_RESPONSE;
                if (registration_attempt <= O)
                {
                    sleep(U); // wait U seconds and restart a new registration process
                }
            }
        }
        else // Response received
        {
            int has_received_pkg = recvfrom(sockfd_udp, pdu_package, UDP_PKG_SIZE, 0, (struct sockaddr *)&server_address_udp, &(socklen_t){sizeof(server_address_udp)});
            if (has_received_pkg < 0)
            {
                perror("recvfrom() failed");
                exit(-1);
            }
            pdu_package[has_received_pkg] = '\0';
            received_reg_pdu = unpack_pdu_udp((char *)pdu_package);
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
                printf("%s\n \t(id: %s, mac: %s, alea:%s, port tcp: %s)\n", user_config.nms_id, received_reg_pdu.system_id, received_reg_pdu.mac_address, received_reg_pdu.random_number, user_config.nms_udp_port);

                alive_phase(sockfd_udp, status, user_config, server_address_udp, received_reg_pdu, debug, boot_name);
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
                registration_attempt += 1;
                break;
            case REGISTER_REJ: // 0x06
                status = DISCONNECTED;
                char data[50];
                memcpy(data, &pdu_package[28], 50);
                show_status("INFO  =>  Petició de registre rebutjada, motiu: ", -1);
                printf("%s\n", data);
                show_status("INFO  =>  Registre rebutjat per el servidor: ", -1);
                printf("%s\n", user_config.nms_id);
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
struct pdu_udp generate_pdu_udp(int pdu_type, char random_number[], char data[])
{
    struct pdu_udp pdu;
    memset(&pdu, 0, sizeof(pdu)); // To ensure pdu structure is all initialized with zero
    pdu.pdu_type = pdu_type;
    strcpy((char *)pdu.system_id, (const char *)user_config.id);
    strcpy((char *)pdu.mac_address, (const char *)user_config.mac);
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
struct pdu_tcp generate_pdu_tcp(int pdu_type, char random_number[], char data[])
{
    struct pdu_tcp pdu;
    memset(&pdu, 0, sizeof(pdu)); // To ensure pdu structure is all initialized with zero
    pdu.pdu_type = pdu_type;
    strcpy((char *)pdu.system_id, (const char *)user_config.id);
    strcpy((char *)pdu.mac_address, (const char *)user_config.mac);
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
int check_equal_pdu_tcp(struct pdu_tcp pdu1, struct pdu_udp pdu2)
{
    int equal = 1; // assume the PDUs are equal until proven otherwise
    equal &= strcmp(pdu1.system_id, pdu2.system_id) == 0;
    equal &= strcmp(pdu1.mac_address, pdu2.mac_address) == 0;
    equal &= strcmp(pdu1.random_number, pdu2.random_number) == 0;
    return equal;
}
//
// MAINTAIN CONNECTION WITH THE SERVER AND ATTEND COMMANDS FROM USER CONCURRENTLY
//
void alive_phase()
{

    int flags = fcntl(sockfd_udp, F_GETFL, 0);
    fcntl(sockfd_udp, F_SETFL, flags | O_NONBLOCK);
    pthread_create(&thread_send_alive, NULL, send_alive_inf, NULL);

    int non_confirmated_alives = 0;
    while (status != DISCONNECTED && non_confirmated_alives < S)
    {
        fd_set read_fds;
        FD_ZERO(&read_fds);              // clears the file descriptor set read_fds and initializes it to the empty set.
        FD_SET(sockfd_udp, &read_fds);   // Adds the sockfd file descriptor to the read_fds set
        FD_SET(STDIN_FILENO, &read_fds); // Adds the stdin file descriptor to the read_fds set

        struct timeval timeout = {R, 0};
        int select_status = select(sockfd_udp + 1, &read_fds, NULL, NULL, &timeout);
        if (select_status == -1)
        {
            perror("select() failed");
            exit(-1);
        }
        else if (select_status != 0)
        {
            if (FD_ISSET(sockfd_udp, &read_fds)) // Pkg was detected in sockfd_udp
            {
                unsigned char pdu_received_alive[UDP_PKG_SIZE] = {"\n"};
                int has_received_pkg = recvfrom(sockfd_udp, pdu_received_alive, UDP_PKG_SIZE, 0, (struct sockaddr *)&server_address_udp, &(socklen_t){sizeof(server_address_udp)});
                if (has_received_pkg < 0)
                {
                    non_confirmated_alives += 1;
                }
                else
                {
                    received_alive_pdu = unpack_pdu_udp((char *)pdu_received_alive);
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
                        if (non_confirmated_alives >= S)
                        {
                            registration_attempt += 1;
                        }
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
                                printf("(Servidor id: %s, mac: %s, alea:%s)\n\n", received_alive_pdu.system_id, received_alive_pdu.mac_address, received_alive_pdu.random_number);
                            }
                            else
                            {
                                show_status("INFO  =>  Denegada subscripció amb servidor: ", -1);
                                printf("%s\n \t(id: %s, mac: %s, alea:%s, port tcp: %s)\n", user_config.nms_id, received_alive_pdu.system_id, received_alive_pdu.mac_address, received_alive_pdu.random_number, user_config.nms_udp_port);
                            }
                            break;
                        case ALIVE_REJ:
                            if (status == SEND_ALIVE)
                            {
                                printf("ALIVE_REJ\n");
                                status = DISCONNECTED;
                                pthread_join(thread_send_alive, NULL);
                                connection_phase(received_reg_pdu.random_number);
                            }

                        case ALIVE_NACK:
                            show_status("INFO => ALIVE_NACK rebut, es considera com no haver rebut resposta del servidor\n", -1);
                            non_confirmated_alives += 1;
                            break;
                        }
                    }
                }
            }
            else if (FD_ISSET(STDIN_FILENO, &read_fds)) // Command detected in fd0(stdin)
            {
                // Start a thread to read input from user concurrently meanwhile alive phase mainteinance is being executed

                pthread_create(&thread_command, NULL, command_phase, NULL);
                pthread_join(thread_command, NULL);
            }
        }
        else //// Timeout occurred
        {
            non_confirmated_alives += 1;
            printf("TIMEEEOUT %d\n", non_confirmated_alives);
        }
    }
    // After O sent ALIVE_INF without server response(ALIVE_ACK), REGISTER proccess is reseted
    if (registration_attempt > O)
    {
        show_status("INFO  =>  No es pot contactar amb servidor: localhost\n", -1);
    }
    status = DISCONNECTED;
    show_status("MSG.  =>  Equip passa a l'estat: DISCONNECTED (Sense resposta a 3 ALIVES)\n", -1);
    if (debug == 1)
    {
        show_status(" DEBUG =>  Cancelat temporitzador per enviament alives\n", -1);
        show_status("DEBUG =>  Finalitzat procés per gestionar alives\n", -1);
    }
    pthread_join(thread_send_alive, NULL);
    connection_phase(received_reg_pdu.random_number);
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
void *send_alive_inf()
{

    int flags = fcntl(sockfd_udp, F_GETFL, 0);
    fcntl(sockfd_udp, F_SETFL, flags | O_NONBLOCK);
    if (debug == 1)
    {
        show_status("DEBUG =>  Creat procés per gestionar alives\n", -1);
        show_status("DEBUG =>  Establert temporitzador per enviament alives\n", -1);
    }

    // Create PDU ALIVE_INF package
    struct pdu_udp pdu_alive_inf = generate_pdu_udp(ALIVE_INF, received_reg_pdu.random_number, "");
    unsigned char pdu_package[UDP_PKG_SIZE];
    generate_pdu_udp_array(pdu_alive_inf, pdu_package, UDP_PKG_SIZE);

    while (status != DISCONNECTED)
    {
        if (sendto(sockfd_udp, pdu_package, UDP_PKG_SIZE, 0, (struct sockaddr *)&server_address_udp, sizeof(server_address_udp)) < 0)
        {
            perror("sendto() failed");
            exit(-1);
        }
        if (debug == 1)
        {
            show_status(" DEBUG =>  Enviat: ", -1);
            printf("bytes=%d, comanda=%s id=%s, mac=%s, alea=%s  dades=%s\n", UDP_PKG_SIZE, commands(pdu_alive_inf.pdu_type), pdu_alive_inf.system_id, pdu_alive_inf.mac_address, pdu_alive_inf.random_number, pdu_alive_inf.data);
        }
        sleep(R);
    }
    return NULL;
}
void *command_phase()
{
    char command[10];
    fgets(command, sizeof(command), stdin); // Reading what the user entered from the fd0
    command[strcspn(command, "\n")] = 0;    // Deleting the \n that fgets function sets by default at the end of the array where input was written
    if (known_command(command))
    {
        if (strcmp((const char *)command, "send-cfg") == 0) // strcmp returns 0 if both strings are equal
        {
            if (debug == 1)
            {
                show_status("DEBUG =>  Creat procés per gestionar comanda sobre arxiu configuració ", -1);
            }

            show_status("MSG.  =>  Sol·licitud d'enviament d'arxiu de configuració al servidor ", -1);
            printf("(%s)\n", boot_name);
            connect_tcp_socket();
            send_cfg();
        }
        else if (strcmp((const char *)command, "get-cfg") == 0)
        {
            if (debug == 1)
            {
                show_status("DEBUG =>  Creat procés per gestionar comanda sobre arxiu configuració \n", -1);
            }
            show_status("MSG.  =>  Sol·licitud de recepció d'arxiu de configuració del servidor ", -1);
            printf("(%s)\n", user_config.id);
            connect_tcp_socket();
            get_cfg();
        }
        else
        {
            exit(0); // Client finished due to quit command
        }
    }
    else
    {
        show_status("MSG  => Comanda incorrecta\n", -1); // Alive phase continues because an uknown command was entered
    }

    return NULL;
}
int known_command(char command[])
{
    return strcmp((const char *)command, "send-cfg") == 0 || strcmp((const char *)command, "get-cfg") == 0 || strcmp((const char *)command, "quit") == 0;
}
//
// COMMAND EXECUTION PHASE
//

void send_cfg()
{
    long int file_size = get_file_size(boot_name);
    if (file_size == -1)
    {
        printf("Error: Could not open file.\n");
    }
    char data[150];
    unsigned char pdu_package[TCP_PKG_SIZE];
    snprintf(data, sizeof(data), "%s,%ld", "boot.cfg", file_size);
    struct pdu_tcp try_send_file_pdu = generate_pdu_tcp(SEND_FILE, received_reg_pdu.random_number, data);
    generate_pdu_tcp_array(try_send_file_pdu, pdu_package, TCP_PKG_SIZE);
    fd_set read_fds;
    FD_ZERO(&read_fds);            // clears the file descriptor set read_fds and initializes it to the empty set.
    FD_SET(sockfd_tcp, &read_fds); // Adds the sockfd file descriptor to the read_fds set
    struct timeval timeout = {W, 0};
    send(sockfd_tcp, pdu_package, sizeof(pdu_package), 0);
    if (debug == 1)
    {
        show_status(" DEBUG =>  Enviat: ", -1);
        printf("bytes=%d, comanda=%s id=%s, mac=%s, alea=%s  dades=%s\n", UDP_PKG_SIZE, commands(try_send_file_pdu.pdu_type), try_send_file_pdu.system_id, try_send_file_pdu.mac_address, try_send_file_pdu.random_number, try_send_file_pdu.data);
    }

    int select_status = select(sockfd_tcp + 1, &read_fds, NULL, NULL, &timeout);
    if (select_status == -1)
    {
        perror("select() failed");
        exit(-1);
    }
    else if (select_status == 0) // Timeout occurred, we consider the communication with the server isn't working properly
    {
        show_status("ALERT =>  No s'ha rebut informació per el canal TCP durant 3 segons\n", -1);
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
        if (debug == 1)
        {
            show_status(" DEBUG =>  Rebut: ", -1);
            printf("bytes=%d, comanda=%s id=%s, mac=%s, alea=%s  dades=%s\n", UDP_PKG_SIZE, commands(serv_response.pdu_type), serv_response.system_id, serv_response.mac_address, serv_response.random_number, serv_response.data);
        }
        if (check_equal_pdu_tcp(serv_response, received_reg_pdu))
        {
            printf("send file\n");
            send_file_by_lines();
            show_status("MSG.  =>  Finalitzat enviament d'arxiu de configuració al servidor ", -1);
            printf("(%s)\n", boot_name);
        }
        else
        {
            show_status("INFO  =>  Error en acceptació d'enviament d'arxiu de configuració (dades pdu incorrectes)\n", -1);
        }
    }
    close(sockfd_tcp);
    if (debug == 1)
    {
        show_status("DEBUG =>  Finalitzat procés per gestionar comanda sobre arxiu configuració\n", -1);
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
void send_file_by_lines()
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
        pdu_line = generate_pdu_tcp(SEND_DATA, received_reg_pdu.random_number, line);
        generate_pdu_tcp_array(pdu_line, (unsigned char *)pdu_package, TCP_PKG_SIZE);
        send(sockfd_tcp, pdu_package, sizeof(pdu_package), 0);
        if (debug == 1)
        {
            show_status(" DEBUG =>  Enviat: ", -1);
            printf("bytes=%d, comanda=%s id=%s, mac=%s, alea=%s  dades=%s\n\n", TCP_PKG_SIZE, commands(pdu_line.pdu_type), pdu_line.system_id, pdu_line.mac_address, pdu_line.random_number, pdu_line.data);
        }
    }
    pdu_line = generate_pdu_tcp(SEND_END, received_reg_pdu.random_number, "");
    generate_pdu_tcp_array(pdu_line, (unsigned char *)pdu_package, TCP_PKG_SIZE);
    send(sockfd_tcp, pdu_package, sizeof(pdu_package), 0);
    if (debug == 1)
    {
        show_status(" DEBUG =>  Enviat: ", -1);
        printf("bytes=%d, comanda=%s id=%s, mac=%s, alea=%s  dades=%s\n\n", TCP_PKG_SIZE, commands(pdu_line.pdu_type), pdu_line.system_id, pdu_line.mac_address, pdu_line.random_number, pdu_line.data);
    }
    close(sockfd_tcp);
    // Close the file send-cfg
    fclose(fp);
}
void connect_tcp_socket()
{
    // socket create and verification
    sockfd_tcp = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd_tcp == -1)
    {
        printf("socket creation failed...\n");
        exit(0);
    }
    struct hostent *ent;
    ent = gethostbyname((char *)user_config.nms_id);
    if (!ent)
    {
        printf("Error! No trobat: %s \n", user_config.nms_id);
        exit(-1);
    }
    memset(&server_address_tcp, 0, sizeof(server_address_tcp));
    server_address_tcp.sin_family = AF_INET;
    server_address_tcp.sin_addr.s_addr = (((struct in_addr *)ent->h_addr_list[0])->s_addr);
    server_address_tcp.sin_port = htons(atoi(received_reg_pdu.data)); // Assign the port received from the REGISTER_ACK to the sockaddr_in for tcp file sharing

    if (connect(sockfd_tcp, (struct sockaddr *)&server_address_tcp, sizeof(server_address_tcp)) != 0) // connect the client socket to server socket
    {
        perror("connect() failed:");
        exit(-1);
    }
}
void get_cfg()
{

    long int file_size = get_file_size(boot_name);
    if (file_size == -1)
    {
        printf("Error: Could not open file.\n");
    }
    char data[150];
    int err = 0;
    unsigned char pdu_package[TCP_PKG_SIZE];
    snprintf(data, sizeof(data), "%s,%ld", boot_name, file_size);
    struct pdu_tcp try_send_file_pdu = generate_pdu_tcp(GET_FILE, received_reg_pdu.random_number, data);
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
        show_status("ALERT =>  No s'ha rebut informació per el canal TCP durant 3 segons\n", -1);
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
        if (serv_response.pdu_type == GET_ACK)
        {
            printf("send file\n");
            err = get_file_by_lines();
        }
        if (err == 0)
        {
            show_status("MSG.  =>  Finalitzada l'obtenció arxiu de configuració del servidor ", -1);
            printf("(%s)\n", user_config.id);
        }
    }
    close(sockfd_tcp);
    if (debug == 1)
    {
        show_status("DEBUG =>  Finalitzat procés per gestionar comanda sobre arxiu configuració\n", -1);
    }
}
int get_file_by_lines()
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
    FD_SET(sockfd_tcp, &read_fds);
    while (pdu_line.pdu_type != GET_END)
    {
        int ready = select(sockfd_tcp + 1, &read_fds, NULL, NULL, &timeout);
        if (ready < 0)
        {
            perror("select() failed");
            exit(-1);
        }
        else if (ready == 0)
        {
            close(sockfd_tcp);
            show_status("ALERT =>  No s'ha rebut informació per el canal TCP durant 3 segons\n", -1);
            return -1;
        }
        else
        {
            has_received_pkg = recv(sockfd_tcp, pdu_package, sizeof(pdu_package), 0);
            pdu_line = unpack_pdu_tcp((char *)pdu_package);
            if (debug == 1)
            {
                show_status(" DEBUG =>  Rebut: ", -1);
                printf("bytes=%d, comanda=%s id=%s, mac=%s, alea=%s  dades=%s\n\n", TCP_PKG_SIZE, commands(pdu_line.pdu_type), pdu_line.system_id, pdu_line.mac_address, pdu_line.random_number, pdu_line.data);
            }
            if (has_received_pkg < 0)
            {
                perror("recvfrom() failed");
                exit(-1);
            }

            fprintf(fp, "%s", pdu_line.data);
        }
    }

    close(sockfd_tcp);
    // Close the file send-cfg get-cfg
    fclose(fp);
    return 0;
}