import socket
from bson.json_util import dumps

def client_action(host, port, action, data):
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
        sock.connect((host, port))
        
        # Preparar a mensagem conforme a nova estrutura
        message_data = {
            "action": action,
            "username": data.get("username"),
            "password": data.get("password"),
            "message": data.get("message"),
            "destination": data.get("destination"),
            "signature": data.get("signature"),
            "public_key": data.get("public_key"),
            "group_name": data.get('group_name'),
        "member_usernames": data.get('member_usernames')
        }
        
        # Serializar a mensagem usando BSON
        message = dumps(message_data)
        sock.send(message.encode())
        
        # Receber a resposta do servidor
        response = sock.recv(1024)
        print("Response:", response.decode())

# Exemplo de uso da função ajustada:
# Teste de envio de mensagem
client_action('localhost', 54321, 'delete_group', {
    "username": "paulo4",
    "password": "123456",
    "message": "ola mundo",
    "destination": "paulo",
    "signature": "jdasdasdasda",
    "public_key": "asdasdadasdads",
    "group_name": "GPT4",
    "member_usernames": 'paulo4'
})

# {
#     "action": "singup",
#     "username": "paulo",
#     "password": "123456",
#     "message": "olá mundo!",
#     "destination": "GP04",
#     "signature": "dhaurencnjausur",
#     "public_key": "publickey"
# }
