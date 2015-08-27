#include "stdafx.h"
#define WIN32_LEAN_AND_MEAN

//#include <windows.h>

#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>
#include <io.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdint.h>
#include <string>
#include <vector>
#include <process.h>
#include <sstream>
#include <algorithm>
#include <map>
#include <cstdarg>
#include <iostream>
#include <vadefs.h>
#include "Resource.h"
#include <assert.h>

const char *szTLPath = NULL;

class CTLauncher;
CTLauncher *CTL = NULL;

LRESULT CALLBACK ProcessMessages(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

std::map<CTLauncher*, HWND> skLauncherHandleList;

char szClass[32];
BOOL CALLBACK API_EW_CALLBACK(HWND hwnd, LPARAM lParam)
{
	GetClassNameA(hwnd, szClass, 32);
	if(strcmp(szClass, "LAUNCHER_CLASS") == 0)
	{
		skLauncherHandleList.insert(std::make_pair((CTLauncher*)NULL, hwnd));
	}

	return TRUE;
}

BOOL CALLBACK API_EW_FIND_CALLBACK(HWND hwnd, LPARAM lParam)
{
	GetClassNameA(hwnd, szClass, 32);
	if(strcmp(szClass, "LAUNCHER_CLASS") == 0)
	{
		for(std::map<CTLauncher*, HWND>::iterator it = skLauncherHandleList.begin(); it != skLauncherHandleList.end(); ++it)
		{
			if(it->second == hwnd)
			{
				return true;
			}
		}

		if(lParam)
		{
			*(HWND*)lParam = hwnd;
		}
	}

	return true;
}

class CTLauncher
{
private:
	wchar_t m_szTitle[100];
	wchar_t m_szClass[100];

	HWND	m_MainHWnd;
	HWND	m_TLHWnd;

	unsigned char m_szMsgBuffer[1024];

	COPYDATASTRUCT	m_CopyData;
	WNDPROC m_WNDProc;

	DWORD m_dwStage;

	char m_szServerList[1024];
	char m_szAccountData[1024];

	unsigned int m_iServerListLen;
	unsigned int m_iAccountDataLen;

public:
	enum EIdents
	{
		ID_HELLO		= 0x0DBADB0A,
		ID_SLS_URL		= 0x00000002,
		ID_GAME_STR		= 0x00000003,
		ID_LAST_SVR		= 0x00000005,
		ID_CHAR_CNT		= 0x00000006,
		ID_TICKET		= 0x00000008,
		ID_END_POPUP	= 0x00000000
	};

public:
	CTLauncher(HINSTANCE hInstance, const char* ServerList, const char* AccountData,
		unsigned int iServerListLen, unsigned int iAccountDataLen) : 
	m_MainHWnd(NULL), 
	m_TLHWnd(NULL), 
	m_WNDProc(NULL), 
	m_iServerListLen(iServerListLen), 
	m_iAccountDataLen(iAccountDataLen)
	{
		memset(m_szMsgBuffer, 0x00, 1024);
		memset(&m_CopyData, 0x00, sizeof(COPYDATASTRUCT));

		wcscpy(m_szTitle, TEXT("```````d```!````````"));
		wcscpy(m_szClass, TEXT("EME.LauncherWnd"));

		if(!szTLPath)
		{
			szTLPath = "Client/TL.exe";
		}
		if(ServerList)
		{
			strncpy(m_szServerList, ServerList, iServerListLen);
			m_szServerList[iServerListLen] = 0x00;
			//iServerListLen++;
		}
		if(AccountData)
		{
			strncpy(m_szAccountData, AccountData, iAccountDataLen);
			m_szAccountData[iAccountDataLen] = 0x00;
			//iAccountDataLen++;
		}
	};

	~CTLauncher()
	{
	}

	void Initialize(HINSTANCE hInstance)
	{
		MSG msg;
		HACCEL hAccelTable;

		assert(ARegisterClass(hInstance));	
		assert(CreateEMEWindow(hInstance));
		assert(LaunchTL());

		hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_WIN32PROJECT5));

		while(true)
		{
			while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE))
			{
				if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
				{
					TranslateMessage(&msg);
					DispatchMessage(&msg);
				}
			}

			if(m_dwStage == ID_HELLO)
			{
				SendHello();
			}
			else if(m_dwStage == ID_SLS_URL)
			{
				SendServerList();
			}
			else if(m_dwStage == ID_GAME_STR)
			{
				SendAccountList();
			}

			m_dwStage = -1;
		}
	}

	ATOM ARegisterClass(HINSTANCE hInstance)
	{
		WNDCLASSEX wcex;

		wcex.cbSize = sizeof(WNDCLASSEX);

		wcex.style			= CS_HREDRAW | CS_VREDRAW;
		wcex.lpfnWndProc	= CTLauncher::ProcessMessages;
		wcex.cbClsExtra		= 0;
		wcex.cbWndExtra		= 0;
		wcex.hInstance		= hInstance;
		wcex.hIcon			= LoadIcon(hInstance, MAKEINTRESOURCE(IDI_WIN32PROJECT5));
		wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
		wcex.hbrBackground	= (HBRUSH)(COLOR_WINDOW+1);
		wcex.lpszMenuName	= MAKEINTRESOURCE(IDC_WIN32PROJECT5);
		wcex.lpszClassName	= m_szClass;
		wcex.hIconSm		= LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

		return RegisterClassEx(&wcex);
	}

	bool CreateEMEWindow(HINSTANCE hInstance)
	{
		m_MainHWnd = CreateWindow(m_szClass, m_szClass, WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, NULL, NULL, hInstance, this);

		if(!m_MainHWnd)
		{
			return false;
		}

		SetWindowLongPtr(m_MainHWnd, GWL_USERDATA, (LONG_PTR)this);
		m_WNDProc = (WNDPROC)SetWindowLongPtr(m_MainHWnd, GWL_WNDPROC, (LONG_PTR)&ProcessClassMessages);

		return true;
	}

	bool LaunchTL()
	{
		STARTUPINFOA aInfo;
		PROCESS_INFORMATION pInfo;

		memset(&aInfo, 0x00, sizeof(STARTUPINFOA));
		memset(&pInfo, 0x00, sizeof(PROCESS_INFORMATION));

		aInfo.cb = sizeof(STARTUPINFOA);

		assert(szTLPath);

		if(CreateProcessA(NULL, (LPSTR)szTLPath, NULL, NULL, true, 0x24, NULL, NULL, &aInfo, &pInfo))
		{
			ResumeThread(pInfo.hThread);
			CloseHandle(pInfo.hProcess);

			WaitForSingleObject(pInfo.hProcess, INFINITE);
			Sleep(1000);
			EnumWindows(API_EW_FIND_CALLBACK, (LPARAM)&m_TLHWnd);

			if(!m_TLHWnd)
			{
				return false;
			}

			skLauncherHandleList.insert(std::make_pair(this, m_TLHWnd));

			return true;
		}

		return false;
	}

	void SendHello()
	{
		assert(m_MainHWnd != NULL);
		assert(m_TLHWnd != NULL);

		memcpy(m_szMsgBuffer, "Hello!!", 7);

		m_CopyData.cbData = 8;
		m_CopyData.dwData = ID_HELLO;
		m_CopyData.lpData = m_szMsgBuffer;

		DWORD p;
		SendMessageTimeoutW(m_TLHWnd, WM_COPYDATA, (WPARAM)m_MainHWnd, (LPARAM)&m_CopyData, SMTO_NORMAL, 5000, &p);

		if(!p)
		{
			printf("Couldn't send ID_HELLO: Timeout (5000ms) -> %d\n", p);
		}
		else
		{
			printf("Sent new message: ID_HELLO -> %d\n", p);
		}
	}

	void SendServerList()
	{
		assert(m_MainHWnd != NULL);
		assert(m_TLHWnd != NULL);

		memcpy(m_szMsgBuffer, m_szServerList, m_iServerListLen);

		m_CopyData.cbData = m_iServerListLen;
		m_CopyData.dwData = ID_SLS_URL;
		m_CopyData.lpData = m_szMsgBuffer;

		DWORD p;
		SendMessageTimeoutW(m_TLHWnd, WM_COPYDATA, (WPARAM)m_MainHWnd, (LPARAM)&m_CopyData, SMTO_NORMAL, 5000, &p);

		if(!p)
		{
			printf("Couldn't send ID_SLS_URL: Timeout (5000ms) -> %d\n", p);
		}
		else
		{
			printf("Sent new message: ID_SLS_URL -> %d\n", p);
		}
	}

	void SendAccountList()
	{
		assert(m_MainHWnd != NULL);
		assert(m_TLHWnd != NULL);

		memcpy(m_szMsgBuffer, m_szAccountData, m_iAccountDataLen);

		m_CopyData.cbData = m_iAccountDataLen;
		m_CopyData.dwData = ID_GAME_STR;
		m_CopyData.lpData = m_szMsgBuffer;

		DWORD p;
		SendMessageTimeoutW(m_TLHWnd, WM_COPYDATA, (WPARAM)m_MainHWnd, (LPARAM)&m_CopyData, SMTO_NORMAL, 5000, &p);

		if(!p)
		{
			printf("Couldn't send ID_GAME_STR: Timeout (5000ms) -> %d\n", p);
		}
		else
		{
			printf("Sent new message: ID_GAME_STR -> %d\n", p);
		}
	}

	HWND HWNDGetMain()
	{
		return m_MainHWnd;
	}

	HWND HWNDGetTL()
	{
		return m_TLHWnd;
	}

	static LRESULT CALLBACK ProcessMessages(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
	{
		CTLauncher *CTL = (CTLauncher*)GetWindowLongPtr(hwnd, GWL_USERDATA);

		if(!CTL || hwnd != CTL->HWNDGetMain())
		{
			return DefWindowProc(hwnd, message, wParam, lParam);
		}
		else
		{
			return CallWindowProc(CTL->m_WNDProc, hwnd, message, wParam, lParam);
		}
	}

	static LRESULT CALLBACK ProcessClassMessages(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
	{
		HDC hdc;
		PAINTSTRUCT ps;
		DWORD wmId;
		DWORD wmEvent;
		std::string Ticket;
		COPYDATASTRUCT *copy = NULL;

		CTLauncher *CTL = (CTLauncher*)GetWindowLongPtr(hwnd, GWL_USERDATA);

		if(!CTL || hwnd != CTL->HWNDGetMain())
		{
			return DefWindowProc(hwnd, message, wParam, lParam);
		}

		DWORD dwIdent = -1;
		if(lParam && message == WM_COPYDATA)
		{
			dwIdent = *(DWORD*)(lParam);
		}

		printf("Received new message: HWND %d, MSG %d, WPARAM %d, IDENT %d\n", hwnd, message, wParam, dwIdent);

		switch(message)
		{
		case WM_COPYDATA:
			switch(dwIdent)
			{
			case CTL->ID_TICKET:
				copy = (COPYDATASTRUCT*)lParam;
				if(copy)
				{
					printf("Received ticket: %s\n", std::string(reinterpret_cast<const char*>(copy->lpData)).c_str());

					Ticket = std::string(CTL->m_szAccountData);
					wmId = Ticket.find("\"ticket\":");

					if(wmId != Ticket.npos)
					{
						memcpy((LPVOID)(CTL->m_szAccountData + wmId + strlen("\"ticket\":")), copy->lpData, copy->cbData);
					}
					else
					{
						printf("Couldn't patch ticket.\n");
					}
				}
				return 1;
			case CTL->ID_HELLO:
				SendMessage(hwnd, WM_APP, (WPARAM)hwnd, NULL);
				return 1;
			case CTL->ID_SLS_URL:
				CTL->m_dwStage = CTL->ID_SLS_URL;
				return 1;
			case CTL->ID_GAME_STR:
				CTL->m_dwStage = CTL->ID_GAME_STR;
				SendMessage(hwnd, WM_APP + 3, (WPARAM)hwnd, NULL);
				return 1;
			case CTL->ID_CHAR_CNT:
			case CTL->ID_LAST_SVR:
				return 1;
			case CTL->ID_END_POPUP:
				printf("Received end popup message. Waiting for TL.exe termination.\n");
				while(IsWindow((HWND)wParam))
				{
					Sleep(10000);
				}
				abort();
				return 1;
			default:
				break;
			}
		case WM_APP:
			CTL->SendHello();
			return 1;
		case WM_APP + CTL->ID_SLS_URL:
			CTL->SendServerList();
			return 1;
		case WM_APP + CTL->ID_GAME_STR:
			CTL->SendAccountList();
			return 1;
		case WM_COMMAND:
			wmId    = LOWORD(wParam);
			wmEvent = HIWORD(wParam);
			switch (wmId)
			{
			case IDM_ABOUT:
				break;
			case IDM_EXIT:
				DestroyWindow(hwnd);
				break;
			default:
				return DefWindowProc(hwnd, message, wParam, lParam);
			}
			break;
		case WM_PAINT:
			hdc = BeginPaint(hwnd, &ps);
			EndPaint(hwnd, &ps);
			break;
		case WM_DESTROY:
			PostQuitMessage(0);
			break;
		default:
			return DefWindowProc(hwnd, message, wParam, lParam);
		}

		return message;
	}
};

unsigned char AccountData[] = { 0x7B,0x22,0x72,0x65,0x73,0x75,0x6C,0x74,0x2D,0x6D,0x65,0x73,0x73,0x61,0x67,0x65,0x22,0x3A,0x22,0x4F,0x4B,0x22,0x2C,0x22,0x61,0x63,0x63,0x65,0x73,0x73,0x5F,0x6C,0x65,0x76,0x65,0x6C,0x22,0x3A,0x30,0x2C,0x22,0x61,0x63,0x63,0x6F,0x75,0x6E,0x74,0x5F,0x62,0x69,0x74,0x73,0x22,0x3A,0x22,0x30,0x78,0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x22,0x2C,0x22,0x63,0x68,0x61,0x72,0x73,0x5F,0x70,0x65,0x72,0x5F,0x73,0x65,0x72,0x76,0x65,0x72,0x22,0x3A,0x5B,0x7B,0x22,0x63,0x68,0x61,0x72,0x5F,0x63,0x6F,0x75,0x6E,0x74,0x22,0x3A,0x22,0x32,0x22,0x2C,0x22,0x69,0x64,0x22,0x3A,0x22,0x31,0x31,0x22,0x7D,0x2C,0x7B,0x22,0x63,0x68,0x61,0x72,0x5F,0x63,0x6F,0x75,0x6E,0x74,0x22,0x3A,0x22,0x32,0x22,0x2C,0x22,0x69,0x64,0x22,0x3A,0x22,0x31,0x33,0x22,0x7D,0x5D,0x2C,0x22,0x75,0x73,0x65,0x72,0x5F,0x70,0x65,0x72,0x6D,0x69,0x73,0x73,0x69,0x6F,0x6E,0x22,0x3A,0x30,0x2C,0x22,0x6D,0x61,0x73,0x74,0x65,0x72,0x5F,0x61,0x63,0x63,0x6F,0x75,0x6E,0x74,0x5F,0x6E,0x61,0x6D,0x65,0x22,0x3A,0x22,0x54,0x42,0x35,0x4D,0x46,0x41,0x49,0x51,0x4F,0x35,0x22,0x2C,0x22,0x72,0x65,0x73,0x75,0x6C,0x74,0x2D,0x63,0x6F,0x64,0x65,0x22,0x3A,0x32,0x30,0x30,0x2C,0x22,0x6C,0x61,0x73,0x74,0x5F,0x63,0x6F,0x6E,0x6E,0x65,0x63,0x74,0x65,0x64,0x5F,0x73,0x65,0x72,0x76,0x65,0x72,0x5F,0x69,0x64,0x22,0x3A,0x31,0x33,0x2C,0x22,0x67,0x61,0x6D,0x65,0x5F,0x61,0x63,0x63,0x6F,0x75,0x6E,0x74,0x5F,0x6E,0x61,0x6D,0x65,0x22,0x3A,0x22,0x54,0x45,0x52,0x41,0x22,0x2C,0x22,0x74,0x69,0x63,0x6B,0x65,0x74,0x22,0x3A,0x22,0x70,0x65,0x66,0x64,0x58,0x43,0x34,0x4B,0x54,0x78,0x6E,0x44,0x61,0x41,0x67,0x46,0x47,0x78,0x6F,0x74,0x68,0x69,0x64,0x54,0x63,0x73,0x74,0x72,0x37,0x65,0x6D,0x57,0x65,0x73,0x34,0x41,0x6A,0x73,0x61,0x68,0x70,0x39,0x66,0x76,0x69,0x2D,0x6D,0x41,0x70,0x48,0x22,0x7D,0x00 };
unsigned int __stdcall OpenConsole(LPVOID nul)
{
#ifdef _DEBUG
	AllocConsole();

    HANDLE handle_out = GetStdHandle(STD_OUTPUT_HANDLE);
    int hCrt = _open_osfhandle((long) handle_out, _O_TEXT);
    FILE* hf_out = _fdopen(hCrt, "w");
    setvbuf(hf_out, NULL, _IONBF, 1);
    *stdout = *hf_out;

    HANDLE handle_in = GetStdHandle(STD_INPUT_HANDLE);
    hCrt = _open_osfhandle((long) handle_in, _O_TEXT);
    FILE* hf_in = _fdopen(hCrt, "r");
    setvbuf(hf_in, NULL, _IONBF, 128);
    *stdin = *hf_in;

	return 1;
#else
	return 0;
#endif
}

int APIENTRY _tWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPTSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
	char szCommandLine[1024];

	std::string out;
	std::vector<std::string> v_CmdLine;

	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	if(*lpCmdLine != 0)
	{
		WideCharToMultiByte(CP_ACP, 0, lpCmdLine, -1, szCommandLine, 1024, " ", NULL);

		int npos = 0;
		while(std::getline(std::stringstream(std::string(szCommandLine + npos)), out, ' '))
		{
			v_CmdLine.push_back(out);
			npos += out.length() + 1;
			if(npos >= strlen(szCommandLine))
			{
				break;
			}
		}
		if(v_CmdLine.size() > 2)
		{
			szTLPath = v_CmdLine[3].data();
		}
	}

	OpenConsole(NULL);

	EnumWindows(API_EW_CALLBACK, NULL);

	CTL = new CTLauncher(hInstance, v_CmdLine.empty() == false ? v_CmdLine[0].data() : NULL, v_CmdLine.size() > 1 ? v_CmdLine[1].data() : NULL, 
		v_CmdLine.empty() == false ? v_CmdLine[0].length() : 0, v_CmdLine.size() > 1 ? v_CmdLine[1].length() : 0);
	CTL->Initialize(hInstance);
}
