#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <bson.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#define PORT 12345
#define SERVER "127.0.0.1"
#define BUF_SIZE 512

// SSL FUNCTIONS

void init_openssl() {
    SSL_load_error_strings();
    OpenSSL_add_ssl_algorithms();
}

void cleanup_openssl() {
    EVP_cleanup();
}

SSL_CTX* create_context() {
    const SSL_METHOD* method;
    SSL_CTX* ctx;

    method = TLS_client_method();

    ctx = SSL_CTX_new(method);
    if (!ctx) {
        perror("Unable to create SSL context");
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }

    return ctx;
}
// END OF SSL FUNCTIONS

void clear_input_buffer() {
    int c;
    while ((c = getchar()) != '\n' && c != EOF) {}
}

// INIT CLIENT OPTIONS FUNCTIONS

void createUser(SSL *ssl, const char* username, const char* password) {
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
        SSL_write(ssl, data, lenght);
        printf("Usuario criado com sucesso!\n");
    }
    free(data);
}

bool loginUser(SSL *ssl, const char* username, const char* password) {
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
    if (SSL_write(ssl, data, length) == -1) {
        perror("Failed to send data");
        free(data);
        return false;
    }
    free(data);

    // Receive response from server
    bytesReceived = SSL_read(ssl, buffer, BUF_SIZE);
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

void sendMessage(SSL *ssl, const char* senderUsername, const char* recipientUsername, const char* messageContent) {
    bson_t* msg;
    uint8_t* msg_data;
    uint32_t msg_len;
    char buffer[1024];
    int bytesSent, bytesReceived;

    // Create BSON object for the message
    msg = bson_new();
    BSON_APPEND_UTF8(msg, "action", "send_message");
    BSON_APPEND_UTF8(msg, "username", senderUsername);
    BSON_APPEND_UTF8(msg, "destination", recipientUsername);
    BSON_APPEND_UTF8(msg, "message", messageContent);

    // Serialize the BSON object
    msg_data = bson_destroy_with_steal(msg, true, &msg_len);


    // Send the message to the server
    bytesSent = SSL_write(ssl, msg_data, msg_len);

    if (bytesSent <= 0) {
        int ssl_err = SSL_get_error(ssl, bytesSent);
        printf("Failed to send message:%d\n",ssl_err);
    }
    free(msg_data);

    // Wait for response from server
    bytesReceived = SSL_read(ssl, buffer, sizeof(buffer));
    if (bytesReceived < 0) {
        printf("Receive failed");
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

void getMessages(SSL *ssl, const char* username) {
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
    if (SSL_write(ssl, request_data, request_length) == -1) {
        perror("Failed to send message request");
        free(request_data);
        return;
    }
    free(request_data);

    // Wait for server response
    bytesReceived = SSL_read(ssl, buffer, sizeof(buffer));
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

void sendMessageToGroup(SSL *ssl, const char* username, const char* groupName, const char* message) {
    bson_t* msg_request;
    uint8_t* request_data;
    uint32_t request_length;
    char buffer[4096];  // Assume a buffer large enough for the response
    int bytesReceived;

    // Create a BSON object for the message request
    msg_request = bson_new();
    BSON_APPEND_UTF8(msg_request, "action", "send_message_group");
    BSON_APPEND_UTF8(msg_request, "username", username);
    BSON_APPEND_UTF8(msg_request, "destination", groupName);
    BSON_APPEND_UTF8(msg_request, "message", message);

    // Serialize the BSON object
    request_data = bson_destroy_with_steal(msg_request, true, &request_length);

    // Send the request to the server
    if (SSL_write(ssl, request_data, request_length) == -1) {
        perror("Failed to send group message");
        free(request_data);
        return;
    }
    free(request_data);

    // Wait for the server's response
    bytesReceived = SSL_read(ssl, buffer, sizeof(buffer));
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

void create_group(SSL *ssl, const char* group_name, char** user_names, int num_users) {
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
    if (SSL_write(ssl, buf, length) < 0) {
        perror("Failed to send data to server");
    }
    bson_free(buf); // Ensure to free the buffer to prevent memory leaks

    char buffer[4096];
    int bytesReceived = SSL_read(ssl, buffer, sizeof(buffer));
    if (bytesReceived > 0) {
        printf("Server response: %s\n", buffer);
    }
    else {
        perror("Failed to receive response");
    }
}

void delete_group(SSL *ssl, const char* username, const char* group_name) {
    char command[1024];
    uint8_t* buf;
    uint32_t length;
    bson_t* doc;

    // First, delete the group on the Linux system
    sprintf(command, "./src/delete_group.sh %s", group_name);
    if (system(command) != 0) {
        fprintf(stderr, "Failed to delete Linux group '%s'.\n", group_name);
        return;
    }

    // Now send details to the Python server for application-level group deletion
    doc = bson_new();
    BSON_APPEND_UTF8(doc, "action", "delete_group");
    BSON_APPEND_UTF8(doc, "username", username);
    BSON_APPEND_UTF8(doc, "group_name", group_name);

    buf = bson_destroy_with_steal(doc, true, &length);
    if (SSL_write(ssl, buf, length) < 0) {
        perror("Failed to send data to server");
    }
    bson_free(buf);

    char buffer[4096];
    int bytesReceived = SSL_read(ssl, buffer, sizeof(buffer));
    if (bytesReceived > 0) {
        printf("Server response: %s\n", buffer);
    }
    else {
        perror("Failed to receive response");
    }
}

void add_member_to_group(SSL *ssl, const char* username, const char* group_name, const char* new_member) {
    char command[1024];
    bson_t* doc;
    uint8_t* buf;
    uint32_t length;

    // Construct the command to execute the shell script
    sprintf(command, "./src/add_member.sh %s %s", new_member, group_name);
    if (system(command) != 0) {
        fprintf(stderr, "Failed to add user to Linux group.\n");
        return;
    }

    // Prepare BSON document to send to the server
    doc = bson_new();
    BSON_APPEND_UTF8(doc, "action", "add_member");
    BSON_APPEND_UTF8(doc, "username", username); // Username who is performing the action
    BSON_APPEND_UTF8(doc, "group_name", group_name);
    BSON_APPEND_UTF8(doc, "member_usernames", new_member); // Username to be added

    buf = bson_destroy_with_steal(doc, true, &length);
    if (SSL_write(ssl, buf, length) < 0) {
        perror("Failed to send data to server");
    }
    bson_free(buf);

    // Wait for the server's response
    char buffer[4096];
    int bytesReceived = SSL_read(ssl, buffer, sizeof(buffer));
    if (bytesReceived > 0) {
        printf("Server response: %s\n", buffer);
    }
    else {
        perror("Failed to receive response");
    }
}


int main() {
    // SSL AND SOCK CONFIG
    int sock;
    struct sockaddr_in server_addr;
    SSL_CTX* ctx;
    SSL* ssl;

    init_openssl();
    ctx = create_context();

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

    ssl = SSL_new(ctx);
    SSL_set_fd(ssl, sock);
    if (SSL_connect(ssl) != 1) {
        ERR_print_errors_fp(stderr);
        close(sock);
        return -1;
    }

    printf("Conectado ao servidor.\n");

    // END OF SSL AND SOCK CONFIG

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
            createUser(ssl, user, pass);
        }
        else if (u_command==2) {
            char user[10];
            char pass[10];
            printf("Usuario: \n");
            scanf("%9s", user);
            printf("Password: \n");
            scanf("%9s", pass);
            //loginUser(sock, user, pass);
            if (loginUser(ssl, user, pass)) {
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
                        clear_input_buffer();
                        printf("Message:\n");
                        fgets(message, 511, stdin);
                        message[strcspn(message, "\n")] = 0;
                        clear_input_buffer();
                        sendMessage(ssl, user, receiver, message);
                        break;

                    case 2:
                        getMessages(ssl, user);
                        break;

                    case 3:
                        char group_name[10];
                        char message_group[512];
                        printf("Group Name:\n");
                        scanf("%9s", group_name);
                        clear_input_buffer();
                        printf("Message:\n");
                        fgets(message_group, 511, stdin);
                        message_group[strcspn(message_group, "\n")] = 0;
                        clear_input_buffer();


                        sendMessageToGroup(ssl, user, group_name, message_group);
                        break;


                    case 4:
                        char grp_nme[10];
                        int num_users;
                        char** user_names;

                        printf("Group name:\n");
                        scanf("%9s", grp_nme);  // Safe input with limit
                        clear_input_buffer();
                        printf("How many users?\n");
                        scanf("%d", &num_users);  // Ensure & is used to store the value in num_users
                        clear_input_buffer();
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
                        create_group(ssl, grp_nme, user_names, num_users);
                        break;
                    
                    case 5:
                        char group_delete[10];
                        printf("Enter group to delete:\n");
                        scanf("%9s", group_delete);
                        clear_input_buffer();
                        delete_group(ssl, user, group_delete);
                        break;

                    case 6:
                        char gp_name[10];
                        char add_member[10];

                        printf("Enter group name:\n");
                        scanf("%s9", gp_name);
                        clear_input_buffer();
                        printf("Enter member name:\n");
                        scanf("%s9", add_member);
                        clear_input_buffer();


                        add_member_to_group(ssl, user, gp_name, add_member);



                        break;



                    }
                }
            
            
            
            
            }
        
        
        } 
    
    
    
    
    
    
    
    
    
    } while (u_command != 0);

    // Implementar funcionalidades adicionais aqui
    // (Login, troca de mensagens, encerramento de conexão, etc.)
    SSL_shutdown(ssl);
    SSL_free(ssl);
    close(sock);
    SSL_CTX_free(ctx);
    cleanup_openssl();

    return 0;
}
