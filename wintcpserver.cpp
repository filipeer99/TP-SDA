#define _CRT_SECURE_NO_WARNINGS /* Para evitar warnings sobre funçoes seguras de manipulacao de strings*/
#define _WINSOCK_DEPRECATED_NO_WARNINGS /* para evitar warnings de funções WINSOCK depreciadas */

 // Para evitar warnings irritantes do Visual Studio
#pragma warning(disable:6031)
#pragma warning(disable:6385)

#include <winsock2.h>
#include <stdio.h>
#include <conio.h>

//#include <sys/types.h>	/* basic system data types */
//#include <sys/socket.h>	/* basic socket definitions */
//#include <netinet/in.h>
//#include <string.h>
//#include <stdio.h>
//#include <time.h>
//#include <errno.h>
//#include <unistd.h>

#define	LISTENQ	 1024	/* 2nd argument to listen() */
#define SA struct sockaddr

#define TAMMSGDADOS  29  // 5+2+7+6+5 caracteres + 4 separadores
#define TAMMSGACK     8  // 5+2 caracteres + 1 separador
#define TAMMSGREQ     8  // 5+2 caracteres + 1 separador    
#define TAMMSGPAR    24  // 5+2+4+5+4 caracteres + 4 separadores
#define TAMMSGACKCP   8  // 5+2 caracteres + 1 separador
#define ESC		   0x1B

#define ESC 0x1B

#define WHITE   FOREGROUND_RED   | FOREGROUND_GREEN      | FOREGROUND_BLUE  | FOREGROUND_INTENSITY
#define HLGREEN FOREGROUND_GREEN | FOREGROUND_INTENSITY
#define HLRED   FOREGROUND_RED   | FOREGROUND_INTENSITY
#define HLBLUE  FOREGROUND_BLUE  | FOREGROUND_INTENSITY
#define YELLOW  FOREGROUND_RED   | FOREGROUND_GREEN
#define CYAN    FOREGROUND_BLUE  | FOREGROUND_GREEN      | FOREGROUND_INTENSITY

/**************************************************************************/
/* Função para testar o código de erro na comunicação via sockets         */
/*                                                                        */
/* Parâmetros de entrada:                                                 */
/*     status - código devolvido pela função de sockets chamada           */
/*                                                                        */
/*                                                                        */
/* Valor de retorno: 0 se não houve erro                                  */
/*                  -1 se o erro for recuperável                          */
/*                  -2 se o erro for irrecuperável                        */
/**************************************************************************/

int CheckSocketError(int status, HANDLE hOut) {
	int erro;

	if (status == SOCKET_ERROR) {
		SetConsoleTextAttribute(hOut, HLRED);
		erro = WSAGetLastError();
		if (erro == WSAEWOULDBLOCK) {
			printf("Timeout na operacao de RECV! errno = %d - reiniciando...\n\n", erro);
			return(-1); // acarreta reinício da espera de mensagens no programa principal
		}
		else if (erro == WSAECONNABORTED) {
			printf("Conexao abortada pelo cliente TCP - reiniciando...\n\n");
			return(-1); // acarreta reinício da espera de mensagens no programa principal
		}
		else {
			printf("Erro de conexao! valor = %d\n\n", erro);
			return (-2); // acarreta encerramento do programa principal
		}
	}
	else if (status == 0) {
		//Este caso indica que a conexão foi encerrada suavemente ("gracefully")
		printf("Conexao com cliente TCP encerrada prematuramente! status = %d\n\n", status);
		return(-1); // acarreta reinício da espera de mensagens no programa principal
	}
	else return(0);
}

/***********************************************/
/* Função para encerrar a conexao de sockets   */
/*                                             */
/* Parâmetros de entrada:  socket de conexão   */
/* Valor de retorno:       nenhum              */
/*                                             */
/***********************************************/

void CloseConnection(SOCKET connfd) {
	closesocket(connfd);
	WSACleanup();
}
/**************************************************************************/
/* Corpo do Programa                                                      */
/**************************************************************************/

int main(int argc, char** argv) {

	WSADATA     wsaData;
	SOCKET      listenfd, connfd;
	SOCKADDR_IN ServerAddr;
	SOCKADDR    MySockAddr;
	int         MySockAddrLenght;
	MySockAddrLenght = sizeof(MySockAddr);
	int optval;

	SYSTEMTIME SystemTime;

	char msgdados[TAMMSGDADOS + 1];
	char msgreq[TAMMSGREQ + 1];
	char msgspar[TAMMSGPAR + 1];
	char msgspar1[TAMMSGPAR + 1] = "NNNNN$45$48.0$010.0$0001";
	char msgspar2[TAMMSGPAR + 1] = "NNNNN$45$43.5$013.7$0012";
	char msgack[TAMMSGACK + 1] = "NNNNN$99";
	char msgackcp[TAMMSGACKCP + 1];
	char buf[100];

	int status, port, vez = 0;
	int tecla = 0, acao;
	int nseql, nseqr;
	HANDLE hOut;

	/* Verifica parametros de linha de comando */
	if ((argc != 2) || (argc == 2 && strcmp(argv[1], "-h") == 0)) {
		printf("Use: wintcpserver <port>\n");
		_exit(0);
	}
	else if (argc == 2) port = atoi(argv[1]);

	// Obtém um handle para a saída da console
	hOut = GetStdHandle(STD_OUTPUT_HANDLE);
	if (hOut == INVALID_HANDLE_VALUE)
		printf("Erro ao obter handle para a saída da console\n");
	SetConsoleTextAttribute(hOut, WHITE);

	/* Inicializa Winsock versão 2.2 */
	status = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (status != 0) {
		printf("Falha na inicializacao do Winsock 2! Erro  = %d\n", WSAGetLastError());
		WSACleanup();
		exit(0);
	}

	/* Cria socket */
	printf("Criando socket ...\n");
	listenfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (listenfd == INVALID_SOCKET) {
		status = WSAGetLastError();
		if (status == WSAENETDOWN)
			printf("Rede ou servidor de sockets inacessíveis!\n");
		else
			printf("Falha na rede: codigo de erro = %d\n", status);
		WSACleanup();
		exit(0);
	}

	/* Permite a possibilidade de reuso deste socket, de forma que,   */
	/* se uma instância anterior deste programa tiver sido encerrada  */
	/* com CTRL-C por exemplo, não ocorrera' o erro "10048" ("address */
	/* already in use") na operacao de BIND                           */
	optval = 1;
	setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (const char*)&optval, sizeof(optval));

	/* Inicializa estrutura sockaddr_in */
	printf("Inicializando estruturas ...\n");
	memset(&ServerAddr, 0, sizeof(ServerAddr));
	ServerAddr.sin_family = AF_INET;
	ServerAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	ServerAddr.sin_port = htons(port);

	/* Vincula o socket ao endereco e porta especificados */
	printf("Binding ...\n");
	status = bind(listenfd, (SOCKADDR*)&ServerAddr, sizeof(ServerAddr));
	if (status == SOCKET_ERROR) {
		printf("Falha na execucao do BIND! Erro  = %d\n", WSAGetLastError());
		WSACleanup();
		exit(0);
	}

	/* Coloca o socket em estado de escuta */
	printf("Listening ...\n");
	status = listen(listenfd, LISTENQ);
	if (status == SOCKET_ERROR) {
		printf("Falha na execucao do LISTEN! Erro  = %d\n", WSAGetLastError());
		WSACleanup();
		exit(0);
	}

	/* LOOP EXTERNO - AGUARDA CONEXOES */

	while (true) {

		nseql = 0; // Reinicia cnumeração das mensagens

		/* Aguarda conexao do cliente */
		GetSystemTime(&SystemTime);
		printf("Aguardando conexoes... data/hora local = %02d-%02d-%04d %02d:%02d:%02d\n",
			SystemTime.wDay, SystemTime.wMonth, SystemTime.wYear,
			SystemTime.wHour, SystemTime.wMinute, SystemTime.wSecond);
		connfd = accept(listenfd, &MySockAddr, &MySockAddrLenght);
		if (connfd == INVALID_SOCKET) {
			printf("Falha na execucao do ACCEPT! Erro  = %d\n", WSAGetLastError());
			WSACleanup();
			exit(0);
		}
		else printf("Conexao efetuada!\n");

		/* LOOP DE TROCA DE MENSAGENS */
		for (;;) {

			/* AGUARDA MENSAGEM */

			/* Como a mensagem a receber pode ter tamanhos diferentes, a estratégia é  */
			/* ler a mensagem de menor tamanho e verificar o campo de "codigo" da      */
			/* mensagem recebida. Se este indicar que a mensagem refere-se à de maior  */
			/* tamanho, então uma segunda chamada a recv() é feita para completar o    */
			/* buffer de mensagens.                                                    */

			SetConsoleTextAttribute(hOut, WHITE);
			printf("Aguardando mensagem...\n");
			memset(buf, 0, sizeof(buf));
			status = recv(connfd, buf, TAMMSGREQ, 0);
			if ((acao = CheckSocketError(status, hOut)) != 0) break;
			sscanf(buf, "%5d", &nseqr);
			if (++nseql != nseqr) {
				SetConsoleTextAttribute(hOut, HLRED);
				printf("Numero sequencial de mensagem incorreto [1]: recebido %d ao inves de %d\n",
					nseqr, nseql);
				CloseConnection(connfd);
				SetConsoleTextAttribute(hOut, WHITE);
				exit(0);
			}

			/* VERIFICA O CAMPO DE "CÓDIGO" DA MENSAGEM RECEBIDA */
			if (strncmp(&buf[6], "55", 2) == 0) {

				/* MENSAGEM DE DADOS DA APLICAÇÂO DE INTEGRAÇÂO */
				strncpy(msgdados, buf, TAMMSGREQ + 1);
				/* Lê o restante da mensagem */
				status = recv(connfd, buf, TAMMSGDADOS - TAMMSGREQ, 0);
				if ((acao = CheckSocketError(status, hOut)) != 0) break;
				strncpy(&msgdados[TAMMSGREQ], buf, TAMMSGDADOS - TAMMSGREQ + 1);
				SetConsoleTextAttribute(hOut, HLGREEN);
				printf("\nMensagem recebida do CLP do disco de pelotamento:\n%s\n",
					msgdados);

				/* DEVOLVE MSG DE ACK */
				memset(buf, 0, sizeof(buf));
				sprintf(buf, "%05d", ++nseql);
				memcpy(msgack, buf, 5);
				status = send(connfd, msgack, TAMMSGACK, 0);
				if ((acao = CheckSocketError(status, hOut)) != 0) break;
				SetConsoleTextAttribute(hOut, YELLOW);
				printf("Mensagem de ACK enviada ao CLP do disco de pelotamento:\n%s\n\n",
					msgack);
			}

			else if (strncmp(&buf[6], "33", 2) == 0) {

				/* MENSAGEM DE SOLICITAÇÃO DE PARÂMETROS DE CONTROLE */
				strncpy(msgreq, buf, TAMMSGREQ + 1);
				SetConsoleTextAttribute(hOut, HLBLUE);
				printf("\nMensagem de requisicao de parametros de controle:\n%s\n", msgreq);

				/* DEVOLVE MENSAGEM COM OS PARÂMETROS DE CONTROLE PARA O CLP */
				memset(buf, 0, sizeof(buf));
				sprintf(buf, "%05d", ++nseql);
				if (vez == 0) {
					strcpy(msgspar, msgspar1);
					vez = 1;
				}
				else {
					strcpy(msgspar, msgspar2);
					vez = 0;
				}
				memcpy(&msgspar[0], buf, 5);
				status = send(connfd, msgspar, TAMMSGPAR, 0);
				if ((acao = CheckSocketError(status, hOut)) != 0) break;
				SetConsoleTextAttribute(hOut, CYAN);
				printf("Enviando parametros de controle ao CLP do pelotamento:\n%s\n", msgspar);

				/* AGUARDA MENSAGEM DE ACK DO CLP DO DISCO DE PELOTAMENTO */
				memset(buf, 0, sizeof(buf));
				status = recv(connfd, buf, TAMMSGACKCP, 0);
				if ((acao = CheckSocketError(status, hOut)) != 0) break;
				sscanf(buf, "%5d", &nseqr);
				if (++nseql != nseqr) {
					SetConsoleTextAttribute(hOut, HLRED);
					printf("Numero sequencial de mensagem incorreto [2]: recebido %d ao inves de %d\n",
						nseqr, nseql);
					CloseConnection(connfd);
					SetConsoleTextAttribute(hOut, WHITE);
					exit(0);
				}
				if (strncmp(&buf[6], "00", 2) == 0) {
					strncpy(msgackcp, buf, TAMMSGACKCP + 1);
					SetConsoleTextAttribute(hOut, YELLOW);
					printf("Mensagem de ACK recebida do CLP do pelotamento:\n%s\n\n", msgackcp);
				}
				else {
					SetConsoleTextAttribute(hOut, HLRED);
					buf[8] = '\0';
					printf("Mensagem de ACK invalida: recebido %s ao inves de '00'\n\n", &buf[7]);
					acao = -2;
					break;
				}
			}

			else {
				/* MENSAGEM INVÁLIDA */
				SetConsoleTextAttribute(hOut, HLRED);
				buf[8] = '\0';
				printf("MENSAGEM RECEBIDA COM CODIGO INVALIDO: %s\n\n", &buf[6]);
				acao = -2;
				break;
			}
			/* Testa se usuario digitou ESC e, em caso afirmativo,encerra o programa */
			if (_kbhit() != 0)
				if ((tecla = _getch()) == ESC) break;
		} // END Loop secundario
		if (acao == -1)	closesocket(connfd);
		else if ((acao == -2) || (tecla == ESC)) break;
	}// END Loop primario

	SetConsoleTextAttribute(hOut, WHITE);
	printf("Encerrando o programa ...");
	CloseConnection(connfd);
	exit(0);
}
