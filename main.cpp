#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN 1
#include <windows.h>
typedef PROCESS_INFORMATION ProcessType;
typedef HANDLE PipeType;
#define NULLPIPE NULL
#else
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/wait.h>
#include <signal.h>
#include <poll.h>
typedef pid_t ProcessType;
typedef int PipeType;
#define NULLPIPE -1
#endif
//#define DEBUG
#ifdef DEBUG
#include <fstream>
std::ofstream logfile;
#endif

#include "wrath_common.h"
#include "wrath_launcher.h"

#ifdef GOG
bool gog_bypass_drm;
#endif

#define DEBUGPIPE 1
#if DEBUGPIPE
#include <cstdio>
#include <cstdarg>
static void dbgpipe(const char *fmt, ...)
{
	va_list arg_list;
    va_start(arg_list, fmt);
	#ifdef DEBUG
    char str[4096];
    va_list args;
    va_start(args, fmt);

	vsnprintf(str, sizeof(str), fmt, args);
	printf(str);
	
	logfile.open("debugpipe.txt", std::ios_base::app | std::ios_base::out);
	logfile << str;
	logfile.close();
	#else
	printf(fmt, arg_list);
	#endif
	va_end(arg_list);
}
#else
static inline void dbgpipe(const char *fmt, ...) {};
#endif

PipeType pipeParentRead;
PipeType pipeParentWrite;
PipeType pipeChildRead;
PipeType pipeChildWrite;
ProcessType childPid;

bool	is_server;
char	EXECUTABLE_NAME[MAX_PIPESTRING];

/* platform-specific mainline calls this. */
static int mainline(int argc, char **argv);

/* Windows and Unix implementations of this stuff below. */
static void fail(const char *err);
static bool writePipe(PipeType fd, const void *buf, const unsigned int _len);
static int readPipe(PipeType fd, void *buf, const unsigned int _len);
static bool createPipes(PipeType *pPipeParentRead, PipeType *pPipeParentWrite,
                        PipeType *pPipeChildRead, PipeType *pPipeChildWrite);
static void closePipe(PipeType fd);
static bool setEnvVar(const char *key, const char *val);
static bool launchChild(ProcessType *pid, std::string argStr);
static int closeProcess(ProcessType *pid);

#ifdef _WIN32
static void fail(const char *err)
{
	#if DEBUG
	logfile.open("debugpipe.txt", std::ios_base::app | std::ios_base::out);
	logfile << "ERROR: " << err;
	logfile.close();
	#endif

	MessageBoxA(NULL, err, "ERROR", MB_ICONERROR | MB_OK);
	ExitProcess(1);
} // fail

static int pipeReady(PipeType fd)
{
	DWORD avail = 0;
	return (PeekNamedPipe(fd, NULL, 0, NULL, &avail, NULL) && (avail > 0));
} /* pipeReady */

static bool writePipe(PipeType fd, const void *buf, const unsigned int _len)
{
	const DWORD len = (DWORD)_len;
	DWORD bw = 0;
	return ((WriteFile(fd, buf, len, &bw, NULL) != 0) && (bw == len));
} // writePipe

static int readPipe(PipeType fd, void *buf, const unsigned int _len)
{
	DWORD avail = 0;
	PeekNamedPipe(fd, NULL, 0, NULL, &avail, NULL);
	if (avail < _len)
		return 0;

	const DWORD len = (DWORD)_len;
	DWORD br = 0;

	return ReadFile(fd, buf, len, &br, NULL) ? (int)br : 0;
} // readPipe

static bool createPipes(PipeType *pPipeParentRead, PipeType *pPipeParentWrite,
	PipeType *pPipeChildRead, PipeType *pPipeChildWrite)
{
	SECURITY_ATTRIBUTES pipeAttr;

	pipeAttr.nLength = sizeof(pipeAttr);
	pipeAttr.lpSecurityDescriptor = NULL;
	pipeAttr.bInheritHandle = TRUE;
	if (!CreatePipe(pPipeParentRead, pPipeChildWrite, &pipeAttr, 0))
		return 0;

	pipeAttr.nLength = sizeof(pipeAttr);
	pipeAttr.lpSecurityDescriptor = NULL;
	pipeAttr.bInheritHandle = TRUE;
	if (!CreatePipe(pPipeChildRead, pPipeParentWrite, &pipeAttr, 0))
	{
		CloseHandle(*pPipeParentRead);
		CloseHandle(*pPipeChildWrite);
		return 0;
	} // if
	
	return 1;
} // createPipes

static void closePipe(PipeType fd)
{
	CloseHandle(fd);
} // closePipe

static bool setEnvVar(const char *key, const char *val)
{

	return (SetEnvironmentVariableA(key, val) != 0);
} // setEnvVar

static bool launchChild(ProcessType *pid, std::string argStr)
{
	STARTUPINFO info = { sizeof(info) };

	std::string buf(".\\");
	buf.append(EXECUTABLE_NAME);

	char argbuf[8192];
	strcpy(argbuf, argStr.c_str());

	return (CreateProcessA(buf.c_str(),
		argbuf, NULL, NULL, TRUE, 0, NULL,
		NULL, &info, pid) != 0);
} // launchChild

static int closeProcess(ProcessType *pid)
{
	CloseHandle(pid->hProcess);
	CloseHandle(pid->hThread);
	return 0;
} // closeProcess

int CALLBACK WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	mainline(__argc, __argv);
	ExitProcess(0);
	return 0;  // just in case.
} // WinMain
#else  // everyone else that isn't Windows.

static void fail(const char *err)
{
	#if DEBUG
	logfile.open("debugpipe.txt", std::ios_base::app | std::ios_base::out);
	logfile << "ERROR: " << err;
	logfile.close();
	#endif

	// !!! FIXME: zenity or something.
	fprintf(stderr, "%s\n", err);
	_exit(1);
} // fail

static int pipeReady(PipeType fd)
{
	int rc;
	struct pollfd pfd = { fd, POLLIN | POLLERR | POLLHUP, 0 };
	while (((rc = poll(&pfd, 1, 0)) == -1) && (errno == EINTR)) { /*spin*/ }
	return (rc == 1);
} /* pipeReady */

static bool writePipe(PipeType fd, const void *buf, const unsigned int _len)
{
	const ssize_t len = (ssize_t)_len;
	ssize_t bw;
	while (((bw = write(fd, buf, len)) == -1) && (errno == EINTR)) { /*spin*/ }
	return (bw == len);
} // writePipe

static int readPipe(PipeType fd, void *buf, const unsigned int _len)
{
	if (!pipeReady(fd))
		return 0;

	return read(fd, buf, (ssize_t)_len);
	//const ssize_t len = (ssize_t)_len;
	//ssize_t br;
	//while (((br = read(fd, buf, len)) == -1) && (errno == EINTR)) { /*spin*/ }
	//return (int)br == -1 ? (int)br : 0;
} // readPipe

static bool createPipes(PipeType *pPipeParentRead, PipeType *pPipeParentWrite,
	PipeType *pPipeChildRead, PipeType *pPipeChildWrite)
{
	int fds[2];
	if (pipe(fds) == -1)
		return 0;
	*pPipeParentRead = fds[0];
	*pPipeChildWrite = fds[1];

	if (pipe(fds) == -1)
	{
		close(*pPipeParentRead);
		close(*pPipeChildWrite);
		return 0;
	} // if

	*pPipeChildRead = fds[0];
	*pPipeParentWrite = fds[1];

	return 1;
} // createPipes

static void closePipe(PipeType fd)
{
	close(fd);
} // closePipe

static bool setEnvVar(const char *key, const char *val)
{
	return (setenv(key, val, 1) != -1);
} // setEnvVar

static int GArgc = 0;
static char **GArgv = NULL;

static bool launchChild(ProcessType *pid, std::string argStr)
{
	*pid = fork();
	if (*pid == -1)   // failed
		return false;
	else if (*pid != 0)  // we're the parent
		return true;  // we'll let the pipe fail if this didn't work.

	std::string buf("./");
	buf.append(EXECUTABLE_NAME);

	GArgv[1] = (char*)argStr.c_str();

	// we're the child.
	GArgv[0] = strdup(buf.c_str());
	execvp(GArgv[0], GArgv);
	// still here? It failed! Terminate, closing child's ends of the pipes.
	_exit(1);
} // launchChild

static int closeProcess(ProcessType *pid)
{
	int rc = 0;
	while ((waitpid(*pid, &rc, 0) == -1) && (errno == EINTR)) { /*spin*/ }
	if (!WIFEXITED(rc))
		return 1;  // oh well.
	return WEXITSTATUS(rc);
} // closeProcess

int main(int argc, char **argv)
{
	signal(SIGPIPE, SIG_IGN);
	GArgc = argc;
	GArgv = argv;
	return mainline(argc, argv);
} // main

#endif

namespace fs = ghc::filesystem;
using namespace std;

string dirGame;
string dirSteamTemp;
#ifdef STEAM
char steam_UserName[MAX_PIPESTRING];
const char *steam_Language;
CSteamID steam_LocalID;
#endif

string convertToString(char* a, int size)
{
	int i;
	string s = "";
	for (i = 0; i < size; i++) {
		s = s + a[i];
	}
	return s;
}

typedef struct
{
	unsigned char	data[MAX_PIPEBUFFSIZE];
	int				cursize;
} pipebuff_t;

pipebuff_t pipeSendBuffer;

int PIPE_SendData(void)
{
	unsigned long bytes_written = 0;
	int succ = 0;
	succ = writePipe(pipeParentWrite, pipeSendBuffer.data, pipeSendBuffer.cursize);
	pipeSendBuffer.cursize = 0;

	return succ;
}

float PIPE_ReadFloat(void)
{
	float dat = 0;
	int succ = 0;
#if 0
	succ = ReadFile(hPipeServer, &dat, 4, NULL, NULL);
#endif
	succ = readPipe(pipeParentRead, &dat, 4);

	if (!succ)
	{
		return -1;
	}

	return dat;
}


signed long PIPE_ReadLong(void)
{
	signed long dat = 0;
	int succ = 0;
#if 0
	succ = ReadFile(hPipeServer, &dat, 4, NULL, NULL);
#endif
	succ = readPipe(pipeParentRead, &dat, 4);

	if (!succ)
	{
		return -1;
	}

	return dat;
}


long long PIPE_ReadLongLong(void)
{
	long long dat = 0;
	int succ = 0;
#if 0
	succ = ReadFile(hPipeServer, &dat, 8, NULL, NULL);
#endif
	succ = readPipe(pipeParentRead, &dat, 8);

	if (!succ)
	{
		return -1;
	}

	return dat;
}


signed short PIPE_ReadShort(void)
{
	signed short dat = 0;
	int succ = 0;
#if 0
	succ = ReadFile(hPipeServer, &dat, 2, NULL, NULL);
#endif
	succ = readPipe(pipeParentRead, &dat, 2);

	if (!succ)
	{
		return -1;
	}

	return dat;
}


unsigned char PIPE_ReadByte(void)
{
	unsigned char dat = 0;
	int succ = 0;
#if 0
	succ = ReadFile(hPipeServer, &dat, 1, NULL, NULL);
#endif
	succ = readPipe(pipeParentRead, &dat, 1);

	if (!succ)
	{
		return -1;
	}

	return dat;
}


int PIPE_ReadString(char *buff)
{
	unsigned long amount_written;

	int i;
	for (i = 0; i < MAX_PIPESTRING; i++)
	{
#if 0
		ReadFile(hPipeServer, buff + i, 1, NULL, NULL);
#endif
		readPipe(pipeParentRead, buff + i, 1);
		if (buff[i] == 0)
			break;
	}
	amount_written = i;

	return amount_written;
}


void PIPE_ReadCharArray(char *into, unsigned long *size)
{
	*size = (unsigned long)PIPE_ReadShort();
	int succ = 0;
#if 0
	succ = ReadFile(hPipeServer, into, *size, size, NULL);
#endif
	succ = readPipe(pipeParentRead, into, *size);
	*size = (unsigned long)succ;
	if (!succ)
	{
		*size = 0;
	}
}

int PIPE_WriteFloat(float dat_float)
{
	long dat;
	memcpy(&dat, &dat_float, sizeof(long));

	int seek = pipeSendBuffer.cursize;
	pipeSendBuffer.data[seek] = dat & 0xFF;
	pipeSendBuffer.data[seek + 1] = (dat >> 8) & 0xFF;
	pipeSendBuffer.data[seek + 2] = (dat >> 16) & 0xFF;
	pipeSendBuffer.data[seek + 3] = (dat >> 24) & 0xFF;

	pipeSendBuffer.cursize += 4;

	return true;
}


int PIPE_WriteLong(signed long dat)
{
	int seek = pipeSendBuffer.cursize;
	pipeSendBuffer.data[seek] = dat & 0xFF;
	pipeSendBuffer.data[seek + 1] = (dat >> 8) & 0xFF;
	pipeSendBuffer.data[seek + 2] = (dat >> 16) & 0xFF;
	pipeSendBuffer.data[seek + 3] = (dat >> 24) & 0xFF;

	pipeSendBuffer.cursize += 4;

	return true;
}


int PIPE_WriteLongLong(long long dat)
{
	int seek = pipeSendBuffer.cursize;
	pipeSendBuffer.data[seek] = dat & 0xFF;
	pipeSendBuffer.data[seek + 1] = (dat >> 8) & 0xFF;
	pipeSendBuffer.data[seek + 2] = (dat >> 16) & 0xFF;
	pipeSendBuffer.data[seek + 3] = (dat >> 24) & 0xFF;
	pipeSendBuffer.data[seek + 4] = (dat >> 32) & 0xFF;
	pipeSendBuffer.data[seek + 5] = (dat >> 40) & 0xFF;
	pipeSendBuffer.data[seek + 6] = (dat >> 48) & 0xFF;
	pipeSendBuffer.data[seek + 7] = (dat >> 56) & 0xFF;

	pipeSendBuffer.cursize += 8;

	return true;
}


int PIPE_WriteShort(signed short dat)
{
	int seek = pipeSendBuffer.cursize;
	pipeSendBuffer.data[seek] = dat & 0xFF;
	pipeSendBuffer.data[seek + 1] = (dat >> 8) & 0xFF;

	pipeSendBuffer.cursize += 2;

	return true;
}


int PIPE_WriteByte(unsigned char dat)
{
	int seek = pipeSendBuffer.cursize;
	pipeSendBuffer.data[seek] = dat;
	pipeSendBuffer.cursize += 1;

	return true;
}


int PIPE_WriteString(const char *str)
{
	int str_length = strlen(str);
	memcpy(&(pipeSendBuffer.data[pipeSendBuffer.cursize]), str, str_length);

	if (str[str_length - 1] != 0)
		pipeSendBuffer.data[pipeSendBuffer.cursize + str_length] = 0; str_length++;

	pipeSendBuffer.cursize += str_length;

	return true;
}


int PIPE_WriteCharArray(char *dat, unsigned long size)
{
	PIPE_WriteShort((signed short)size);

	int seek = pipeSendBuffer.cursize;
	memcpy(&(pipeSendBuffer.data[seek]), dat, size);
	pipeSendBuffer.cursize += size;

	return true;
}


void Con_Print(const char *dat)
{
#if 1
	PIPE_WriteByte(SV_PRINT);
	PIPE_WriteString((char*)dat);
#else
	cout << dat;
#endif
}


// Steam Functions

#ifdef STEAM
AppId_t GAppID = 0;
uint64 GUserID = 0;
ISteamApps *GSteamApps = NULL;
ISteamUserStats *GSteamStats = NULL;
ISteamUtils *GSteamUtils = NULL;
ISteamUser *GSteamUser = NULL;
ISteamUserStats *GSteamUserStats = NULL;
ISteamFriends *GSteamFriends = NULL;
#endif

#ifdef EPIC
EOS_HPlatform GEpicPlatform;
EOS_HUserInfo GEpicUserInfo;
EOS_HAchievements GEpicAchievements;
EOS_HStats GEpicStats;
EOS_HAuth GEpicAuth;
EOS_HConnect GEpicConnect;

char epic_devauth_ip[1024] = {0};
char epic_devauth_name[1024] = {0};
char epic_auth_password[1024] = {0};
bool epic_auth_exchangecode = false;
#endif

void(*func_readarray[CL_MAX])();

void DRM_Cleanup(void)
{
	//if (fs::is_regular_file(dirSteamTemp + "authtoken"))
	//	fs::remove(dirSteamTemp + "authtoken");

	if (fs::is_directory(dirSteamTemp) && !fs::is_empty(dirSteamTemp))
	{
		fs::remove_all(dirSteamTemp);
	}
}

// THE ACTUAL PROGRAM.
#ifdef STEAM
bool steam_AchievementStatsPending;
static int steam_StatsCooldown;
void Steam_StoreStats(void)
{
	if (!steam_AchievementStatsPending)
		return;

	if (steam_StatsCooldown > 0)
		return;

	Con_Print("STEAM PIPE: (Server) storing stats\n");
	GSteamUserStats->StoreStats();

	steam_AchievementStatsPending = false;
#ifdef _WIN32 // lazy fix for unix running at 1hz instead of 10hz because of the sleep command
	steam_StatsCooldown = 80;
#else
	steam_StatsCooldown = 8;
#endif
}
#endif

void processCommands(void)
{
	while (true)
	{
		#ifdef STEAM
		if (is_server)
			SteamGameServer_RunCallbacks();
		SteamAPI_RunCallbacks();
		steam_StatsCooldown--;
		#endif
		#ifdef EPIC
		EOS_Platform_Tick(GEpicPlatform);
		
		if (!epic_auth_checked)
		{
			EOS_Platform_GetDesktopCrossplayStatusOptions dcpsOptions = {};
			dcpsOptions.ApiVersion = EOS_PLATFORM_GETDESKTOPCROSSPLAYSTATUS_API_LATEST;
			
			EOS_Platform_DesktopCrossplayStatusInfo dcpsOutput = {};
			EOS_EResult result = EOS_Platform_GetDesktopCrossplayStatus(GEpicPlatform, &dcpsOptions, &dcpsOutput);
			if (result == EOS_EResult::EOS_Success)
			{
				cout << "Checking epic auth" << "\n";
				Epic_Auth();
				epic_auth_checked = true;
			}
		}
		#endif

		unsigned char index = PIPE_ReadByte();
		while (index != 255)
		{
			//cout << "reading " << to_string(index) << endl;
			//char dbgmsg[256];
			//sprintf(dbgmsg, "STEAM PIPE: (Server) reading packet %i\n", (int)index);
			//Con_Print(dbgmsg);

			if (index < CL_MAX)
			{
				if (func_readarray[index])
					func_readarray[index]();
			}
			else
			{
				Con_Print("STEAM PIPE: (Server) bad packet read\n");
			}

			index = PIPE_ReadByte();
		}

		#ifdef STEAM
		Steam_StoreStats();
		#endif
		#ifdef EPIC
		Epic_QueryAchievements();
		#endif

		if (pipeSendBuffer.cursize)
			PIPE_SendData();

#ifdef _WIN32
		if (!WaitForSingleObject(childPid.hProcess, 0))
		{
			DRM_Cleanup();
			exit(0);
		}
		

		Sleep(100);
#else
		int status = waitpid(childPid, NULL, WNOHANG);
		if (status < 0)
		{
			DRM_Cleanup();
			exit(0);
		}


		sleep(1);
#endif
	}
} // processCommands

static bool setEnvironmentVars(PipeType pipeChildRead, PipeType pipeChildWrite)
{
	dbgpipe("%s\n", __func__);

    char buf[64];
    snprintf(buf, sizeof (buf), "%llu", (unsigned long long) pipeChildRead);
    if (!setEnvVar("STEAMSHIM_READHANDLE", buf))
        return false;

    snprintf(buf, sizeof (buf), "%llu", (unsigned long long) pipeChildWrite);
    if (!setEnvVar("STEAMSHIM_WRITEHANDLE", buf))
        return false;

    return true;
} // setEnvironmentVars

#ifdef STEAM
static bool initSteamworks(PipeType fd)
{
    // this can fail for many reasons:
    //  - you forgot a steam_appid.txt in the current working directory.
    //  - you don't have Steam running
    //  - you don't own the game listed in steam_appid.txt
	if (!SteamAPI_Init())
	{
#ifdef GOG
		gog_bypass_drm = true;
		return 1;
#else
		return 0;
#endif
	}


	GSteamApps = SteamApps();
	GSteamStats = SteamUserStats();
	GSteamUtils = SteamUtils();
	GSteamUser = SteamUser();
	GSteamUserStats = SteamUserStats();
	GSteamFriends = SteamFriends();
#if ACHIEVEMENT_HARDCODE
	g_SteamAchievements = new CSteamAchievements(g_Achievements, ACHIEVEMENTS_MAX, g_Stats, STATS_MAX);
#else
	g_SteamAchievements = new CSteamAchievements();
#endif

    //GAppID = GSteamUtils ? GSteamUtils->GetAppID() : 0;
	//GUserID = GSteamUser ? GSteamUser->GetSteamID().ConvertToUint64() : 0;

    return 1;
} // initSteamworks

static void deinitSteamworks(void)
{
    SteamAPI_Shutdown();
	GSteamApps = NULL;
    GSteamStats = NULL;
    GSteamUtils= NULL;
    GSteamUser = NULL;
	GSteamUserStats = NULL;
	GSteamFriends = NULL;
} // deinitSteamworks
#endif
#ifdef EPIC
static bool initEpicOnlineServices(PipeType fd)
{
	dbgpipe("%s\n", __func__);
	
	EOS_InitializeOptions iOptions = {};
	iOptions.ApiVersion = EOS_INITIALIZE_API_LATEST;
	iOptions.ProductName = "Wrath: Aeon of Ruin";
	iOptions.ProductVersion = "rc1";

    if (EOS_Initialize(&iOptions) != EOS_EResult::EOS_Success)
        return 0;

	EOS_Platform_ClientCredentials clCreds = {};
	clCreds.ClientId = "xyza7891NceGdToaybkcmgKdXwh84QEw";
	clCreds.ClientSecret = "PqNNTSh/URJXrfdLAgyKA6GATIvXyC/7x7xfv8Ke5Ks";
	EOS_Platform_Options pOptions = {};
	pOptions.ClientCredentials = clCreds;
	pOptions.ApiVersion = EOS_PLATFORM_OPTIONS_API_LATEST;
	pOptions.ProductId = "96cf5177ed1c481d88a2b609f3c68fac";
	pOptions.SandboxId = "p-9zp38bj4z5us3dlcr8dyaxyvqcqyyk";
	pOptions.DeploymentId = "b9a36391a48c4f6ca4448164ae36c82a";
	pOptions.Flags = EOS_PF_WINDOWS_ENABLE_OVERLAY_OPENGL;
	
	EOS_Logging_SetLogLevel(EOS_ELogCategory::EOS_LC_ALL_CATEGORIES, EOS_ELogLevel::EOS_LOG_VeryVerbose);
	EOS_Logging_SetCallback(Epic_LoggingFunc);

	GEpicPlatform = EOS_Platform_Create(&pOptions);
	GEpicUserInfo = EOS_Platform_GetUserInfoInterface(GEpicPlatform);
	GEpicAchievements = EOS_Platform_GetAchievementsInterface(GEpicPlatform);
	GEpicStats = EOS_Platform_GetStatsInterface(GEpicPlatform);
	GEpicAuth = EOS_Platform_GetAuthInterface(GEpicPlatform);
	GEpicConnect = EOS_Platform_GetConnectInterface(GEpicPlatform);

    return 1;
} // initEpicOnlineServices

static void deinitEpicOnlineServices(void)
{
	EOS_Shutdown();
	GEpicPlatform = NULL;
	GEpicUserInfo = NULL;
	GEpicAchievements = NULL;
	GEpicStats = NULL;
	GEpicAuth = NULL;
	GEpicConnect = NULL;
}
#endif

static int mainline(int argc, char **argv)
{
	stringstream argStr;
    pipeParentRead = NULLPIPE;
    pipeParentWrite = NULLPIPE;
    pipeChildRead = NULLPIPE;
    pipeChildWrite = NULLPIPE;
	#ifdef STEAM
	steam_StatsCooldown = 0;
	#endif
#ifdef GOG
	gog_bypass_drm = false;
#endif

	#ifdef DEBUG
	logfile.open("debugpipe.txt");
	logfile.close();
	#endif

    dbgpipe("Parent starting mainline.\n");

#ifdef _WIN32
	strcpy_lazy(EXECUTABLE_NAME, "wrath-sdl.exe");
#else
	strcpy_lazy(EXECUTABLE_NAME, "wrath-sdl");
#endif

#pragma region ARGUMENT GENERATION
	int i;
#ifdef _WIN32
	i = 0;
#else
	i = 1;
#endif
	for (; i < argc; i++)
	{
		{
#ifdef _WIN32
			if (i == 0)
#else
			if (i == 1)
#endif
				argStr << " \"" << argv[i] << "\"";
			else
				argStr << " " << argv[i];

			if (!stricmp(argv[i], "-nosdl"))
			{
#ifdef _WIN32
				strcpy_lazy(EXECUTABLE_NAME, "wrath.exe");
#else
				strcpy_lazy(EXECUTABLE_NAME, "wrath-64-glx");
#endif
				continue;
			}
			else if (!stricmp(argv[i], "-server"))
			{
#ifdef _WIN32
				strcpy_lazy(EXECUTABLE_NAME, "wrath-dedicated.exe");
#else
				strcpy_lazy(EXECUTABLE_NAME, "wrath-dedicated");
#endif
				continue;
			}
			#ifdef EPIC
			else if (!substrcmp(argv[i], "-AUTH_PASSWORD="))
			{
				strcpy(epic_auth_password, argv[i] + strlen("-AUTH_PASSWORD="));
			}
			else if (!strcmp(argv[i], "-AUTH_TYPE=exchangecode"))
			{
				epic_auth_exchangecode = true;
			}
			else if (!substrcmp(argv[i], "-DEVAUTH_IP="))
			{
				strcpy(epic_devauth_ip, argv[i] + strlen("-DEVAUTH_IP="));
			}
			else if (!substrcmp(argv[i], "-DEVAUTH_NAME="))
			{
				strcpy(epic_devauth_name, argv[i] + strlen("-DEVAUTH_NAME="));
			}
			#endif
		}
	}
#pragma endregion

	if (!EXECUTABLE_NAME[0]) // executable string is null, wtf?
	{
		fail("Null executable chosen");
	}

	argStr << " -wrath";

	dbgpipe("Starting child process %s.\n", EXECUTABLE_NAME);
	dbgpipe("%s\n", argStr.str().c_str());


	if (!createPipes(&pipeParentRead, &pipeParentWrite, &pipeChildRead, &pipeChildWrite))
		fail("Failed to create application pipes");
	#ifdef STEAM
	else if (!initSteamworks(pipeParentWrite))
		fail("Failed to initialize Steamworks");
	#endif
	#ifdef EPIC
	else if (!initEpicOnlineServices(pipeParentWrite))
		fail("Failed to initialize Epic Online Services");
	#endif
	else if (!setEnvironmentVars(pipeChildRead, pipeChildWrite))
		fail("Failed to set environment variables");
	else if (!launchChild(&childPid, argStr.str()))
		fail("Failed to launch application");

	dbgpipe("Parent startup finished.\n");

    // Close the ends of the pipes that the child will use; we don't need them.
    closePipe(pipeChildRead);
    closePipe(pipeChildWrite);
    pipeChildRead = pipeChildWrite = NULLPIPE;

#ifdef GOG
	if (gog_bypass_drm) // steam wrapper didn't launch... just leave the game be
	{
		return 0;
	}
#endif

	DRM_Init();

	dbgpipe("DRM initialized.\n");

	#ifdef STEAM
	if (is_server)
	{
		//steam_LocalID = SteamGameServer()->GetSteamID();
	}
	else
	{
		strcpy(steam_UserName, GSteamFriends->GetPersonaName());
		steam_LocalID = GSteamUser->GetSteamID();
		steam_Language = GSteamApps->GetCurrentGameLanguage();
	}
	#endif

	dbgpipe("Parent in command processing loop.\n");
	// Now, we block for instructions until the pipe fails (child closed it or
	//  terminated/crashed).
	//processCommands(pipeParentRead, pipeParentWrite);
	processCommands();

	dbgpipe("Parent shutting down.\n");

	// Close our ends of the pipes.
	closePipe(pipeParentRead);
	closePipe(pipeParentWrite);

	#ifdef STEAM
	deinitSteamworks();
	#endif
	#ifdef EPIC
	deinitEpicOnlineServices();
	#endif

	dbgpipe("Parent waiting on child process.\n");

	// Wait for the child to terminate, close the child process handles.
	const int retval = closeProcess(&childPid);

	dbgpipe("Parent exiting mainline (child exit code %d).\n", retval);

	return retval;
} // mainline

// end of steamshim_parent.cpp ...

