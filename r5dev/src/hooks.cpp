#include <string>
#include <stdarg.h>

#include <Windows.h>
#include <detours.h>

#include "patterns.h"
#include "utility.h"
#include "structs.h"
#include "console.h"
#include "overlay.h"
#include "hooks.h"

//---------------------------------------------------------------------------------
// Netchan
//---------------------------------------------------------------------------------

bool Hook_NET_ReceiveDatagram(int sock, void* inpacket, bool raw)
{
	bool result = NET_ReceiveDatagram(sock, inpacket, raw);
	if (result)
	{
		int i = NULL;
		netpacket_t* pkt = (netpacket_t*)inpacket;

		///////////////////////////////////////////////////////////////////////////
		// Log received packet data
		HexDump("[+] NET_ReceiveDatagram", "platform\\log\\netchan.log", "a", 0, &pkt->data[i], pkt->wiresize);
	}

	return result;
}

unsigned int Hook_NET_SendDatagram(SOCKET s, const char* buf, int len, int flags)
{
	unsigned int result = NET_SendDatagram(s, buf, len, flags);
	if (result)
	{
		///////////////////////////////////////////////////////////////////////////
		// Log transmitted packet data
		HexDump("[+] NET_SendDatagram", "platform\\log\\netchan.log", "a", 0, buf, len);
	}

	return result;
}

//---------------------------------------------------------------------------------
// SquirrelVM
//---------------------------------------------------------------------------------

void* Hook_SQVM_Print(void* sqvm, char* fmt, ...)
{
	char buf[1024];
	va_list args;
	va_start(args, fmt);
	vprintf(fmt, args);
	vsnprintf(buf, IM_ARRAYSIZE(buf), fmt, args);
	buf[IM_ARRAYSIZE(buf) - 1] = 0;
	va_end(args);
	Items.push_back(Strdup(buf));
	return NULL;
}

__int64 Hook_SQVM_LoadRson(const char* rson_name)
{
	printf("\n");
	printf("##################################################\n");
	printf("] '%s'\n", rson_name);
	printf("##################################################\n");
	printf("\n");
	return SQVM_LoadRson(rson_name);
}

bool Hook_SQVM_LoadScript(void* sqvm, const char* script_path, const char* script_name, int flag)
{
	char filepath[MAX_PATH] = { 0 };
	sprintf_s(filepath, MAX_PATH, "platform\\%s", script_path);

	///////////////////////////////////////////////////////////////////////////////
	// Flip forward slashes in filepath to windows-style backslash
	for (int i = 0; i < strlen(filepath); i++)
	{
		if (filepath[i] == '/')
		{
			filepath[i] = '\\';
		}
	}
	if (g_bDebugLog)
	{
		printf(" [+] Loading SQVM Script '%s' ...\n", filepath);
	}
	///////////////////////////////////////////////////////////////////////////////
	// Returns true if the script exists on the disk
	if (FileExists(filepath) && SQVM_LoadScript(sqvm, filepath, script_name, flag))
	{
		return true;
	}
	if (g_bDebugLog)
	{
		printf(" [!] FAILED. Try SP / VPK for '%s'\n", filepath);
	}
	return SQVM_LoadScript(sqvm, script_path, script_name, flag);
}

//---------------------------------------------------------------------------------
// Management
//---------------------------------------------------------------------------------

void InstallHooks()
{
	///////////////////////////////////////////////////////////////////////////////
	// Begin the detour transaction
	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());
	///////////////////////////////////////////////////////////////////////////////
	// Hook Engine functions
	DetourAttach((LPVOID*)&SQVM_Print, &Hook_SQVM_Print);
	DetourAttach((LPVOID*)&SQVM_LoadRson, &Hook_SQVM_LoadRson);
	DetourAttach((LPVOID*)&SQVM_LoadScript, &Hook_SQVM_LoadScript);
	///////////////////////////////////////////////////////////////////////////////
	// Commit the transaction
	if (DetourTransactionCommit() != NO_ERROR)
	{
		// Failed to hook into the process, terminate
		TerminateProcess(GetCurrentProcess(), 0xBAD0C0DE);
	}
}

void RemoveHooks()
{
	///////////////////////////////////////////////////////////////////////////////
	// Begin the detour transaction, to unhook the the process
	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());
	///////////////////////////////////////////////////////////////////////////////
	// Unhook Squirrel functions
	DetourDetach((LPVOID*)&SQVM_Print, &Hook_SQVM_Print);
	DetourDetach((LPVOID*)&SQVM_LoadRson, &Hook_SQVM_LoadRson);
	DetourDetach((LPVOID*)&SQVM_LoadScript, &Hook_SQVM_LoadScript);
	///////////////////////////////////////////////////////////////////////////////
	// Unhook Netchan functions
	DetourDetach((LPVOID*)&NET_SendDatagram, &Hook_NET_SendDatagram);
	DetourDetach((LPVOID*)&NET_ReceiveDatagram, &Hook_NET_ReceiveDatagram);
	///////////////////////////////////////////////////////////////////////////////
	// Unhook Console functions
	DetourDetach((LPVOID*)&ConVar_IsFlagSet, &Hook_ConVar_IsFlagSet);
	DetourDetach((LPVOID*)&ConCommand_IsFlagSet, &Hook_ConCommand_IsFlagSet);
	///////////////////////////////////////////////////////////////////////////////
	// Commit the transaction
	DetourTransactionCommit();
}

//---------------------------------------------------------------------------------
// Toggles
//---------------------------------------------------------------------------------

void ToggleNetHooks()
{
	static bool g_net = false;

	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());

	if (!g_net)
	{
		DetourAttach((LPVOID*)&NET_SendDatagram, &Hook_NET_SendDatagram);
		DetourAttach((LPVOID*)&NET_ReceiveDatagram, &Hook_NET_ReceiveDatagram);
		printf("\n");
		printf("+--------------------------------------------------------+\n");
		printf("|>>>>>>>>>>>>>| NETCHANNEL TRACE ACTIVATED |<<<<<<<<<<<<<|\n");
		printf("+--------------------------------------------------------+\n");
		printf("\n");
	}
	else
	{
		DetourDetach((LPVOID*)&NET_SendDatagram, &Hook_NET_SendDatagram);
		DetourDetach((LPVOID*)&NET_ReceiveDatagram, &Hook_NET_ReceiveDatagram);
		printf("\n");
		printf("+--------------------------------------------------------+\n");
		printf("|>>>>>>>>>>>>| NETCHANNEL TRACE DEACTIVATED |<<<<<<<<<<<<|\n");
		printf("+--------------------------------------------------------+\n");
		printf("\n");
	}

	if (DetourTransactionCommit() != NO_ERROR)
	{
		TerminateProcess(GetCurrentProcess(), 0xBAD0C0DE);
	}

	g_net = !g_net;
}

void ToggleDevCommands()
{
	static bool g_dev = false;

	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());

	if (!g_dev)
	{
		DetourAttach((LPVOID*)&ConVar_IsFlagSet, &Hook_ConVar_IsFlagSet);
		DetourAttach((LPVOID*)&ConCommand_IsFlagSet, &Hook_ConCommand_IsFlagSet);
		printf("\n");
		printf("+--------------------------------------------------------+\n");
		printf("|>>>>>>>>>>>>>| DEVONLY COMMANDS ACTIVATED |<<<<<<<<<<<<<|\n");
		printf("+--------------------------------------------------------+\n");
		printf("\n");

	}
	else
	{
		DetourDetach((LPVOID*)&ConVar_IsFlagSet, &Hook_ConVar_IsFlagSet);
		DetourDetach((LPVOID*)&ConCommand_IsFlagSet, &Hook_ConCommand_IsFlagSet);
		printf("\n");
		printf("+--------------------------------------------------------+\n");
		printf("|>>>>>>>>>>>>| DEVONLY COMMANDS DEACTIVATED |<<<<<<<<<<<<|\n");
		printf("+--------------------------------------------------------+\n");
		printf("\n");
	}

	if (DetourTransactionCommit() != NO_ERROR)
	{
		TerminateProcess(GetCurrentProcess(), 0xBAD0C0DE);
	}

	g_dev = !g_dev;
}