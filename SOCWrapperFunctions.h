#define _CRT_SECURE_NO_WARNINGS 1

void SetGroupActive(IUnknown* pGroupIUnknown);
bool VarToStr(VARIANT pvar, char* buffer, int index);
void SetDataCallback(IUnknown* pGroupIUnknown, IOPCDataCallback* pSOCDataCallback,
	IConnectionPoint*& pIConnectionPoint, DWORD* pdwCookie);
void CancelDataCallback(IConnectionPoint* pIConnectionPoint, DWORD dwCookie);
