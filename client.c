#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netdb.h>

//defining macros that used in the project.
#define USAGE_MESSAGE "Message format: [–p n <text>] [–r n <pr1=value1 pr2=value2 …>] <URL>"
#define R_ARG "-r"
#define P_ARG "-p"
#define MAX_PORT 65536
#define DEFAULT_PORT 80
//success & fail status.
#define FUNCTION_SUCCESS 0
#define FUNCTION_FAIL (-1)
//defining functions.
int isNumber(char *);
int parseRArg(char**, int, int, char *);
int parseUrl(char *, char *, char *, int *);
int parseRequest(int , char **, char *, char *, char *, char *, char *, int *);
void deallocateMemory(int*, struct sockaddr_in*, char *, char *, char *, char *, char *);
int connectToServer(int *, struct sockaddr_in *, int, char *);

/*Function checks if a given text is all digits*/
int isNumber(char *txt){
    for (int i = 0; i < strlen(txt); i++) {
        if((int)txt[i] < 48 || (int)txt[i] > 57)
            return FUNCTION_FAIL;
    }
    return FUNCTION_SUCCESS;
}
/*Function parses '-r' part of the request*/
int parseRArg(char** command, int beginning, int totalLength, char *rPart){
    //if there's no n.
    if(beginning + 1 >= totalLength || isNumber(command[beginning+1]) == FUNCTION_FAIL)
        return FUNCTION_FAIL;
    int args = (int)strtol(command[beginning+1], NULL, 10);
    //zero arguments(n = 0).
    if(args == 0 && command[beginning+1][0] == '0') {
        rPart[0] = '\0';
        return FUNCTION_SUCCESS;
    }
    //strtol() failed.
    else if(args == 0) return FUNCTION_FAIL;
    //fewer arguments than n.
    if(beginning + args + 1 >= totalLength)
        return FUNCTION_FAIL;
    int length = 0;
    //passing on the arguments.
    for(int i = beginning + 2; i < beginning + 2 + args; i++){
        if(strchr(command[i], (int)'=') == NULL) //argument that not contain '='.
            return FUNCTION_FAIL;
        length += (int)strlen(command[i]) + 1;
    }
    //building '-r' part.
    char *result = (char *) calloc(length, sizeof(char));
    int k = 0;
    for(int i = beginning + 2; i < beginning + 2 + args; i++){
        strncpy(result+k, command[i], strlen(command[i]));
        k += (int)strlen(command[i]);
        if(i == beginning + 1 + args) result[k] = '\0';
        else result[k++] = '&';
    }
    strncpy(rPart, result, strlen(result));
    free(result);
    return FUNCTION_SUCCESS;
}
/*Function parses the url part*/
int parseUrl(char *url, char *host, char *path, int *port){
    int start = 7, end = start;
    //passing on the url.
    while (end < strlen(url)){
        if(url[end] == ':'){ //if there's a port.
            int tmp = (int) strtol(url + end + 1, NULL, 10);
            if((tmp > 0 || url[end+1] == '0') && tmp < MAX_PORT) *port = tmp;
            else return FUNCTION_FAIL;
        }
        if(url[end] == '/') break;
        end++;
    }
    strncpy(host, url+start, end-start); //host.
    start = end;
    if(end < strlen(url))
       strncpy(path, url+start, strlen(url)-start); //path.
    else strncpy(path, "/", 1);
    return FUNCTION_SUCCESS;
}
/*Function parses the request passed in the main args*/
int parseRequest(int argc, char **argv, char *request, char *host, char *path, char *text, char *rPart, int *port){
    char *protocol = "HTTP/1.0";
    int post = 0, containR = 0, containUrl = 0, contentLen = 0;
    for (int i = 1; i < argc; i++) {
        //'-r' part.
        if(strcmp(argv[i], R_ARG) == 0){
            if(parseRArg(argv,i, argc, rPart) == FUNCTION_FAIL)
                return FUNCTION_FAIL;
            if(strlen(rPart) != 0) containR = 1;
        }
        //'-p' part.
        else if(strcmp(argv[i], P_ARG) == 0){
            if(post || i + 2 >= argc || isNumber(argv[i+1]) == FUNCTION_FAIL)
                return FUNCTION_FAIL;
            contentLen = (int)strtol(argv[i+1], NULL, 10);
            if(contentLen == 0 || strlen(argv[i+2]) < contentLen)
                return FUNCTION_FAIL;
            text = (char *) calloc(contentLen+1, sizeof(char));
            strncpy(text, argv[i+2], contentLen);
            post = 1;
        }
        //'-' without a letter.
        else if(argv[i][0] == '-')
            return FUNCTION_FAIL;
        else if(strlen(argv[i]) > 7){
            char token[7];
            strncpy(token,argv[i],7);
            if(strcmp(token, "http://") == 0){
                containUrl = 1;
                parseUrl(argv[i], host, path, port);
            }
        }
    }
    if(!containUrl) return FUNCTION_FAIL;
    //building the request.
    if(!containR && !post)
        sprintf(request, "GET %s %s\r\nHost: %s\r\n\r\n", path, protocol, host);
    else if(!post)
        sprintf(request, "GET %s?%s %s\r\nHost: %s\r\n\r\n", path, rPart, protocol, host);
    else if(!containR)
        sprintf(request, "POST %s %s\r\nHost: %s\r\nContent-length:%d\r\n\r\n%s", path, protocol, host, contentLen, text);
    else {
        sprintf(request, "POST %s?%s %s\r\nHost: %s\r\nContent-length:%d\r\n\r\n%s", path, rPart, protocol, host, contentLen, text);
    }
    if(text != NULL) free(text);
    return FUNCTION_SUCCESS;
}

/*Function deallocates all the memory*/
void deallocateMemory(int *fd,struct sockaddr_in* add, char *req, char *host, char *path, char *rPart, char *txt){
    if(fd != NULL)
        close(*fd);
    if(add != NULL)
        free(add);
    if(req != NULL)
        free(req);
    if(host != NULL)
        free(host);
    if(path != NULL)
        free(path);
    if(rPart != NULL)
        free(rPart);
    if(txt != NULL)
        free(txt);
}
/*Function makes a socket and connects to the server*/
int connectToServer(int *socket_fd, struct sockaddr_in *address, int port, char *host){
    if(address == NULL) {
        return FUNCTION_FAIL;
    }
    if ((*socket_fd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
        return FUNCTION_FAIL;
    }
    if(port != DEFAULT_PORT){
        int i;
        for (i = 0; host[i] != ':'; i++);
        host[i] = '\0';
    }
    //get host.
    struct hostent *server = gethostbyname(host);
    if(server == NULL) {
        return FUNCTION_FAIL;
    }
    //updating the struct.
    address->sin_family = PF_INET;
    address->sin_addr.s_addr = ((struct in_addr*)(server->h_addr))->s_addr;
    if(address->sin_addr.s_addr < 0)
        return FUNCTION_FAIL;
    address->sin_port = htons(port);
    //connection between socket and the server.
    if (connect(*socket_fd, (struct sockaddr*)address, sizeof(*address)) < 0)
        return FUNCTION_FAIL;
    return FUNCTION_SUCCESS;
}


int main(int argc, char *argv[]) {
    //allocating memory.
    char *request = NULL, *host = NULL, *path = NULL, *text = NULL, *rPart = NULL;
    int length = 0;
    for (int i = 0; i < argc; i++) {
        length += (int)strlen(argv[i]);
    }
    host = (char *) calloc(length, sizeof(char));
    path = (char *) calloc(length, sizeof(char));
    rPart = (char *) calloc(length, sizeof(char));
    request = (char *) calloc(length*2, sizeof(char));
    if(!host || !path || !rPart || !request){
        deallocateMemory(NULL, NULL, request, host, path, rPart, text);
        perror("error while allocating memory!");
        exit(EXIT_FAILURE);
    }
    //parsing request.
    int port = DEFAULT_PORT;
    if(parseRequest(argc, argv, request, host, path, text, rPart, &port) == FUNCTION_FAIL) {
        deallocateMemory(NULL, NULL, request, host, path, rPart, text);
        printf("%s\n",USAGE_MESSAGE);
        exit(EXIT_FAILURE);
    }
    //printing request.
    printf("Request:\n%s\nLEN: %d\n", request, (int)strlen(request));

    /***********************************Connecting to the server************************************/
    int socket_fd;
    struct sockaddr_in *address;
    address = (struct sockaddr_in*)calloc(1, sizeof(struct sockaddr_in));
    if(connectToServer(&socket_fd, address, port, host) == FUNCTION_FAIL){
        deallocateMemory(&socket_fd, address, request, host, path, rPart, text);
        fprintf(stderr,"error while allocating memory!\n");
        exit(EXIT_FAILURE);
    }

    /************************************Sending Request***************************************/
    if (write(socket_fd, request, strlen(request)) < 0){
        perror("error while sending request!\n");
        deallocateMemory(&socket_fd, address, request, host, path, rPart, text);
        exit(EXIT_FAILURE);
    }
    /************************************Reading response***************************************/
    char *message = (char *) calloc(1024, sizeof(char));
    ssize_t reader;
    int totalRead = 0;
    while (1) {
        bzero(message, (size_t)sizeof(message));
        reader = read(socket_fd, message, sizeof(message));
        if (reader < 0) {
            perror("error while reading response");
            free(message);
            deallocateMemory(&socket_fd, address, request, host, path, rPart, text);
            exit(EXIT_FAILURE);
        }
        else if (reader > 0){
            printf("%s", message);
            totalRead += (int)reader;
        }
        else break;
    }
    printf("\n Total received response bytes: %d\n", totalRead);
    free(message);
    deallocateMemory(&socket_fd, address, request, host, path, rPart, text);
    return 0;
}