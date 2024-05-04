#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <bson.h>
#include <arpa/inet.h>

#define PORT 12345
#define SERVER "127.0.0.1"
#define BUF_SIZE 512

void createUser(int sock, const char* username, const char* password) {
    char command[256];
    bson_t *user = bson_new();
    BSON_APPEND_UTF8(user, "action", "signup");
    BSON_APPEND_UTF8(user, "username", username);
    BSON_APPEND_UTF8(user, "password", password);
    uint32_t lenght;
    uint8_t* data = bson_destroy_with_steal(user, true, &lenght);
    
    snprintf(command, sizeof(command), "bash ./src/create_user.sh %s %s", username, password);
    int result = system(command);
    if (WIFEXITED(result) && WEXITSTATUS(result) != 0) {
        printf("Falha em executar o comando, verifique se usuario ja existe\n");
    }
    else {
        send(sock, data, lenght, 0);
        printf("Usuario criado com sucesso!\n");
    }
    free(data);
}

void uLogin(int sock, const char* username, const char* password) {
    bson_t* user = bson_new();
    BSON_APPEND_UTF8(user, "action", "login");
    BSON_APPEND_UTF8(user, "username", username);
    BSON_APPEND_UTF8(user, "password", password);




}

int main() {
    int sock;
    struct sockaddr_in server_addr;
    char buffer[BUF_SIZE];


    // Criar o socket
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Erro ao criar o socket");
        return 1;
    }

    // Configurar o endereço do servidor
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = inet_addr(SERVER);
    memset(server_addr.sin_zero, '\0', sizeof server_addr.sin_zero);

    // Conectar ao servidor
    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Falha na conexão");
        return 1;
    }

    printf("Conectado ao servidor.\n");

    int u_command;
    while (u_command != 9) {
    
        printf("\n\nEscolha abaixo: \n1 - Sign Up \n2 - Login\n\n\n9 - SAIR\n\n");
        scanf("%d", &u_command);
        //while (getchar() != '\n');
        if (u_command == 1) {
            char user[10];
            char pass[10];
            printf("Usuario: \n");
            scanf("%9s", user);
            printf("Password: \n");
            scanf("%9s", pass);
            createUser(sock, user, pass);
        }
    
    
    
    
    
    
    
    
    
    }

    // Implementar funcionalidades adicionais aqui
    // (Login, troca de mensagens, encerramento de conexão, etc.)

    close(sock);
    return 0;
}
