;

    void connection_phase(int status, struct cfg user_cfg)
{
    int sockfd;
    struct sockaddr_in client_address, server_address;

    // Create a UDP socket
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
    {
        perror("socket() failed");
        exit(-1);
    }

    // Set the client address to bind it to a specific address and port
    memset(&client_address, 0, sizeof(client_address));
    client_address.sin_family = AF_INET;
    // client_address.sin_addr = address;
    // client_address.sin_port = htons(atoi((const char *)user_cfg.nms_udp_port));
    client_address.sin_addr.s_addr = htonl(0);
    client_address.sin_port = htonl(0);

    if (bind(sockfd, (struct sockaddr *)&client_address, sizeof(client_address)) != 0)
    {
        perror("bind() failed");
        exit(-1);
    }

    // Set the server port and address to be able to send the packages

    // If address from the config file is localhost, then 127.0.0.1 otherwise the ip specified is established
    char *address_str;
    if (strcmp((char *)user_cfg.nms_id, "localhost") == 0)
    {
        address_str = "127.0.0.1";
    }
    else
    {
        address_str = (char *)user_cfg.nms_id;
    }

    // Convert IP address to binary format: https://man7.org/linux/man-pages/man3/inet_pton.3.html
    struct in_addr address;
    if (inet_pton(AF_INET, address_str, &address) != 1)
    {
        perror("inet_pton() failed");
        exit(-1);
    }

    // Convert IP address to binary format
   /*  struct in_addr address;
    if (inet_pton(AF_INET, (const char *)user_cfg.nms_id, &address) == -1)
    {
        perror("Failed to convert IP address");
        exit(EXIT_FAILURE);
    } */

    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_addr = address;
    server_address.sin_port = htons(atoi((const char *)user_cfg.nms_udp_port));
    // Create PDU REGISTER_REQ package
    struct pdu_udp pdu_reg_request = generate_pdu_request(user_cfg);
    unsigned char pdu_package[78] = {"\n"};
    pdu_package[0] = 0x00;
    // strcpy((char *)&pdu_package[0], (const char *)pdu_reg_request.pdu_type);
    strcpy((char *)&pdu_package[1], (const char *)pdu_reg_request.system_id);
    strcpy((char *)&pdu_package[1 + 7], (const char *)pdu_reg_request.mac_address);
    strcpy((char *)&pdu_package[1 + 7 + 13], (const char *)pdu_reg_request.random_number);
    strcpy((char *)&pdu_package[1 + 7 + 13 + 7], (const char *)pdu_reg_request.data);

    // Send register package to the server
    if (sendto(sockfd, pdu_package, 78, 0, (struct sockaddr *)&server_address, sizeof(server_address)) < 0)
    {
        perror("sendto() failed");
        exit(-1);
    }
    status = WAIT_REG_RESPONSE;
    struct timeval timeout = {T, 0}; // tv_sec -> T(s) and tv_usec -> 0(ms)
    // Receive data from the server
    int received_bytes = recvfrom(sockfd, pdu_package, 78, 0, (struct sockaddr *)&server_address, &(socklen_t){sizeof(server_address)});
    if (received_bytes < 0)
    {
        perror("recvfrom() failed");
        exit(-1);
    }
    pdu_package[received_bytes] = '\0';
    // printf("Received message from server: .%s. with %d received_bytes\n", pdu_package, received_bytes);
    switch (pdu_package[0])
    {
    case REGISTER_ACK: // 0x02
        status = REGISTERED;
        char random_num[8];
        random_num[8] = '\n';
        for (int i = 0; i < 7; i++)
        {
            random_num[i] = pdu_package[21 + i];
        }
        printf("Random numero leido es: %s\n", random_num);
        char port[5];
        port[5] = '\n';
        for (int i = 0; i < 4; i++)
        {
            port[i] = pdu_package[28 + i];
        }
        printf("Port number: %s\n", port);
        printf("REGISTERED\n");
        // ENTERING ALIVE MODE
        break;
    case REGISTER_NACK: // 0x04
        /* if (num_process < O){

            connection_phase(DISCONNECTED, user_cfg);
        } */
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
    // Close the socket
    close(sockfd);
}