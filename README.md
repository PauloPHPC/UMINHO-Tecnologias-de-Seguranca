# TP03 TS

Para o uso desta aplicação é necessária a instalação dos seguintes pacotes:

Python (requirements.txt):
- PyMongo
- bcrypt
- sqlite3

C:
- OpenSSL (libssl-dev):
	sudo apt-get install libssl-dev
- MongoDB C Driver:
	sudo apt-get install libbson-dev

##### Funcionamento da aplicação:
Após a instalação das bibliotecas necessárias, o makefile fornecido deve funcionar corretamente com os seguintes comandos:
1. Make.
2. Iniciar o servidor através do comando "Make runserver".
3. Iniciar o Cliente através do comando "Make run".
4. "Make stopserver" da kill no processo do servidor.
   
No primeiro uso é necessário a criação de usuarios, através do "Sign Up" no menu inicial (criará usuários no linux).

Para as ações Create Group, Sign Up, Add Member e Delete Group será necessário a permissão de super usuário do Linux (sudo).
