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

const wchar_t* itemID_Read[3] = { L"Random.Real4",L"Saw-toothed Waves.Real4",L"Triangle Waves.Real4"};
enum VARENUM typesRead[5] = { VT_R4, VT_R4, VT_R4};

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

DWORD WINAPI loopingReadSocket(LPVOID lpParameter);
DWORD WINAPI loopEsc(LPVOID lpParameter);
void loopingReadOPC();
void InitOPCvariables();
void closingOPCvariables();

// -------------------- Fim Parte OPC -------------------- //


// -------------------- Começo Parte Servidor -------------------- //

#define DEFAULT_PORT "2342"
#define DEFAULT_BUFLEN 250
int iResultSock;

SOCKET ListenSocket = INVALID_SOCKET;
// Socket temporario para aceitar conexões
SOCKET ClientSocket = INVALID_SOCKET;


int initSocks();

// -------------------- Fim Parte Servidor -------------------- //

int ProcessMsg(CHAR* buffer);
HANDLE hEventsESC;
HANDLE hMutexChange, hMutexIncrementNSEQ;
int nSeq = 0;
bool nseqIncrement = false, mustWrite = false;

char tecla = 0;
#define ESC		   0x1B

void main(void)
{
	InitOPCvariables();
	initSocks();
	DWORD dwThreadId1, dwThreadId2;
	hEventsESC = CreateEvent(NULL, TRUE, FALSE, L"EscEvent");
	hMutexChange = CreateMutex(NULL, FALSE, L"MutexChange");
	hMutexIncrementNSEQ = CreateMutex(NULL, FALSE, L"MutexIncrementNSEQ");
	
	HANDLE hThread1 = (HANDLE)_beginthreadex(
		NULL,
		0,
		(CAST_FUNCTION)loopingReadSocket,
		NULL,
		0,
		(CAST_LPDWORD)&dwThreadId1	// casting necessário
	);

	if (hThread1) printf("Thread de leitura do socket criada com Id= %0x \n", dwThreadId1);

	HANDLE hThread2 = (HANDLE)_beginthreadex(
		NULL,
		0,
		(CAST_FUNCTION)loopEsc,
		NULL,
		0,
		(CAST_LPDWORD)&dwThreadId2	// casting necessário
	);

	if (hThread2) printf("Thread de leitura do teclado criada com Id= %0x \n", dwThreadId2);
	loopingReadOPC();
	HANDLE threads[2] = { hThread1, hThread2 };
	WaitForMultipleObjects(2, threads, TRUE, 5000);

	closesocket(ClientSocket);
	closesocket(ListenSocket);
	WSACleanup();

	CloseHandle(hThread1);
	CloseHandle(hThread2);
	CloseHandle(hEventsESC);
	CloseHandle(hMutexChange);
	
	closingOPCvariables();
}

void loopingReadOPC() {
	DWORD posH;
	int iSendResult;
	char variableProcess[35];
	memset(variableProcess, 0, 35);
	do {
		posH = WaitForSingleObject(hEventsESC, 0); 
		if (posH == WAIT_OBJECT_0){
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
			}
			ReleaseMutex(hMutexChange);
		}
		sprintf_s(variableProcess, 35, "%06d$55%s", nSeq, data_readed);
		iSendResult = send(ClientSocket, variableProcess, strlen(variableProcess), 0);
		nseqIncrement = true;
		memset(variableProcess, 0, 35);

	} while (TRUE);
}

void InitOPCvariables() {

	// Have to be done before using microsoft COM library:
	printf("Initializing the COM environment...\n");
	CoInitialize(NULL);

	// Let's instantiante the IOPCServer interface and get a pointer of it:
	printf("Instantiating the MATRIKON OPC Server for Simulation...\n");
	pIOPCServer = InstantiateServer((wchar_t*)OPC_SERVER_NAME);

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
	for (int i = 0; i < 5; i++) {
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

void closingOPCvariables() {
	// Cancel the callback and release its reference
	printf("Cancelling the IOPCDataCallback notifications...\n");
	CancelDataCallback(pIConnectionPoint, dwCookie);
	//pIConnectionPoint->Release();
	pSOCDataCallback->Release();

	for (int i = 0; i < 5; i++) {
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

int initSocks() {
	WSADATA wsaData;

	// Inicializa Winsock
	iResultSock = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResultSock != 0) {
		printf("WSAStartup failed: %d\n", iResultSock);
		return 1;
	}

	struct addrinfo* result = NULL, * ptr = NULL, hints;

	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;

	// Resolve o local address e a porta porta para ser usada no servidor
	iResultSock = getaddrinfo(NULL, DEFAULT_PORT, &hints, &result);
	if (iResultSock != 0) {
		printf("getaddrinfo failed: %d\n", iResultSock);
		WSACleanup();
		return 1;
	}

	ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
	printf("Waiting for new connections...\n");

	if (ListenSocket == INVALID_SOCKET) {
		printf("Error at socket(): %ld\n", WSAGetLastError());
		freeaddrinfo(result);
		WSACleanup();
		return 1;
	}

	// Configura a "escuta" do Socket TCP
	iResultSock = bind(ListenSocket, result->ai_addr, (int)result->ai_addrlen);
	if (iResultSock == SOCKET_ERROR) {
		printf("bind failed with error: %d\n", WSAGetLastError());
		freeaddrinfo(result);
		closesocket(ListenSocket);
		WSACleanup();
		return 1;
	}
	freeaddrinfo(result);

	if (listen(ListenSocket, 5) == SOCKET_ERROR) {
		printf("Listen failed with error: %ld\n", WSAGetLastError());
		closesocket(ListenSocket);
		WSACleanup();
		return 1;
	}
}

int ProcessMsg(CHAR* buffer) {
	PSTR pointer, pointer2, pointer3, nextToken;
	CHAR part[25];
	
	CHAR bufferOut[60];
	int iSendResult = 0;

	pointer3 = strtok_s(buffer, "$", &nextToken);
	pointer = strtok_s(NULL, "$", &nextToken);
	do {
		memset(bufferOut, 0, 60);
		memset(part, 0, 25);
		if (strcmp(pointer, "99") == 0) {
			strncpy_s(part, nextToken, 6);
			printf("reicived: %s$%s\n", pointer, part);
			pointer = nextToken + 6;
			nSeq = atoi(part) + 1;


			sprintf_s(bufferOut, 60, "%06d$55%s", nSeq, data_readed);
			iSendResult = send(ClientSocket, bufferOut, strlen(bufferOut), 0);
		}

		/*else if (strcmp(pointer, "33") == 0) {
			strncpy_s(part, nextToken, 6);
			printf("reicived: %s|%s\n", pointer, part);
			pointer = nextToken + 6;
			nSeq = atoi(part);
		}*/

		else if (strcmp(pointer, "45") == 0) {
			nSeq = atoi(pointer3) + 1;

			// strncpy_s(next, nextToken, 19);
			printf("reicived: %s$%s$%s\n", pointer3, pointer, nextToken);

			pointer2 = strtok_s(NULL, "$", &nextToken);
			aux1 = (double)atof(pointer2);
			pointer2 = strtok_s(NULL, "$", &nextToken);
			aux2 = (float)atof(pointer2);
			pointer2 = strtok_s(NULL, "$", &nextToken);
			aux3 = (unsigned)atoi(pointer2);
			
			writeVals[0].dblVal = aux1;
			writeVals[1].fltVal = aux2;
			writeVals[2].intVal = aux3;

			SetEvent(/*hEvents[1]*/hEvents[2]);
			WaitForSingleObject(/*hEvents[3]*/hEvents[4], INFINITE);
			ResetEvent(/*hEvents[3]*/hEvents[4]);

			sprintf_s(bufferOut, 60, "%06d$00", nSeq);
			iSendResult = send(ClientSocket, bufferOut, strlen(bufferOut), 0);
		}
		else {

		}
		pointer = strtok_s(pointer, "|", &nextToken);
	} while (pointer != NULL && iSendResult != SOCKET_ERROR);

	if (iSendResult == SOCKET_ERROR) {

		printf("send failed: %d\n", WSAGetLastError());
		closesocket(ClientSocket);
		return 1;
	}
	return 0;
}

DWORD WINAPI loopingReadSocket(LPVOID lpParameter) {
	char recvbuf[DEFAULT_BUFLEN];
	int bufLen = DEFAULT_BUFLEN;

	while (tecla != ESC) {
		// Aceita um cliente socket
		ClientSocket = accept(ListenSocket, NULL, NULL);

		if (ClientSocket == INVALID_SOCKET) {
			printf("accept failed: %d\n", WSAGetLastError());
			closesocket(ListenSocket);
			WSACleanup();
			break;
		}
		printf("New connection accepted..\n");

		const WSAEVENT evs[2] = { hEvents[0], (WSAEVENT)ClientSocket };
		DWORD res;
		while (true) {
			bufLen = DEFAULT_BUFLEN;
			res = WaitForSingleObject(hEvents[0], 0);
			if (res == WAIT_OBJECT_0) {
				break;
			}
			memset(recvbuf, 0, bufLen);
			iResultSock = recv(ClientSocket, recvbuf, bufLen, 0);

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
	}
	return 0;
}

DWORD WINAPI loopEsc(LPVOID lpParameter) {
	while (tecla != ESC) {
		tecla = _getch();
		if (tecla == 's')
			SetEvent(hEvents[1]);
	}
	SetEvent(hEvents[0]);
	return 0;
}
