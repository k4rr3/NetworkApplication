#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
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
//DECLARATION
struct cfg get_cfg(int argc, char* argv[]);
char* get_params(int argc, char* argv[]);
char* get_line(char line[], FILE* file);

int main(int argc, char *argv[])
{
   // char status = "DISCONNECTED";
    struct cfg user_cfg = get_cfg(argc, argv);
};


struct cfg get_cfg(int argc, char *argv[])
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

        
        
        
        
        
    
    

    //perror("An error ocurred when opening ''.cfg file");
    //exit(1);
};
char* get_line(char line[], FILE* file){
    char* word;
    if ((word = fgets(line,MAX_LEN,file)) != NULL)
        line[strlen(line) - 1] = '\0';
        word = strtok(line, " ");
        word = strtok(NULL, " ");
        //strcpy((char*) user_cfg.id, result);
        //printf("%s\n", result);
        return word;
    perror("An error ocurred when opening ''.cfg file");
    exit(1);
};

char* get_params(int argc, char* argv[])
{
    for (int i = 0; i < argc ; i++)
    {
        if ((strcmp("-c", argv[i]) == 0))
        {
            return argv[i + 1];
        }
        
    }
    return "client.cfg";
    
};
