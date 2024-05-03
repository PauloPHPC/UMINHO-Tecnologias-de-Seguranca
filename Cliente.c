#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

#define PORT 12345
#define SERVER "127.0.0.1"
#define BUF_SIZE 512

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

    // Implementar funcionalidades adicionais aqui
    // (Login, troca de mensagens, encerramento de conexão, etc.)

    close(sock);
    return 0;
}
