# TP03 TS

Para o uso desta aplica��o � necess�ria a instala��o dos seguintes pacotes:

Python (requirements.txt):
- PyMongo
- bcrypt
- sqlite3

C:
- OpenSSL (libssl-dev):
	sudo apt-get install libssl-dev
- MongoDB C Driver:
	sudo apt-get install libbson-dev

##### Funcionamento da aplica��o:
Ap�s a instala��o das bibliotecas necess�rias, o makefile fornecido deve funcionar corretamente com os seguintes comandos:
1. Make
2. Iniciar o servidor atrav�s do comando "Make runserver"
3. Iniciar o Cliente atrav�s do comando "Make run"

No primeiro uso � necess�rio a cria��o de usuarios, atrav�s do "Sign Up" no menu inicial (criar� usu�rios no linux).

Para as a��es Create Group, Sign Up, Add Member e Delete Group ser� necess�rio a permiss�o de super usu�rio do Linux (sudo).
