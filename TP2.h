#ifndef SIMPLE_OPC_CLIENT_H
#define SIMPLE_OPC_CLIENT_H

IOPCServer* InstantiateServer(wchar_t ServerName[]);
void AddTheGroup(IOPCServer* pIOPCServer, IOPCItemMgt*& pIOPCItemMgt,
	OPCHANDLE& hServerGroup, wchar_t* group);
void AddTheItem(IOPCItemMgt* pIOPCItemMgt, OPCHANDLE& hServerItem, wchar_t* itemID, enum VARENUM type);
void ReadItem(IUnknown* pGroupIUnknown, OPCHANDLE hServerItem, VARIANT& varValue);
void RemoveItem(IOPCItemMgt* pIOPCItemMgt, OPCHANDLE hServerItem);
void RemoveGroup(IOPCServer* pIOPCServer, OPCHANDLE hServerGroup);

void WriteItem(IUnknown* pGroupIUnknown, OPCHANDLE* hServerItem, VARIANT* varValue, int numVar);


#endif // SIMPLE_OPC_CLIENT_H not defined
