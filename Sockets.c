#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/stat.h> 
#include <sys/time.h>       // Biblioteca utilizada para atender multiplos clientes.

#define MAX_REQ_LEN 1000
#define FBLOCK_SIZE 4000

#define COD_OK0_GET "OK-0 method GET OK\n"
#define COD_OK1_FILE "OK-1 File open OK\n"
#define COD_OK2_CREATE "OK-2 method CREATE OK\n"
#define COD_OK5_CREATE_FILE "OK-5 File created OK\n"
#define COD_OK3_REMOVE "OK-3 method REMOVE OK\n"
#define COD_OK6_REMOVE_FILE "OK-6 File removed OK\n"
#define COD_OK4_APPEND "OK-4 method APPEND OK\n"
#define COD_OK7_APPEND_CONTENT "OK-7 Content appended\n"
#define COD_ERROR_0_METHOD "ERROR-0 Method not supported\n"
#define COD_ERROR_1_FILE "ERROR-1 File could not be openned\n"
#define COD_ERROR_2_CREATE_FILE "ERROR-2 File could not be created\n"
#define COD_ERROR_3_REMOVE_FILE "ERROR-3 File could not be removed\n"
#define COD_ERROR_4_APPEND_CONTENT "ERROR 4 Content could not be appended\n"
#define REQ_GET "GET"
#define REQ_CREATE "CREATE"
#define REQ_REMOVE "REMOVE"
#define REQ_APPEND "APPEND "

struct req_t
{
    char method[128];
    char filename[256];
    char text[256];
};
typedef struct req_t req_t;

void get_request(req_t *request, char *rstr)
{
    bzero(request, sizeof(req_t));
    const char s[2] = "\"";
    char *token;
    char copia[MAX_REQ_LEN];
    int i;
    strcpy(copia, rstr);
    token = strtok(copia, s);

    if ((strcmp(token, REQ_APPEND)) == 0)
    {
        strcpy(request->method, token);
        token = strtok(NULL, s);
        strcpy(request->text, token);
        token = strtok(NULL, s);
        for (i = 0; i < strlen(token); i++)
        {
            token[i] = token[i + 1];
        }
        strncpy(request->filename, token, strlen(token) - 2);
        //printf("%s", request->filename);
    }
    else
    {
        sscanf(rstr, "%s %s", request->method, request->filename);
    }
}

void send_file(int sockfd, req_t request)
{
    int fd, nr;
    unsigned char fbuff[FBLOCK_SIZE];
    fd = open(request.filename, O_RDONLY, S_IRUSR); //retorna mais um descritor
    if (fd == -1)
    {
        perror("open");
        send(sockfd, COD_ERROR_1_FILE, strlen(COD_ERROR_1_FILE), 0);
        return;
    }
    send(sockfd, COD_OK1_FILE, strlen(COD_OK1_FILE), 0);
    do
    {
        bzero(fbuff, FBLOCK_SIZE);
        nr = read(fd, fbuff, FBLOCK_SIZE);
        if (nr > 0)
        {
            send(sockfd, fbuff, nr, 0);
        }
    } while (nr > 0);
    close(fd);
    send(sockfd, "\n", 1, 0);
    return;
}

void create_file(int sockfd, req_t request)
{
    int fd, nr;
    fd = open(request.filename, O_CREAT | O_WRONLY | O_EXCL, 0777);
    if (fd == -1)
    {
        perror("create");
        send(sockfd, COD_ERROR_2_CREATE_FILE, strlen(COD_ERROR_2_CREATE_FILE), 0);
        return;
    }
    send(sockfd, COD_OK5_CREATE_FILE, strlen(COD_OK5_CREATE_FILE), 0);
    close(fd);
    return;
}

void remove_file(int sockfd, req_t request)
{
    int error;
    error = remove(request.filename);
    if (error == -1)
    {
        perror("remove");
        send(sockfd, COD_ERROR_3_REMOVE_FILE, strlen(COD_ERROR_3_REMOVE_FILE), 0);
        return;
    }
    send(sockfd, COD_OK6_REMOVE_FILE, strlen(COD_OK6_REMOVE_FILE), 0);
    return;
}

void append_content(int sockfd, req_t request)
{
    int fd;
    fd = open(request.filename, O_APPEND | O_RDWR);
    if (fd == -1)
    {
        perror("append");
        send(sockfd, COD_ERROR_4_APPEND_CONTENT, strlen(COD_ERROR_4_APPEND_CONTENT), 0);
        return;
    }
    write(fd, request.text, strlen(request.text));
    send(sockfd, COD_OK7_APPEND_CONTENT, strlen(COD_OK7_APPEND_CONTENT), 0);
    return;
}

void proc_request(int sockfd, req_t request)
{
    if (strcmp(request.method, REQ_GET) == 0)
    {
        send(sockfd, COD_OK0_GET, strlen(COD_OK0_GET), 0);
        send_file(sockfd, request);
    }
    else if (strcmp(request.method, REQ_CREATE) == 0)
    {
        send(sockfd, COD_OK2_CREATE, strlen(COD_OK2_CREATE), 0);
        create_file(sockfd, request);
    }
    else if (strcmp(request.method, REQ_REMOVE) == 0)
    {
        send(sockfd, COD_OK3_REMOVE, strlen(COD_OK3_REMOVE), 0);
        remove_file(sockfd, request);
    }
    else if (strcmp(request.method, REQ_APPEND) == 0)
    {
        send(sockfd, COD_OK4_APPEND, strlen(COD_OK4_APPEND), 0);
        append_content(sockfd, request);
    }
    else
    {
        send(sockfd, COD_ERROR_0_METHOD, strlen(COD_ERROR_0_METHOD), 0);
    }
}

int setup_server(int port)
{
    int sl, client_socket;
    int option = 1;
    struct sockaddr_in saddr;

    sl = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    setsockopt(sl, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option)); //isso conserta o problema com o bind already in use
    if (sl == -1)
    {
        perror("socket");
        return 0;
    }

    saddr.sin_port = (port);            // o valor da porta esta em argv[1], eh necessario converter para int, e para a ordem do byte correta da mquina
    saddr.sin_family = AF_INET;         // ipv4
    saddr.sin_addr.s_addr = INADDR_ANY; // endereço IP vinculado a porta

    if (bind(sl, (struct sockaddr *)&saddr, sizeof(struct sockaddr_in)) == -1)
    {
        perror("bind");
        return 0;
    }

    if (listen(sl, 1000) == -1)
    { //servidor eh capaz de aguentar 1000 clientes na fila para se conectar com o servidor
        perror("listen");
        return 0;
    }

    return sl;
}

int accept_new_connection(int sl)
{
    struct sockaddr_in caddr;
    int client_socket;
    int addr_len = sizeof(struct sockaddr_in);

    client_socket = accept(sl, (struct sockaddr *)&caddr, (socklen_t *)&addr_len);
    if (client_socket == -1)
    {
        perror("accept");
        exit(1);
    }
    printf("**Client %s:%d connected\n", inet_ntoa(caddr.sin_addr), ntohs(caddr.sin_port));
    return client_socket;
}

void handle_connection(int client_socket)
{
    char request[MAX_REQ_LEN]; // buffer da aplicacao
    req_t req;
    int nr;

    bzero(request, MAX_REQ_LEN);                       // zera o conteudo de request
    nr = recv(client_socket, request, MAX_REQ_LEN, 0); // atente-se, o descritor passado eh o descritor do cliente
    if (nr > 0)
    { // o request recebe os dados recevidos no buffer de entrada
        //printf("ifffff\n");
        get_request(&req, request); //usamos nr>0 para garantir que o servidor tenha recebido dados
        printf("method: %s\nfilename: %s\n", req.method, req.filename);
        proc_request(client_socket, req);
    }

    close(client_socket); //nao persistente
}

int main(int argc, char const *argv[])
{
    if (argc != 2)
    { //Confere se o usuario inseriu os argumetos corretos ao servidor
        printf("Uso: %s <porta>\n", argv[0]);
        return 0;
    }

    int server_socket = setup_server(htons(atoi(argv[1])));
    int i;

    fd_set current_sockets, ready_sockets; //criando 2 conjuntos de descritores de arquivos

    FD_ZERO(&current_sockets);               //Zera o conjunto de descritores de arquivo
    FD_SET(server_socket, &current_sockets); // add o server_socket para o set

    while (1)
    {
        printf("**Waiting for connections\n");

        ready_sockets = current_sockets; //Select é destrutiva
        if (select(FD_SETSIZE, &ready_sockets, NULL, NULL, NULL) < 0)
        {                     //select analisa o status dos sockets e selecionas os sockets dentro do conjunto
            perror("select"); //quando fazemos o select, ready_sockets só armazena os
            exit(1);          // os sockets que estão prontos para serem lidos
        }
        for (i = 0; i < FD_SETSIZE; i++)
        { //Valor max que PC permite ter de descritores no conjunto
            if (FD_ISSET(i, &ready_sockets))
            { //Confere se o socket i está dentro do nosso conjunto
                if (i == server_socket)
                {
                    //encontramos uma nova conexao
                    int client_socket = accept_new_connection(server_socket);
                    FD_SET(client_socket, &current_sockets); //add o socket conectado ao conjunto fixo
                }
                else
                {
                    //trata-se de um socket que já estavamos de olho
                    handle_connection(i);
                    FD_CLR(i, &current_sockets);
                }
            }
        }

    } //fim while
    close(server_socket);
    return 0;
}
