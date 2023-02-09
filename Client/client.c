#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#define MAX_LEN 15
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
char* get_line(char* result, char line[], FILE* file);

int main(int argc, char *argv[])
{
   // char status = "DISCONNECTED";
    struct cfg user_cfg = get_cfg(argc, argv);
};


struct cfg get_cfg(int argc, char *argv[])
{
    struct cfg user_cfg;
    char* file_name = get_params(argc, argv);
    char line[MAX_LEN], *result;

    FILE* file = fopen(file_name, "r");
    result = get_line(result, line, file);
    strcpy((char*) user_cfg.id, result);
    

    //perror("An error ocurred when opening ''.cfg file");
    //exit(1);
};
char* get_line(char* result, char line[], FILE* file){

    if ((result = fgets(line,MAX_LEN,file)) != NULL)
        line[strlen(line) - 1] = '\0';
        result = strtok(line, " ");
        result = strtok(NULL, " ");
        //strcpy((char*) user_cfg.id, result);
        printf(".%s.\n", result);
        return result;
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
