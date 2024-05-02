import socket
import threading
from pymongo import MongoClient
from bson.json_util import loads, dumps
import sqlite3
from cryptography.hazmat.primitives.asymmetric import padding
from cryptography.hazmat.primitives import hashes
from cryptography.hazmat.primitives.serialization import (
    load_pem_public_key,
    load_pem_private_key,
    pkcs12,
)
import base64
import bcrypt

# Configuração do Banco de Dados SQLite
conn = sqlite3.connect("chat.db")
c = conn.cursor()
c.execute(
    """
CREATE TABLE IF NOT EXISTS users (id INTEGER PRIMARY KEY, username TEXT, password TEXT)
"""
)
c.execute(
    """
CREATE TABLE IF NOT EXISTS messages (id INTEGER PRIMARY KEY, user_id INTEGER, destination INTEGER, content TEXT, timestamp DATETIME DEFAULT CURRENT_TIMESTAMP)
"""
)
c.execute(
    """
CREATE TABLE IF NOT EXISTS groups (id INTEGER PRIMARY KEY, name TEXT UNIQUE)
"""
)
c.execute(
    """
CREATE TABLE IF NOT EXISTS group_members (group_id INTEGER, user_id INTEGER, FOREIGN KEY (group_id) REFERENCES groups(id), FOREIGN KEY (user_id) REFERENCES users(id)) 
"""
)
conn.commit()


def hash_password(password):
    return bcrypt.hashpw(password.encode("utf-8"), bcrypt.gensalt())


def verify_password(stored_password, provided_password):
    return bcrypt.checkpw(provided_password.encode("utf-8"), stored_password)


def verify_signature(signature, message, public_key):
    # try:
    #     public_key.verify(
    #         signature,
    #         message.encode(),
    #         padding.PSS(
    #             mgf=padding.MGF1(hashes.SHA256()),
    #             salt_length=padding.PSS.MAX_LENGTH
    #         ),
    #         hashes.SHA256()
    #     )
    #     return True
    # except Exception:
    return True


def receive_msg(username, message, destination, signature, public_key):
    # verify_signature(signature, message, public_key)

    # if verify_signature == True:
    return "INSERT INTO messages (user_id, destination, content) VALUES (?, ?, ?)", (
        str(username),
        destination,
        message,
    )


# else:
#   return 'Failed', 'to send message'

# Configurações do servidor
#host = "localhost"
#port = 54321

#server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
#server_socket.bind((host, port))
#server_socket.listen(5)
#print(f"Server started on {host}:{port}")


class Server:
    def __init__(self, host, port):
        self.host = host
        self.port = port
        self.server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.server_socket.bind((self.host, self.port))
        self.server_socket.listen(5)
        print(f"Server Started on {self.host}:{self.port}")

    def client_handler(self, connection):
        conn = sqlite3.connect(
            "chat.db"
        )  # Conectar ao banco de dados localmente na thread
        cursor = conn.cursor()  # Criar um novo cursor

        while True:
            data = connection.recv(1024)
            if not data:
                break
            data = loads(data)  # Deserialize BSON to Python dict

            action = data.get("action", "")
            username = data.get("username", "")
            password = data.get("password", "")
            message = data.get("message", "")
            destination = data.get("destination", "")
            signature = data.get("signature", "")
            public_key = data.get("public_key", "")
            group_name = data.get("group_name", "")
            member_usernames = data.get("member_usernames", [])

            try:
                if "action" in data:
                    if action == "singup":
                        cursor.execute(
                            "INSERT INTO users (username, password) VALUES (?, ?)",
                            (username, hash_password(password)),
                        )
                        conn.commit()
                        connection.send(
                            dumps(
                                {"status": "success", "message": "User created!"}
                            ).encode()
                        )

                    elif action == "send_message_group":
                        cursor.execute(
                            "SELECT id FROM users WHERE username = ?", (username,)
                        )
                        uid = cursor.fetchone()
                        if uid:
                            uid = uid[0]
                            cursor.execute(
                                "SELECT id FROM groups WHERE name = ?", (destination,)
                            )
                            group_id = cursor.fetchone()
                            if group_id:
                                group_id = group_id[0]
                                # Verificar se o usuário é membro do grupo
                                cursor.execute(
                                    "SELECT * FROM group_members WHERE user_id = ? AND group_id = ?",
                                    (uid, group_id),
                                )
                                if cursor.fetchone():
                                    sql, params = receive_msg(
                                        uid, message, destination, signature, public_key
                                    )
                                    cursor.execute(sql, params)
                                    conn.commit()
                                    connection.send(
                                        dumps(
                                            {
                                                "status": "success",
                                                "message": "Message sent to group!",
                                            }
                                        ).encode()
                                    )
                                else:
                                    connection.send(
                                        dumps(
                                            {
                                                "status": "error",
                                                "message": "User not in group!",
                                            }
                                        ).encode()
                                    )
                            else:
                                connection.send(
                                    dumps(
                                        {
                                            "status": "error",
                                            "message": "Destination not found!",
                                        }
                                    ).encode()
                                )
                        else:
                            connection.send(
                                dumps(
                                    {"status": "error", "message": "Sender not found!"}
                                ).encode()
                            )

                    elif action == "send_message":
                        # Get sender ID
                        cursor.execute(
                            "SELECT id FROM users WHERE username = ?", (username,)
                        )
                        sender_id = cursor.fetchone()
                        if sender_id:
                            sender_id = sender_id[0]
                            # Get recipient ID
                            cursor.execute(
                                "SELECT id FROM users WHERE username = ?",
                                (destination,),
                            )
                            recipient_id = cursor.fetchone()
                            if recipient_id:
                                recipient_id = recipient_id[0]
                                # Insert message into the database
                                sql, params = receive_msg(
                                    sender_id,
                                    message,
                                    recipient_id,
                                    signature,
                                    public_key,
                                )
                                cursor.execute(sql, params)
                                conn.commit()
                                connection.send(
                                    dumps(
                                        {
                                            "status": "success",
                                            "message": "Message Sent!",
                                        }
                                    ).encode()
                                )
                            else:
                                connection.send(
                                    dumps(
                                        {
                                            "status": "error",
                                            "message": "Receiver not found!",
                                        }
                                    ).encode()
                                )
                        else:
                            connection.send(
                                dumps(
                                    {"status": "error", "message": "Sender not found!"}
                                ).encode()
                            )

                    elif action == "create_group":
                        # Insert the new group into the database
                        cursor.execute(
                            "INSERT INTO groups (name) VALUES (?)", (group_name,)
                        )
                        group_id = (
                            cursor.lastrowid
                        )  # Get the id of the newly created group
                        conn.commit()

                        # Add members to the group
                        for username in member_usernames:
                            # Get user ID for each username
                            cursor.execute(
                                "SELECT id FROM users WHERE username = ?", (username,)
                            )
                            user_id = cursor.fetchone()
                            if user_id:
                                user_id = user_id[0]
                                cursor.execute(
                                    "INSERT INTO group_members (group_id, user_id) VALUES (?, ?)",
                                    (group_id, user_id),
                                )
                                conn.commit()
                        connection.send(
                            dumps(
                                {"status": "success", "message": "Group created!"}
                            ).encode()
                        )

                    elif action == "get_messages":
                        # Encontrar o ID do usuário com base no nome de usuário
                        cursor.execute(
                            "SELECT id FROM users WHERE username = ?", (username,)
                        )
                        user_id_result = cursor.fetchone()
                        if user_id_result:
                            user_id = user_id_result[0]

                            # Buscar mensagens diretas para o usuário e mensagens enviadas a grupos dos quais ele é membro
                            cursor.execute(
                                """
                                SELECT m.content, m.timestamp, u.username AS sender_username, '' AS group_name
                                FROM messages m
                                JOIN users u ON m.user_id = u.id
                                WHERE m.destination = ?  -- Mensagens diretas para o usuário
                                UNION
                                SELECT m.content, m.timestamp, u.username AS sender_username, g.name AS group_name
                                FROM messages m
                                JOIN users u ON m.user_id = u.id
                                JOIN groups g ON m.destination = g.name
                                JOIN group_members gm ON g.id = gm.group_id
                                WHERE gm.user_id = ? AND m.destination = g.name  -- Mensagens para os grupos dos quais o usuário é membro
                                ORDER BY m.timestamp DESC
                                """,
                                (user_id, user_id),
                            )

                            messages = cursor.fetchall()
                            if messages:
                                connection.send(dumps(messages).encode())
                            else:
                                connection.send(
                                    dumps(
                                        {"status": "success", "message": "No messages"}
                                    ).encode()
                                )
                        else:
                            connection.send(
                                dumps(
                                    {"status": "Error", "message": "User not Found"}
                                ).encode()
                            )

                    elif action == "login":
                        cursor.execute(
                            "SELECT password FROM users WHERE username = ?", (username,)
                        )
                        result = cursor.fetchone()
                        if result:
                            stored_password = result[0]
                            if verify_password(stored_password, password):
                                response = {
                                    "status": "success",
                                    "message": "Login successful",
                                }
                            else:
                                response = {
                                    "status": "error",
                                    "message": "Incorrect password",
                                }
                        else:
                            response = {
                                "status": "error",
                                "message": "Username not found",
                            }
                        connection.send(dumps(response).encode())

                    elif action == "add_member":
                        new_member_username = data.get("member_usernames", "")

                        # Primeiro, obtenha o user_id do usuário que está fazendo o pedido
                        cursor.execute(
                            "SELECT id FROM users WHERE username = ?", (username,)
                        )
                        user_id_result = cursor.fetchone()
                        if user_id_result:
                            user_id = user_id_result[0]

                            # Verifica se o grupo existe
                            cursor.execute(
                                "SELECT id FROM groups WHERE name = ?", (group_name,)
                            )
                            group_result = cursor.fetchone()
                            if group_result:
                                group_id = group_result[0]

                                # Verifica se o usuário é membro do grupo
                                cursor.execute(
                                    "SELECT * FROM group_members WHERE user_id = ? AND group_id = ?",
                                    (user_id, group_id),
                                )
                                if cursor.fetchone():
                                    # Tenta adicionar novo membro ao grupo
                                    cursor.execute(
                                        "SELECT id FROM users WHERE username = ?",
                                        (new_member_username,),
                                    )
                                    new_member_id_result = cursor.fetchone()
                                    if new_member_id_result:
                                        new_member_id = new_member_id_result[0]
                                        cursor.execute(
                                            "INSERT INTO group_members (group_id, user_id) VALUES (?, ?)",
                                            (group_id, new_member_id),
                                        )
                                        conn.commit()
                                        response = {
                                            "status": "success",
                                            "message": "Member added successfully",
                                        }
                                    else:
                                        response = {
                                            "status": "error",
                                            "message": "New member username not found",
                                        }
                                else:
                                    response = {
                                        "status": "error",
                                        "message": "User is not a member of the group",
                                    }
                            else:
                                response = {
                                    "status": "error",
                                    "message": "Group not found",
                                }
                        else:
                            response = {"status": "error", "message": "User not found"}

                        connection.send(dumps(response).encode("utf-8"))

                    elif action == "delete_group":
                        username = data.get("username", "")
                        group_name = data.get("group_name", "")

                        # Primeiro, obtenha o user_id do usuário que está fazendo a solicitação
                        cursor.execute(
                            "SELECT id FROM users WHERE username = ?", (username,)
                        )
                        user_id_result = cursor.fetchone()
                        if user_id_result:
                            user_id = user_id_result[0]

                            # Verifica se o grupo existe
                            cursor.execute(
                                "SELECT id FROM groups WHERE name = ?", (group_name,)
                            )
                            group_result = cursor.fetchone()
                            if group_result:
                                group_id = group_result[0]

                                # Verifica se o usuário é membro do grupo
                                cursor.execute(
                                    "SELECT * FROM group_members WHERE user_id = ? AND group_id = ?",
                                    (user_id, group_id),
                                )
                                if cursor.fetchone():
                                    # Deleta todas as referências dos membros e o grupo
                                    cursor.execute(
                                        "DELETE FROM group_members WHERE group_id = ?",
                                        (group_id,),
                                    )
                                    cursor.execute(
                                        "DELETE FROM groups WHERE id = ?", (group_id,)
                                    )
                                    conn.commit()
                                    response = {
                                        "status": "success",
                                        "message": "Group deleted successfully",
                                    }
                                else:
                                    response = {
                                        "status": "error",
                                        "message": "User is not a member of the group",
                                    }
                            else:
                                response = {
                                    "status": "error",
                                    "message": "Group not found",
                                }
                        else:
                            response = {"status": "error", "message": "User not found"}

                        connection.send(dumps(response).encode("utf-8"))

            except Exception as e:
                print(f"Database error: {e}")
                connection.send(b"Error")

        cursor.close()
        conn.close()
        connection.close()

    #    while True:
    #        client_socket, addr = server_socket.accept()
    #        print(f"Accepted connection from {addr}")
    #        threading.Thread(target=client_handler, args=(client_socket,)).start()

    def start(self):
        print("Server is wating for connections...")
        try:
            while True:
                client_socket, client_address = self.server_socket.accept()
                print(f"Accepted connection from {client_address}")
                client_thread = threading.Thread(
                    target=self.client_handler, args=(client_socket,)
                )
                client_thread.start()
        except KeyboardInterrupt:
            print("Bye Bye... until next time")
        finally:
            self.server_socket.close()


if __name__ == "__main__":
    server = Server("127.0.0.1", 12345)
    server.start()
