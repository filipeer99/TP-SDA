// Simple OPC Client
//
// This is a modified version of the "Simple OPC Client" originally
// developed by Philippe Gras (CERN) for demonstrating the basic techniques
// involved in the development of an OPC DA client.
//
// The modifications are the introduction of two C++ classes to allow the
// the client to ask for callback notifications from the OPC server, and
// the corresponding introduction of a message comsumption loop in the
// main program to allow the client to process those notifications. The
// C++ classes implement the OPC DA 1.0 IAdviseSink and the OPC DA 2.0
// IOPCDataCallback client interfaces, and in turn were adapted from the
// KEPWARE´s  OPC client sample code. A few wrapper functions to initiate
// and to cancel the notifications were also developed.
//
// The original Simple OPC Client code can still be found (as of this date)
// in
//        http://pgras.home.cern.ch/pgras/OPCClientTutorial/
//
//
// Luiz T. S. Mendes - DELT/UFMG - 15 Sept 2011
// luizt at cpdee.ufmg.br
//

#include <atlbase.h>    // required for using the "_T" macro
#include <iostream>
#include <ObjIdl.h>
#include <SDKDDKVer.h>
#include <ws2tcpip.h>   // includes <winsock2.h> implicitly which in turn includes <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <conio.h>
#include <process.h> // _beginthreadex() e _endthreadex() 
#include <Windows.h> // _beginthreadex() e _endthreadex() 
#include <synchapi.h>


#include "opcda.h"
#include "opcerror.h"
#include "TP2.h"
#include "SOCDataCallback.h"
#include "SOCWrapperFunctions.h"

using namespace std;
#define WIN32_LEAN_AND_MEAN

#pragma comment (lib, "Ws2_32.lib")

// -------------------- Começo Parte OPC -------------------- //

#define OPC_SERVER_NAME L"Matrikon.OPC.Simulation.1"

#define _CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES 1

// Casting para terceiro e sexto parâmetros da função _beginthreadex
typedef unsigned (WINAPI* CAST_FUNCTION)(LPVOID);
typedef unsigned* CAST_LPDWORD;


//#define REMOTE_SERVER_NAME L"your_path"

// Global variables

// The OPC DA Spec requires that some constants be registered in order to use
// them. The one below refers to the OPC DA 1.0 IDataObject interface.
UINT OPC_DATA_TIME = RegisterClipboardFormat(_T("OPCSTMFORMATDATATIME"));

const wchar_t* itemID_Read[3] = { L"Random.Real4",L"Saw-toothed Waves.Real4",L"Triangle Waves.Real4" };
enum VARENUM typesRead[5] = { VT_R4, VT_R4, VT_R4 };

const wchar_t* itemID_Write[3] = { L"Bucket Brigade.Real8",L"Bucket Brigade.Real4",L"Bucket Brigade.Int4" };
enum VARENUM typesWrite[3] = { VT_R8,VT_R4, VT_I4 };

const wchar_t* GroupsName[2] = { L"Group1", L"Group2" };
CHAR data_readed[50] = "";

double aux1;
float aux2;
unsigned aux3;

IOPCServer* pIOPCServer = NULL;   //pointer to IOPServer interface
IOPCItemMgt* pIOPCItemMgt[2] = { NULL, NULL }; //pointer to IOPCItemMgt interface

OPCHANDLE hServerGroup[2]; // server handle to the group
OPCHANDLE hServerItemRead[5];  // server handle to the item
OPCHANDLE hServerItemWrite[3];  // server handle to the item

IConnectionPoint* pIConnectionPoint = NULL; //pointer to IConnectionPoint Interface
DWORD dwCookie = 0;
SOCDataCallback* pSOCDataCallback;

int bRet;
MSG msg;
VARIANT writeVals[3];

DWORD WINAPI ReadingSock(LPVOID lpParameter);
DWORD WINAPI GetKeyboard(LPVOID lpParameter);
void ReadAndWriteOPC();
void InitOPC();
void ClosingOPC();

// -------------------- Fim Parte OPC -------------------- //


// -------------------- Começo Parte Cliente TCP/IP -------------------- //

#define DEFAULT_PORT "2342"
#define DEFAULT_BUFLEN 250
int iResultSock;

SOCKET ClientSocket = INVALID_SOCKET;


int initSocks(const char* adress);

// -------------------- Fim Parte Servidor -------------------- //

int ProcessMsg(CHAR* buffer);
HANDLE hEventsESC;
HANDLE hMutexChange, hMutexIncrementNSEQ, hMutexSend;
int nSeqSend = 1, nseqRecv = 2;
bool mustWrite = false, mustSend = true;

char tecla = 0;
#define ESC		   0x1B

void main(int argc, char** argv)
{
	InitOPC();
	if (argc == 2) {
		initSocks(argv[1]);
	}
	else {
		initSocks("127.0.0.1");
	}
	DWORD dwThreadId1, dwThreadId2;
	hEventsESC = CreateEvent(NULL, TRUE, FALSE, L"EscEvent");
	hMutexChange = CreateMutex(NULL, FALSE, L"MutexChange");
	hMutexIncrementNSEQ = CreateMutex(NULL, FALSE, L"MutexIncrementNSEQ");
	hMutexSend = CreateMutex(NULL, FALSE, L"MutexSend");

	HANDLE hThread1 = (HANDLE)_beginthreadex(
		NULL,
		0,
		(CAST_FUNCTION)ReadingSock,
		NULL,
		0,
		(CAST_LPDWORD)&dwThreadId1	// casting necessário
	);

	if (hThread1) printf("Thread de leitura do socket criada com Id= %0x \n", dwThreadId1);

	HANDLE hThread2 = (HANDLE)_beginthreadex(
		NULL,
		0,
		(CAST_FUNCTION)GetKeyboard,
		NULL,
		0,
		(CAST_LPDWORD)&dwThreadId2	// casting necessário
	);

	if (hThread2) printf("Thread de leitura do teclado criada com Id= %0x \n", dwThreadId2);
	ReadAndWriteOPC();
	HANDLE threads[2] = { hThread1, hThread2 };
	WaitForMultipleObjects(2, threads, TRUE, 5000);

	closesocket(ClientSocket);
	WSACleanup();

	CloseHandle(hThread1);
	CloseHandle(hThread2);
	CloseHandle(hEventsESC);
	CloseHandle(hMutexChange);
	CloseHandle(hMutexSend);
	CloseHandle(hMutexIncrementNSEQ);

	ClosingOPC();
}

void ReadAndWriteOPC() {
	DWORD posH;
	int iSendResult;
	char variableProcess[35];
	memset(variableProcess, 0, 35);
	do {
		posH = WaitForSingleObject(hEventsESC, 0);
		if (posH == WAIT_OBJECT_0) {
			break;
		}

		bRet = GetMessage(&msg, NULL, 0, 0);
		if (!bRet) {
			printf("Failed to get windows message! Error code = %d\n", GetLastError());
			exit(0);
		}

		memset(data_readed, 0, 50);
		TranslateMessage(&msg); // This call is not really needed ...
		DispatchMessage(&msg);  // ... but this one is!

		if (WaitForSingleObject(hMutexChange, 0) == WAIT_OBJECT_0) {
			if (mustWrite) {
				WriteItem(pIOPCItemMgt[1], hServerItemWrite, writeVals, 3);
				mustWrite = FALSE;
			}
			ReleaseMutex(hMutexChange);
		}
		if (mustSend) {
			// Região crítica para numero de sequência
			WaitForSingleObject(hMutexIncrementNSEQ, INFINITE);
			sprintf_s(variableProcess, 35, "%05d$55%s", nSeqSend, data_readed);
			nSeqSend = nSeqSend + 2;
			ReleaseMutex(hMutexIncrementNSEQ);
			// Fim da região crítica
		
			//Região crítica envio de dados no socket
			WaitForSingleObject(hMutexSend, INFINITE);
			iSendResult = send(ClientSocket, variableProcess, strlen(variableProcess), 0);
			ReleaseMutex(hMutexSend);
			// Fim da região crítica
			printf("enviado - %s\n", variableProcess);
		}
		else {
			printf("Dados obtidos mas não transmitidos: %s\n", data_readed + 1);
		}
		mustSend = !mustSend;
		memset(variableProcess, 0, 35);
	} while (TRUE);
}

void InitOPC() {

	// Have to be done before using microsoft COM library:
	printf("Initializing the COM environment...\n");
	CoInitialize(NULL);

	// Let's instantiante the IOPCServer interface and get a pointer of it:
	printf("Instantiating the MATRIKON OPC Server for Simulation...\n");
	pIOPCServer = InstantiateServer((wchar_t*)OPC_SERVER_NAME);

	// inicializando as variáveis de escrita
	for (int i = 0; i < 3; i++) {
		VariantInit(&writeVals[i]);
		writeVals[i].vt = typesWrite[i];
	}

	printf("Adding a group of Variables in the INACTIVE state for the moment...\n");
	for (int i = 0; i < 2; i++) {
		// Add the OPC group the OPC server and get an handle to the IOPCItemMgt
		//interface:
		AddTheGroup(pIOPCServer, pIOPCItemMgt[i], hServerGroup[i], (wchar_t*)GroupsName[i]);
	}

	// Add the OPC item in order to print the item name in the console.
	for (int i = 0; i < 3; i++) {
		printf("Adding the READ item %ls to the group 1...\n", itemID_Read[i]);
		AddTheItem(pIOPCItemMgt[0], hServerItemRead[i], (wchar_t*)itemID_Read[i], typesRead[i]);
	}
	for (int i = 0; i < 3; i++) {
		printf("Adding the WRITE item %ls to the group 2...\n", itemID_Write[i]);
		AddTheItem(pIOPCItemMgt[1], hServerItemWrite[i], (wchar_t*)itemID_Write[i], typesWrite[i]);
	}

	// Establish a callback asynchronous read by means of the IOPCDaraCallback
	// (OPC DA 2.0) method. We first instantiate a new SOCDataCallback object and
	// adjusts its reference count, and then call a wrapper function to
	// setup the callback.

	pSOCDataCallback = new SOCDataCallback();
	pSOCDataCallback->AddRef();

	printf("Setting up the IConnectionPoint callback connection...\n");
	SetDataCallback(pIOPCItemMgt[0], pSOCDataCallback, pIConnectionPoint, &dwCookie);

	// Change the group to the ACTIVE state so that we can receive the
	// server´s callback notification
	printf("Changing the group state to ACTIVE...\n");
	SetGroupActive(pIOPCItemMgt[0]);
	memset(data_readed, 0, 50);
}

void ClosingOPC() {
	// Cancel the callback and release its reference
	printf("Cancelling the IOPCDataCallback notifications...\n");
	CancelDataCallback(pIConnectionPoint, dwCookie);
	//pIConnectionPoint->Release();
	pSOCDataCallback->Release();

	for (int i = 0; i < 3; i++) {
		// Remove the OPC item:
		printf("Removing the OPC item %ws\n", itemID_Read[i]);
		RemoveItem(pIOPCItemMgt[0], hServerItemRead[i]);
	}

	for (int i = 0; i < 3; i++) {
		// Remove the OPC item:
		printf("Removing the OPC item %ws\n", itemID_Write[i]);
		RemoveItem(pIOPCItemMgt[1], hServerItemWrite[i]);
	}

	printf("Removing the OPC groups object...\n");
	for (int i = 0; i < 2; i++) {
		// Remove the OPC group:
		pIOPCItemMgt[i]->Release();
		RemoveGroup(pIOPCServer, hServerGroup[i]);
	}

	// release the interface references:
	printf("Removing the OPC server object...\n");
	pIOPCServer->Release();

	//close the COM library:
	printf("Releasing the COM environment...\n");
	CoUninitialize();
}

int initSocks(const char* adress) {

	SOCKADDR_IN ServerAddr;
	WSADATA wsaData;

	// Inicializa Winsock
	iResultSock = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResultSock != 0) {
		printf("WSAStartup failed: %d\n", iResultSock);
		return 1;
	}

	struct addrinfo* ptr = NULL, hints;

	ZeroMemory(&ServerAddr, sizeof(ServerAddr));
	ServerAddr.sin_family = AF_INET;
	int das = atoi(DEFAULT_PORT);
	ServerAddr.sin_port = htons(atoi(DEFAULT_PORT));
	// Substitua inet_addr por inet_pton
	if (inet_pton(AF_INET, adress, &ServerAddr.sin_addr) != 1) {
		printf("inet_pton failed: %d\n", WSAGetLastError());
		WSACleanup();
		return 1;
	}

	// Create a SOCKET for connecting to server
	ClientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (ClientSocket == INVALID_SOCKET) {
		printf("socket failed with error: %ld\n", WSAGetLastError());
		WSACleanup();
		return 1;
	}

	// Connect to server.
	iResultSock = connect(ClientSocket, (SOCKADDR*)&ServerAddr, sizeof(ServerAddr));
	if (iResultSock == SOCKET_ERROR) {
		closesocket(ClientSocket);
		ClientSocket = INVALID_SOCKET;
		printf("socket failed with error: %ld\n", WSAGetLastError());
	}


	if (ClientSocket == INVALID_SOCKET) {
		printf("Unable to connect to server!\n");
		WSACleanup();
		return 1;
	}

}

int ProcessMsg(CHAR* buffer) {
	PSTR pointer2, pointer3, pointer1, nextToken;
	CHAR bufferOut[10];
	int iSendResult = 0, nSeq;
	bool cmp, cmpBigger;


	pointer1 = strtok_s(buffer, "$", &nextToken);
	pointer2 = strtok_s(NULL, "$", &nextToken);

	if (strcmp(pointer2, "99") == 0) {
		//strncpy_s(part, nextToken, 6);
		printf("reicived: %s$%s - ", pointer1, pointer2);
		nSeq = atoi(pointer1);

		// Região crítica - Número de sequência
		WaitForSingleObject(hMutexIncrementNSEQ, INFINITE);
		cmp = nSeq == nseqRecv;
		cmpBigger = nSeq > nseqRecv;
		nseqRecv = nseqRecv + 2;
		ReleaseMutex(hMutexIncrementNSEQ);
		// Fim região crítica

		if (cmp) {
			puts("Sequência conforme");
		}
		else if (cmpBigger) {
			puts("Resposta a msg posterior");
		}
		else {
			puts("Resposta a msg anterior");
		}
	}

	else if (strcmp(pointer2, "45") == 0) {
		nSeq = atoi(pointer1);

		// strncpy_s(next, nextToken, 19);
		printf("recebido: %s$%s$%s - ", pointer1, pointer2, nextToken);

		// Região crítica - Número de sequência
		WaitForSingleObject(hMutexIncrementNSEQ, INFINITE);
		cmp = nSeq == nseqRecv;
		cmpBigger = nSeq > nseqRecv;
		nseqRecv = nseqRecv + 2;
		ReleaseMutex(hMutexIncrementNSEQ);
		// Fim região crítica

		if (cmp) {
			puts("Sequência conforme");
		}
		else if (cmpBigger) {
			puts("Resposta a msg posterior");
		}
		else {
			puts("Resposta a msg anterior");
		}

		pointer3 = strtok_s(NULL, "$", &nextToken);
		aux1 = (double)atof(pointer3);
		pointer3 = strtok_s(NULL, "$", &nextToken);
		aux2 = (float)atof(pointer3);
		pointer3 = strtok_s(NULL, "$", &nextToken);
		aux3 = (unsigned)atoi(pointer3);

		// Seção crítica - alteração dos valores de escrita OPC
		WaitForSingleObject(hMutexChange, INFINITE);
		writeVals[0].dblVal = aux1;
		writeVals[1].fltVal = aux2;
		writeVals[2].intVal = aux3;
		mustWrite = TRUE;
		ReleaseMutex(hMutexChange);
		// Fim seção crítica

		memset(bufferOut, 0, 10);
		WaitForSingleObject(hMutexIncrementNSEQ, INFINITE);
		sprintf_s(bufferOut, 10, "%05d$00", nSeqSend);
		nSeqSend++;
		nseqRecv++;
		ReleaseMutex(hMutexIncrementNSEQ);

		// Região crítica envio de dados pela rede
		WaitForSingleObject(hMutexSend, INFINITE);
		iSendResult = send(ClientSocket, bufferOut, strlen(bufferOut), 0);
		ReleaseMutex(hMutexSend);
		// Fim seção crítica
		printf("enviado - %s\n", bufferOut);
	}
	else {

	}
	if (iSendResult == SOCKET_ERROR) {

		printf("send failed: %d\n", WSAGetLastError());
		closesocket(ClientSocket);
		return 1;
	}
	return 0;
}

DWORD WINAPI ReadingSock(LPVOID lpParameter) {
	char recvbuf[DEFAULT_BUFLEN];
	int bufLen = DEFAULT_BUFLEN;
	DWORD res;
	while (tecla != ESC) {
		bufLen = DEFAULT_BUFLEN;
		memset(recvbuf, 0, bufLen);
		iResultSock = recv(ClientSocket, recvbuf, 9, 0);
		if (iResultSock > 8) {
			iResultSock = recv(ClientSocket, recvbuf + 9, 15, 0);
		}

		if (iResultSock > 0) {
			if (ProcessMsg(recvbuf)) break;

		}
		else if (iResultSock == 0) {
			printf("Connection closing...\n");
			closesocket(ClientSocket);
			break;

		}

		else {
			printf("recv failed: %d\n", WSAGetLastError());
			closesocket(ClientSocket);
			break;
		}
	}
	return 0;
}

DWORD WINAPI GetKeyboard(LPVOID lpParameter) {
	char Requisition[10];
	int iSendResult;
	memset(Requisition, 0, 10);
	while (tecla != ESC) {
		tecla = _getch();
		if (tecla == 's') {
			// Região Crítica número de sequencia
			WaitForSingleObject(hMutexIncrementNSEQ, INFINITE);
			sprintf_s(Requisition, 35, "%05d$33", nSeqSend);
			nSeqSend = nSeqSend + 2;
			ReleaseMutex(hMutexIncrementNSEQ);
			// Fim da região crítica

			// Região crítica envio de dados pela rede
			WaitForSingleObject(hMutexSend, INFINITE);
			iSendResult = send(ClientSocket, Requisition, strlen(Requisition), 0);
			ReleaseMutex(hMutexSend);
			// Fim da seção crítica
			printf("enviado - %s\n", Requisition);
			memset(Requisition, 0, 10);
		}
	}
	SetEvent(hEventsESC);
	return 0;
}
