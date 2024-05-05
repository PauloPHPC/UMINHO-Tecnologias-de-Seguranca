#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <bson.h>
#include <arpa/inet.h>
#include <stdbool.h>

#define PORT 12345
#define SERVER "127.0.0.1"
#define BUF_SIZE 512

void clear_input_buffer() {
    int c;
    while ((c = getchar()) != '\n' && c != EOF) {}
}
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

bool loginUser(int sock, const char* username, const char* password) {
    bson_t* login_request;
    uint8_t* data;
    uint32_t length;
    char buffer[BUF_SIZE];
    int bytesReceived;
    bool success = false;

    // Create BSON object for login
    login_request = bson_new();
    BSON_APPEND_UTF8(login_request, "action", "login");
    BSON_APPEND_UTF8(login_request, "username", username);
    BSON_APPEND_UTF8(login_request, "password", password);
    data = bson_destroy_with_steal(login_request, true, &length);

    // Send login request
    if (send(sock, data, length, 0) == -1) {
        perror("Failed to send data");
        free(data);
        return false;
    }
    free(data);

    // Receive response from server
    bytesReceived = recv(sock, buffer, BUF_SIZE, 0);
    if (bytesReceived < 0) {
        perror("Receive failed");
        return false;
    }
    else if (bytesReceived == 0) {
        printf("The server closed the connection\n");
        return false;
    }

    // Decode response
    bson_t* received_data = bson_new_from_data((uint8_t*)buffer, bytesReceived);
    bson_iter_t iter;
    if (bson_iter_init_find(&iter, received_data, "status") && BSON_ITER_HOLDS_UTF8(&iter)) {
        const char* status = bson_iter_utf8(&iter, NULL);
        if (strcmp(status, "success") == 0) {
            printf("Login successful.\n");
            success = true;
        }
        else {
            printf("Login failed: %s\n", bson_iter_utf8(&iter, NULL));
        }
    }
    else {
        printf("Invalid response from server.\n");
    }

    bson_destroy(received_data);
    return success;
}

void sendMessage(int sock, const char* senderUsername, const char* recipientUsername, const char* messageContent) {
    bson_t* msg;
    uint8_t* msg_data;
    uint32_t msg_len;
    char buffer[1024];
    int bytesReceived;

    // Create BSON object for the message
    msg = bson_new();
    BSON_APPEND_UTF8(msg, "action", "send_message");
    BSON_APPEND_UTF8(msg, "username", senderUsername);
    BSON_APPEND_UTF8(msg, "destination", recipientUsername);
    BSON_APPEND_UTF8(msg, "message", messageContent);

    // Serialize the BSON object
    msg_data = bson_destroy_with_steal(msg, true, &msg_len);

    // Send the message to the server
    if (send(sock, msg_data, msg_len, 0) == -1) {
        perror("Failed to send message");
    }
    free(msg_data);

    // Wait for response from server
    bytesReceived = recv(sock, buffer, sizeof(buffer), 0);
    if (bytesReceived < 0) {
        perror("Receive failed");
    }
    else if (bytesReceived == 0) {
        printf("The server closed the connection\n");
    }
    else {
        // Decode and print server response
        bson_t* received_data = bson_new_from_data((uint8_t*)buffer, bytesReceived);
        bson_iter_t iter;
        if (bson_iter_init_find(&iter, received_data, "message") && BSON_ITER_HOLDS_UTF8(&iter)) {
            const char* server_message = bson_iter_utf8(&iter, NULL);
            printf("Server response: %s\n", server_message);
        }
        bson_destroy(received_data);
    }
}

void getMessages(int sock, const char* username) {
    bson_t* request;
    uint8_t* request_data;
    uint32_t request_length;
    char buffer[4096];  // Large buffer to accommodate multiple messages
    int bytesReceived;

    // Create BSON object for message request
    request = bson_new();
    BSON_APPEND_UTF8(request, "action", "get_messages");
    BSON_APPEND_UTF8(request, "username", username);

    // Serialize the BSON object
    request_data = bson_destroy_with_steal(request, true, &request_length);

    // Send the request to the server
    if (send(sock, request_data, request_length, 0) == -1) {
        perror("Failed to send message request");
        free(request_data);
        return;
    }
    free(request_data);

    // Wait for server response
    bytesReceived = recv(sock, buffer, sizeof(buffer), 0);
    if (bytesReceived < 0) {
        perror("Receive failed");
    }
    else if (bytesReceived == 0) {
        printf("The server closed the connection\n");
    }
    else {
        bson_t* received_data = bson_new_from_data((uint8_t*)buffer, bytesReceived);
        bson_iter_t iter;
        if (bson_iter_init_find(&iter, received_data, "data") && BSON_ITER_HOLDS_ARRAY(&iter)) {
            const uint8_t* array_data;
            uint32_t array_length;
            bson_iter_array(&iter, &array_length, &array_data);
            bson_t messages;
            bson_init_static(&messages, array_data, array_length);
            bson_iter_t msg_iter;
            if (bson_iter_init(&msg_iter, &messages)) {
                while (bson_iter_next(&msg_iter)) {
                    const uint8_t* docbuf;
                    uint32_t doclen;
                    if (BSON_ITER_HOLDS_DOCUMENT(&msg_iter)) {
                        bson_iter_document(&msg_iter, &doclen, &docbuf);
                        bson_t messageDoc;
                        bson_init_static(&messageDoc, docbuf, doclen);
                        bson_iter_t sub_iter;

                        // Extracting each field with proper error handling
                        const char* sender = NULL, * group_name = NULL, * timestamp = NULL, * content = NULL;
                        if (bson_iter_init_find(&sub_iter, &messageDoc, "sender") && BSON_ITER_HOLDS_UTF8(&sub_iter)) {
                            sender = bson_iter_utf8(&sub_iter, NULL);
                        }
                        if (bson_iter_init_find(&sub_iter, &messageDoc, "group_name") && BSON_ITER_HOLDS_UTF8(&sub_iter)) {
                            group_name = bson_iter_utf8(&sub_iter, NULL);
                        }
                        if (bson_iter_init_find(&sub_iter, &messageDoc, "timestamp") && BSON_ITER_HOLDS_UTF8(&sub_iter)) {
                            timestamp = bson_iter_utf8(&sub_iter, NULL);
                        }
                        if (bson_iter_init_find(&sub_iter, &messageDoc, "content") && BSON_ITER_HOLDS_UTF8(&sub_iter)) {
                            content = bson_iter_utf8(&sub_iter, NULL);
                        }

                        printf("From: %s | Group: %s | Date: %s | Message: %s \n", sender, group_name, timestamp, content);
                        bson_destroy(&messageDoc);
                    }
                }
                bson_destroy(&messages);
            }
        }
        bson_destroy(received_data);
    }
}

void sendMessageToGroup(int sock, const char* username, const char* groupName, const char* message) {
    bson_t* msg_request;
    uint8_t* request_data;
    uint32_t request_length;
    char buffer[4096];  // Assume a buffer large enough for the response
    int bytesReceived;

    // Create a BSON object for the message request
    msg_request = bson_new();
    BSON_APPEND_UTF8(msg_request, "action", "send_message_group");
    BSON_APPEND_UTF8(msg_request, "username", username);
    BSON_APPEND_UTF8(msg_request, "group_name", groupName);
    BSON_APPEND_UTF8(msg_request, "message", message);

    // Serialize the BSON object
    request_data = bson_destroy_with_steal(msg_request, true, &request_length);

    // Send the request to the server
    if (send(sock, request_data, request_length, 0) == -1) {
        perror("Failed to send group message");
        free(request_data);
        return;
    }
    free(request_data);

    // Wait for the server's response
    bytesReceived = recv(sock, buffer, sizeof(buffer), 0);
    if (bytesReceived < 0) {
        perror("Receive failed");
    }
    else if (bytesReceived == 0) {
        printf("The server closed the connection\n");
    }
    else {
        // Decode and print the server response
        bson_t* received_data = bson_new_from_data((uint8_t*)buffer, bytesReceived);
        bson_iter_t iter;
        if (bson_iter_init_find(&iter, received_data, "status") && BSON_ITER_HOLDS_UTF8(&iter)) {
            const char* status = bson_iter_utf8(&iter, NULL);
            if (bson_iter_init_find(&iter, received_data, "message") && BSON_ITER_HOLDS_UTF8(&iter)) {
                const char* server_message = bson_iter_utf8(&iter, NULL);
                printf("Server response: %s - %s\n", status, server_message);
            }
        }
        bson_destroy(received_data);
    }
}

void create_group(int sock, const char* group_name, char** user_names, int num_users) {
    char command[1024] = { 0 }; // Initialize to zero to avoid garbage values
    bson_t* doc;
    bson_t child;
    uint8_t* buf;
    uint32_t length;
    int i;

    // Construct the command to execute the shell script
    sprintf(command, "./src/create_group.sh %s", group_name);
    for (i = 0; i < num_users; i++) {
        strcat(command, " ");
        strcat(command, user_names[i]);
    }

    // Execute the command
    if (system(command) != 0) {
        fprintf(stderr, "Failed to create Linux group or add users.\n");
        return;
    }

    // Now send details to the Python server for application-level group management
    doc = bson_new();
    BSON_APPEND_UTF8(doc, "action", "create_group");
    BSON_APPEND_UTF8(doc, "group_name", group_name);

    BSON_APPEND_ARRAY_BEGIN(doc, "member_usernames", &child);
    for (i = 0; i < num_users; i++) {
        char key[12];
        sprintf(key, "%d", i);
        BSON_APPEND_UTF8(&child, key, user_names[i]);
    }
    bson_append_array_end(doc, &child);

    buf = bson_destroy_with_steal(doc, true, &length);
    if (send(sock, buf, length, 0) < 0) {
        perror("Failed to send data to server");
    }
    bson_free(buf); // Ensure to free the buffer to prevent memory leaks

    char buffer[1024];
    int bytesReceived = recv(sock, buffer, sizeof(buffer), 0);
    if (bytesReceived > 0) {
        printf("Server response: %s\n", buffer);
    }
    else {
        perror("Failed to receive response");
    }
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
    do {
    
        printf("\n\nEscolha abaixo: \n1 - Sign Up \n2 - Login\n\n\n0 - SAIR\n\n");
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
        else if (u_command==2) {
            char user[10];
            char pass[10];
            printf("Usuario: \n");
            scanf("%9s", user);
            printf("Password: \n");
            scanf("%9s", pass);
            //loginUser(sock, user, pass);
            if (loginUser(sock, user, pass)) {
                printf("Login Successful!\n");
                int inner_command;

                while (inner_command != 0) {
                    printf("\n\nEscolha o que deseja fazer:\n1 - Enviar uma mensagem\n2 - Ver mensagens recebidas\n3 - Enviar mensagem a grupo\n4 - Criar grupo\n5 - Excluir grupo\n6 - Adicionar membro a grupo\n\n\n0 - Logout\n");
                    scanf("%d", &inner_command);
                    clear_input_buffer();

                    switch (inner_command) {
                    case 1:
                        char receiver[10];
                        char message[512];
                        printf("Destination:\n");
                        scanf("%9s", receiver);
                        printf("Message:\n");
                        scanf("%511s", message);
                        sendMessage(sock, user, receiver, message);
                        break;

                    case 2:
                        getMessages(sock, user);
                        break;

                    case 3:
                        char group_name[10];
                        char message_group[512];
                        printf("Group Name:\n");
                        scanf("%9s", group_name);
                        printf("Message:\n");
                        scanf("%511s", message_group);


                        sendMessageToGroup(sock, user, group_name, message_group);
                        break;


                    case 4:
                        char grp_nme[10];
                        int num_users;
                        char** user_names;

                        printf("Group name:\n");
                        scanf("%9s", grp_nme);  // Safe input with limit

                        printf("How many users?\n");
                        scanf("%d", &num_users);  // Ensure & is used to store the value in num_users

                        // Allocate memory for storing pointers to user names
                        user_names = malloc(num_users * sizeof(char*));
                        if (user_names == NULL) {
                            fprintf(stderr, "Memory allocation failed\n");
                            exit(EXIT_FAILURE);
                        }

                        // Allocate and get each user name
                        for (int i = 0; i < num_users; i++) {
                            user_names[i] = malloc(10 * sizeof(char));  // Allocate space for each username
                            if (user_names[i] == NULL) {
                                fprintf(stderr, "Memory allocation failed\n");
                                exit(EXIT_FAILURE);
                            }
                            printf("Enter user %d name:\n", i + 1);
                            scanf("%9s", user_names[i]);  // Safe input with limit
                        }

                        // Call the function to create a group
                        create_group(sock, grp_nme, user_names, num_users);
                        break;





                    }
                }
            
            
            
            
            }
        
        
        } 
    
    
    
    
    
    
    
    
    
    } while (u_command != 0);

    // Implementar funcionalidades adicionais aqui
    // (Login, troca de mensagens, encerramento de conexão, etc.)

    close(sock);
    return 0;
}
