#pragma once

#ifdef STEAM
#include "steam/steam_api.h"
#endif
#ifdef EPIC
#include "epic/eos_sdk.h"
#include "epic/eos_achievements.h"
#include "epic/eos_auth.h"
#include "epic/eos_logging.h"
#endif
#ifdef GOG

#endif

#include "ghc/filesystem.hpp" // replaced std::filesystem with this, for C++11 support for Steam Runtime Scout
#include <string>
#include <sstream>
#include <iostream>

#ifndef _WIN32 // linux has a different name for it
#define strnicmp strncasecmp
#define stricmp strcasecmp
#define strcpy_s(s1, num, s2) strncpy(s1, s2, num)
#define _snprintf snprintf
#endif
#define strcpy_lazy(s1, s2) strcpy_s(s1, MAX_PIPESTRING, s2)

#define substrcmp(s1, s2) strncmp(s1, s2, sizeof(s2) - 1)
#define substricmp(s1, s2) strnicmp(s1, s2, sizeof(s2) - 1)

extern bool is_server;
extern void(*func_readarray[CL_MAX])();

void Language_SendToGame(const char *langcode);
void Con_Print(const char *dat);
void DRM_Init(void);

typedef enum controllertype_e {
	CONTROLLER_NULL,
	CONTROLLER_GENERIC,
	CONTROLLER_XBOX,
	CONTROLLER_PLAYSTATION,
	CONTROLLER_NINTENDO,
	CONTROLLER_STEAM
} controllertype_t;

#ifdef STEAM
class CSteamAchievements
{
private:
	int64 m_iAppID; // Our current AppID
#if ACHIEVEMENT_HARDCODE
	Achievement_t *m_pAchievements; // Achievements data
	Stat_t *m_pStats; // Stats data
	int m_iNumAchievements; // The number of Achievements
	int m_iNumStats;
#endif
	bool m_bInitialized; // Have we called Request stats and received the callback?

public:
#if ACHIEVEMENT_HARDCODE
	CSteamAchievements(Achievement_t *Achievements, int NumAchievements, Stat_t *Stats, int NumStats);
#else
	CSteamAchievements();
#endif

	bool RequestStats();
	bool SetAchievement(const char* ID);

	STEAM_CALLBACK(CSteamAchievements, OnUserStatsReceived, UserStatsReceived_t, m_CallbackUserStatsReceived);
	STEAM_CALLBACK(CSteamAchievements, OnUserStatsStored, UserStatsStored_t, m_CallbackUserStatsStored);
	STEAM_CALLBACK(CSteamAchievements, OnAchievementStored, UserAchievementStored_t, m_CallbackAchievementStored);
};


extern AppId_t GAppID;
extern uint64 GUserID;
extern ISteamUserStats *GSteamStats;
extern ISteamUtils *GSteamUtils;
extern ISteamUser *GSteamUser;
extern ISteamUserStats *GSteamUserStats;
extern ISteamFriends *GSteamFriends;
extern CSteamAchievements* g_SteamAchievements;
extern char steam_UserName[];
extern const char *steam_Language;
extern bool steam_AchievementStatsPending;
extern CSteamID steam_LocalID;
#endif
#ifdef EPIC
void Epic_Auth(void);
void Epic_QueryAchievements(void);
void EOS_CALL Epic_LoggingFunc(const EOS_LogMessage *Message);

extern char epic_auth_password[1024];
extern bool epic_auth_exchangecode;
extern char epic_devauth_ip[1024];
extern char epic_devauth_name[1024];

extern bool epic_auth_checked;
extern EOS_HPlatform GEpicPlatform;
extern EOS_HUserInfo GEpicUserInfo;
extern EOS_HAchievements GEpicAchievements;
extern EOS_HStats GEpicStats;
extern EOS_HAuth GEpicAuth;
extern EOS_HConnect GEpicConnect;
#endif

