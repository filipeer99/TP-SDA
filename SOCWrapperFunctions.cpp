//
// C++ module with additional wrapper functions to extend CERN´s
// "Simple OPC Client" functionalities in order to allow OPC server
// callbacks (both OPC DA 1.0 IDataAdviseSink and OPC DA 2.0
// IOPCDataCallback.
//
// Luiz T. S. Mendes - DELT/UFMG - 13 Sept 2011

#include <ObjIdl.h>
#include <stdio.h>
#include "opcda.h"
#include <math.h>

extern UINT OPC_DATA_TIME; // Defined in "SimpleOPCClient.cpp"

//////////////////////////////////////////////////////////////////////////////
// Set the group state to ACTIVE
//
void SetGroupActive(IUnknown* pGroupIUnknown)
{
	HRESULT hr;
	IOPCGroupStateMgt* pIOPCGroupStateMgt;
	DWORD RevisedUpdateRate;
	BOOL ActiveFlag = TRUE;

	// Get a pointer to the IOPCGroupStateMgt interface:
	hr = pGroupIUnknown->QueryInterface(__uuidof(pIOPCGroupStateMgt),
		(void**)&pIOPCGroupStateMgt);
	if (hr != S_OK) {
		printf("Could not obtain a pointer to IOPCGroupStateMgt. Error = %x\n", hr);
		return;
	}
	// Set the state to Active. Since the other group properties are to remain
	// unchanged we pass NULL pointers to them as suggested by the OPC DA Spec.
	hr = pIOPCGroupStateMgt->SetState(
		NULL,                // *pRequestedUpdateRate
		&RevisedUpdateRate,  // *pRevisedUpdateRate - can´t be NULL
		&ActiveFlag,		 // *pActive
		NULL,				 // *pTimeBias
		NULL,				 // *pPercentDeadband
		NULL,				 // *pLCID
		NULL);				 // *phClientGroup

	if (hr != S_OK)
		printf("Failed call to IOPCGroupMgt::SetState. Error = %x\n", hr);
	else
		// Free the pointer since we will not use it anymore.
		pIOPCGroupStateMgt->Release();

	return;
}

/////////////////////////////////////////////////////////////////////////
// Function to mimic the Delphi VARTOSTR procedure in which a VARIANT
// is converted to a string. Only a few VARIANT types are supported in
// this version.
//
// Luiz T. S. Mendes - 07/09/2011
//
// Os trechos comentados referem-se às correções apontadas pelos alunos Rafael
// Tupynambá Dutra e Tarcísio Ribeiro Oliveira Cortes em 19/05/2012
//
// Contudo, estes comendos "sprintf_s" devem ser adaptados às características
// de cada situação específica.
//
bool VarToStr(VARIANT pvar, char* buffer, int index)
{
	bool vReturn = true;
	int intPart = 0;
	SYSTEMTIME dataHora;
	switch (pvar.vt & ~VT_ARRAY)
	{
	case VT_I2:
		sprintf_s(buffer, 30, "%03d", pvar.intVal % 1000);	break;

	case VT_I4:
		sprintf_s(buffer, 30, "%02ld", pvar.intVal % 100);	break;

	case VT_R4:
		switch (index) {
		case 1: // Temperatura ambiente no disco de pelotamento (C)
			intPart = (int)trunc(pvar.fltVal);
			sprintf_s(buffer, 30, "%07.1f", pvar.fltVal - intPart + intPart % 100000);
			break;
		case 2: // Umidade das pelotas cruas (g/m3)
			intPart = (int)trunc(pvar.fltVal);
			sprintf_s(buffer, 30, "%06.1f", pvar.fltVal - intPart + intPart % 10000);
			break;
		case 3: // Granulometria média das pelotas cruas (mm)
			intPart = (int)trunc(pvar.fltVal);
			sprintf_s(buffer, 30, "%05.1f", pvar.fltVal - intPart + intPart % 1000);
			break;
		}
		break;
	default:
		sprintf_s(buffer, 30, "%s", "");
		vReturn = false;
		break;
	}
	return(vReturn);
}
///////////////////////////////////////////////////////////////////////////////
// Set up an asynchronous connection with the server by means of the OPC DA
// 2.0 IConnectionPointContainer
//
void SetDataCallback(
	IUnknown* pGroupIUnknown,
	IOPCDataCallback* pSOCDataCallback,
	IConnectionPoint*& pIConnectionPoint,
	DWORD* pdwCookie)
{
	HRESULT hr;

	IConnectionPointContainer* pIConnPtCont = NULL; //pointer to IConnectionPointContainer
	//interface

//Get a pointer to the IConnectionPointContainer interface:
	hr = pGroupIUnknown->QueryInterface(__uuidof(pIConnPtCont), (void**)&pIConnPtCont);
	if (hr != S_OK) {
		printf("Could not obtain a pointer to IConnectionPointContainer. Error = %x\n",
			hr);
		return;
	}

	// Call the IConnectionPointContainer::FindConnectionPoint method on the
	// group object to obtain a Connection Point
	hr = pIConnPtCont->FindConnectionPoint(IID_IOPCDataCallback, &pIConnectionPoint);
	if (hr != S_OK) {
		printf("Failed call to FindConnectionPoint. Error = %x\n", hr);
		//*ptkAsyncConnection = 0;
		return;
	}

	// Now set up the Connection Point.
	// TO BE DONE: in Kepware´s code the IOPCDataCallback object is instantiated
	// here, as a consequence of the FindConnectionPoint success. It makes sense,
	// for if not we would have instantiated it unnecessarly.
	hr = pIConnectionPoint->Advise(pSOCDataCallback, pdwCookie);
	if (hr != S_OK) {
		printf("Failed call to IConnectionPoint::Advise. Error = %x\n", hr);
		*pdwCookie = 0;
	}

	// From this point on we do not need anymore the pointer to the
	// IConnectionPointContainer interface, so release it
	pIConnPtCont->Release();

	return;
}

///////////////////////////////////////////////////////////////////////////////
// Cancel an asynchronous connection with the server previosuly made by use of
// COM IConnectionPoint interface
//
void CancelDataCallback(IConnectionPoint* pIConnectionPoint, DWORD dwCookie)
{
	HRESULT hr;

	//call the IDataObject::DUnAdvise server method for cancelling the callback
	hr = pIConnectionPoint->Unadvise(dwCookie);
	if (hr != S_OK) {
		printf("Failed call to IDataObject::DUnAdvise. Error = %x\n", hr);
		dwCookie = 0;
	}

	// Release the pointer previously obtained for the IDataObject interface
	pIConnectionPoint->Release();
	return;
}
