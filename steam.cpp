
#ifdef STEAM

#include "wrath_common.h"
#include "wrath_launcher.h"
#include <string.h>
#include <stdio.h>

namespace fs = ghc::filesystem;
using namespace std;

CSteamAchievements* g_SteamAchievements = NULL;
#if ACHIEVEMENT_HARDCODE
Achievement_t g_Achievements[] =
{
	ACHIEVEMENT_LIST
};
Stat_t g_Stats[] =
{
	STAT_LIST
};

CSteamAchievements::CSteamAchievements(Achievement_t *Achievements, int NumAchievements, Stat_t *Stats, int NumStats) :
	m_iAppID(0),
	m_bInitialized(false),
	m_CallbackUserStatsReceived(this, &CSteamAchievements::OnUserStatsReceived),
	m_CallbackUserStatsStored(this, &CSteamAchievements::OnUserStatsStored),
	m_CallbackAchievementStored(this, &CSteamAchievements::OnAchievementStored)
{
	m_iAppID = SteamUtils()->GetAppID();
	m_pAchievements = Achievements;
	m_iNumAchievements = NumAchievements;
	m_pStats = Stats;
	m_iNumStats = NumStats;
	RequestStats();
}
#else
steamachievement_t g_Achievements[MAX_STEAM_REGISTERED_FIELDS];
steamstat_t g_Stats[MAX_STEAM_REGISTERED_FIELDS];

CSteamAchievements::CSteamAchievements() :
	m_iAppID(0),
	m_bInitialized(false),
	m_CallbackUserStatsReceived(this, &CSteamAchievements::OnUserStatsReceived),
	m_CallbackUserStatsStored(this, &CSteamAchievements::OnUserStatsStored),
	m_CallbackAchievementStored(this, &CSteamAchievements::OnAchievementStored)
{
	m_iAppID = SteamUtils()->GetAppID();
	RequestStats();

}
#endif

bool CSteamAchievements::RequestStats()
{
	// Is Steam loaded? If not we can't get stats.
	if (NULL == SteamUserStats() || NULL == SteamUser())
	{
		return false;
	}
	// Is the user logged on?  If not we can't get stats.
	if (!SteamUser()->BLoggedOn())
	{
		return false;
	}
	// Request user stats.
	return SteamUserStats()->RequestCurrentStats();
}

bool CSteamAchievements::SetAchievement(const char* ID)
{
	// Have we received a call back from Steam yet?
	if (m_bInitialized)
	{
		SteamUserStats()->SetAchievement(ID);
		return SteamUserStats()->StoreStats();
	}
	// If not then we can't set achievements yet
	return false;
}

void CSteamAchievements::OnUserStatsReceived(UserStatsReceived_t *pCallback)
{
	// we may get callbacks for other games' stats arriving, ignore them
	if (m_iAppID == pCallback->m_nGameID)
	{
		if (k_EResultOK == pCallback->m_eResult)
		{
			Con_Print("Received stats and achievements from Steam\n");
			m_bInitialized = true;

#if ACHIEVEMENT_HARDCODE
			// load achievements
			for (int iAch = 0; iAch < m_iNumAchievements; ++iAch)
			{
				Achievement_t &ach = m_pAchievements[iAch];

				GSteamUserStats->GetAchievement(ach.m_pchAchievementID, &ach.m_bAchieved);
				snprintf(ach.m_rgchName, sizeof(ach.m_rgchName), "%s", GSteamUserStats->GetAchievementDisplayAttribute(ach.m_pchAchievementID, "name"));
				snprintf(ach.m_rgchDescription, sizeof(ach.m_rgchDescription), "%s", GSteamUserStats->GetAchievementDisplayAttribute(ach.m_pchAchievementID, "desc"));

				PIPE_WriteByte(SV_ACHIEVEMENT_VALUE);
				PIPE_WriteString(ach.m_pchAchievementID);
				PIPE_WriteByte(ach.m_bAchieved);
			}

			// load stats
			for (int iStat = 0; iStat < m_iNumStats; ++iStat)
			{
				Stat_t &stat = m_pStats[iStat];
				float val;

				GSteamUserStats->GetStat(stat.m_pchStatID, &val);

				#if 0 // do we even need to do this?? no clue how redefinitions work
				// cast value to a float or type-pun it
				if (stat.m_iType)
					memcpy(&stat.m_fValue, &val, sizeof(stat.m_fValue));
				else
					stat.m_fValue = (float)val;
				//
				#else // no we don't, this shit uses overloads
				stat.m_fValue = val;
				#endif
			}
#else
			// cheevos
			for (int i = 0; i < MAX_STEAM_REGISTERED_FIELDS; i++)
			{
				steamachievement_t *achieve = &g_Achievements[i];
				if (!achieve->m_pchAchievementID[0])
					continue;

				GSteamUserStats->GetAchievement(achieve->m_pchAchievementID, &achieve->m_bAchieved);

				PIPE_WriteByte(SV_ACHIEVEMENT_VALUE);
				PIPE_WriteString(achieve->m_pchAchievementID);
				PIPE_WriteByte(achieve->m_bAchieved);
			}

			// stats
			for (int i = 0; i < MAX_STEAM_REGISTERED_FIELDS; i++)
			{
				int ivalue;
				float value;
				steamstat_t *stat = &g_Stats[i];
				if (!stat->m_pchStatID[0])
					continue;
				
				if (stat->m_iType)
				{
					GSteamUserStats->GetStat(stat->m_pchStatID, &value);
					ivalue = (int)value;
				}
				else
				{
					GSteamUserStats->GetStat(stat->m_pchStatID, &ivalue);
					value = (float)ivalue;
				}

				stat->m_fValue = value;

				PIPE_WriteByte(SV_STAT_VALUE);
				PIPE_WriteString(stat->m_pchStatID);
				PIPE_WriteFloat(stat->m_fValue);
			}
#endif
		}
		else
		{
			char buffer[128];
			_snprintf(buffer, 128, "RequestStats - failed, %d\n", pCallback->m_eResult);
			Con_Print(buffer);
		}
	}
}

void CSteamAchievements::OnUserStatsStored(UserStatsStored_t *pCallback)
{
	// we may get callbacks for other games' stats arriving, ignore them
	if (m_iAppID == pCallback->m_nGameID)
	{
		if (k_EResultOK == pCallback->m_eResult)
		{
			Con_Print("Stored stats for Steam\n");
		}
		else
		{
			char buffer[128];
			_snprintf(buffer, 128, "StatsStored - failed, %d\n", pCallback->m_eResult);
			Con_Print(buffer);
		}
	}
}

void CSteamAchievements::OnAchievementStored(UserAchievementStored_t *pCallback)
{
	// we may get callbacks for other games' stats arriving, ignore them
	if (m_iAppID == pCallback->m_nGameID)
	{
		Con_Print("Stored Achievement for Steam\n");

		if (!pCallback->m_nCurProgress && !pCallback->m_nMaxProgress) // (via documentation) this means we unlocked the achievement, so give the engine a shout!
		{
			// write packet to the engine
			PIPE_WriteByte(SV_ACHIEVEMENT_VALUE);
			PIPE_WriteString(pCallback->m_rgchAchievementName);
			PIPE_WriteByte(true);
		}
	}
}


void PacketRead_Handshake(void)
{
	PIPE_WriteByte(SV_STEAMID);
	PIPE_WriteString((char*)(to_string(steam_LocalID.ConvertToUint64())).c_str());
	
	if (is_server)
		return;

	PIPE_WriteByte(SV_SETNAME);
	PIPE_WriteString(steam_UserName);

	if (steam_Language != NULL)
		Language_SendToGame(steam_Language);
}


void PacketRead_Achievement_Set(void)
{
	char ach_name[MAX_PIPESTRING];
	bool achieved; // for the callback
	PIPE_ReadString(ach_name);

	// get the info from steam
	if (!GSteamUserStats->GetAchievement(ach_name, &achieved))
		return;

	// if we already have it, spare the trouble
	if (achieved)
		return;

	Con_Print("STEAM PIPE: (Server) unlocking achievement\n");
	// send steam the achievement
	GSteamUserStats->SetAchievement(ach_name);
	steam_AchievementStatsPending = true;

	// update our internal datastructure
	for (int i = 0; i < MAX_STEAM_REGISTERED_FIELDS; i++)
	{
		steamachievement_t *achieve = &g_Achievements[i];
		if (stricmp(achieve->m_pchAchievementID, ach_name))
			continue;

		achieve->m_bAchieved = true;
		break;
	}

	// now we have to send the callback to the engine to trigger any hud elements or what have you
	PIPE_WriteByte(SV_ACHIEVEMENT_VALUE);
	PIPE_WriteString(ach_name);
	PIPE_WriteByte(achieved);
}


void PacketRead_Achievement_Get(void)
{
	char ach_name[MAX_PIPESTRING];
	bool achieved;
	PIPE_ReadString(ach_name);

	// get the info from steam
	if (!GSteamUserStats->GetAchievement(ach_name, &achieved))
		return;

	// write packet to the engine
	PIPE_WriteByte(SV_ACHIEVEMENT_VALUE);
	PIPE_WriteString(ach_name);
	PIPE_WriteByte(achieved);
}


int Stat_GetType(const char *stat_name)
{
#if ACHIEVEMENT_HARDCODE
	for (int iStat = 0; iStat < STATS_MAX; ++iStat)
	{
		Stat_t *stat = &g_Stats[iStat];

		if (stricmp(stat->m_pchStatID, stat_name))
			continue;

		return stat->m_iType;
	}
#else
	for (int iStat = 0; iStat < MAX_STEAM_REGISTERED_FIELDS; ++iStat)
	{
		steamstat_t *stat = &g_Stats[iStat];
		if (!stat->m_pchStatID[0]) // empty stat name means we're out of registered territory
			break;
		if (stricmp(stat->m_pchStatID, stat_name))
			continue;
		return stat->m_iType;
	}
#endif

	return -1;
}


float Stat_GetValue(const char *stat_name)
{
	float value;
	int32 ivalue;

	if (Stat_GetType(stat_name)) // stat is a float
	{
		if (!GSteamUserStats->GetStat(stat_name, &value))
			return 0;
	}
	else // stat is an int
	{
		if (!GSteamUserStats->GetStat(stat_name, &ivalue))
			return 0;

		value = (float)ivalue;
	}

	return value;
}


int Stat_ChangeValue(const char *stat_name, float val, bool increment)
{
	float value;
	int32 ivalue;

	// fgsfds... why have an overload if it rejects the unmatched type instead of casting to it?
	if (Stat_GetType(stat_name)) // stat is a float
	{
		if (increment)
		{
			// get the info from steam
			if (!GSteamUserStats->GetStat(stat_name, &value))
				return 0;
			value += val;
		}
		else
		{
			value = val;
		}

		if (!GSteamUserStats->SetStat(stat_name, value))
			return 0;
	}
	else // stat is an int
	{
		if (increment)
		{
			// get the info from steam
			if (!GSteamUserStats->GetStat(stat_name, &ivalue))
				return 0;
			ivalue += val;
		}
		else
		{
			ivalue = val;
		}

		if (!GSteamUserStats->SetStat(stat_name, ivalue))
			return 0;

		value = (float)ivalue;
	}

	for (int i = 0; i < MAX_STEAM_REGISTERED_FIELDS; i++)
	{
		steamstat_t *stat = &g_Stats[i];
		if (stricmp(stat->m_pchStatID, stat_name))
			continue;
		stat->m_fValue = value;
		break;
	}

	steam_AchievementStatsPending = true;

	PIPE_WriteByte(SV_STAT_VALUE);
	PIPE_WriteString(stat_name);
	PIPE_WriteFloat(value);
	return value;
}


// Steam language support
typedef struct {
	const char isocode[3];
	const char steamcode[32];
} langdef_t;

langdef_t languages[] = {
	{"ar", "arabic"},
	{"bg", "bulgarian"},
	{"zh", "schinese"},
	{"cs", "czech"},
	{"da", "danish"},
	{"nl", "dutch"},
	{"en", "english"},
	{"da", "danish"},
	{"fi", "finnish"},
	{"fr", "french"},
	{"de", "german"},
	{"el", "greek"},
	{"hu", "hungarian"},
	{"id", "indonesian"},
	{"it", "italian"},
	{"ja", "japanese"},
	{"ko", "koreana"},
	{"no", "norwegian"},
	{"pl", "polish"},
	{"pt", "portuguese"},
	{"es", "spanish"},
	{"sv", "swedish"},
	{"th", "thai"},
	{"tr", "turkish"},
	{"uk", "ukrainian"},
	{"vn", "vietnamese"}
};

void Language_SendToGame(const char *langid)
{
	const char *isocode;
	for (int i = 0; i < (sizeof(languages) / sizeof(langdef_t)); i++)
	{
		langdef_t *lang = &languages[i];
		if (stricmp(langid, lang->steamcode))
			continue;

		isocode = lang->isocode;
		break;
	}

	PIPE_WriteByte(SV_SETLANGUAGE);
	PIPE_WriteString(isocode);
	return;
}
//

void PacketRead_Stat_Get(void)
{
	char stat_name[MAX_PIPESTRING];
	float value;
	PIPE_ReadString(stat_name);

	value = Stat_GetValue(stat_name);

	// write packet to the engine
	PIPE_WriteByte(SV_STAT_VALUE);
	PIPE_WriteString(stat_name);
	PIPE_WriteFloat(value);
}


void PacketRead_Stat_Set(void)
{
	char stat_name[MAX_PIPESTRING];
	float value;
	PIPE_ReadString(stat_name);
	value = PIPE_ReadFloat();

	Stat_ChangeValue(stat_name, value, false);
}


void PacketRead_Stat_Increment(void)
{
	char stat_name[MAX_PIPESTRING];
	float inc, value;
	PIPE_ReadString(stat_name);
	inc = PIPE_ReadFloat();

	value = Stat_ChangeValue(stat_name, inc, true);
}


void PacketRead_Stat_WipeAll(void)
{
	GSteamUserStats->ResetAllStats(true);
	GSteamUserStats->RequestCurrentStats(); // fetch update afterwards (oh boy I hope the previous call is synchronous)
}


void PacketRead_Register_Achievement(void)
{
	char ach_name[MAX_PIPESTRING];
	PIPE_ReadString(ach_name);

	for (int i = 0; i < MAX_STEAM_REGISTERED_FIELDS; i++)
	{
		steamachievement_t *achieve = &g_Achievements[i];
		if (!stricmp(ach_name, achieve->m_pchAchievementID)) // no duplicates
			return;
		if (achieve->m_pchAchievementID[0]) // we don't want to overwrite any other stats
			continue;

		strcpy_lazy(achieve->m_pchAchievementID, ach_name);

		char buf[MAX_PIPESTRING];
		_snprintf(buf, MAX_PIPESTRING, "STEAM PIPE: (server) registered achievement #%i %s\n", i, achieve->m_pchAchievementID);
		Con_Print(buf);
		return;
	}
}


void PacketRead_Register_Stat(void)
{
	char stat_name[MAX_PIPESTRING];
	int stat_type;
	PIPE_ReadString(stat_name);
	stat_type = PIPE_ReadByte();

	for (int i = 0; i < MAX_STEAM_REGISTERED_FIELDS; i++)
	{
		steamstat_t *stat = &g_Stats[i];
		if (!stricmp(stat_name, stat->m_pchStatID)) // no duplicates
			return;
		if (stat->m_pchStatID[0]) // we don't want to overwrite any other stats
			continue;

		strcpy_lazy(stat->m_pchStatID, stat_name);
		stat->m_iType = stat_type;

		char buf[MAX_PIPESTRING];
		snprintf(buf, MAX_PIPESTRING, "STEAM PIPE: (server) registered stat #%i %s\n", i, stat->m_pchStatID);
		Con_Print(buf);
		return;
	}
}


void PacketRead_Controller_GetType(void)
{
	controllertype_t type;
	int index;
	index = PIPE_ReadByte();

	InputHandle_t inputHandle = SteamInput()->GetControllerForGamepadIndex(index);
	if (inputHandle == 0)
	{
		type = CONTROLLER_XBOX;
	}
	else
	{
		ESteamInputType inputType = SteamInput()->GetInputTypeForHandle(inputHandle);
		switch(inputType)
		{
			case k_ESteamInputType_SteamDeckController:
			case k_ESteamInputType_SteamController:
				type = CONTROLLER_STEAM; break;
			case k_ESteamInputType_PS3Controller:
			case k_ESteamInputType_PS4Controller:
			case k_ESteamInputType_PS5Controller:
				type = CONTROLLER_PLAYSTATION; break;
			case k_ESteamInputType_SwitchJoyConPair:
			case k_ESteamInputType_SwitchProController:
				type = CONTROLLER_NINTENDO; break;
			case k_ESteamInputType_GenericGamepad:
				type = CONTROLLER_GENERIC; break;
			case k_ESteamInputType_XBox360Controller:
			case k_ESteamInputType_XBoxOneController:
			default:
				type = CONTROLLER_XBOX; break;
		}
	}

	PIPE_WriteByte(SV_CONTROLLER_TYPE);
	PIPE_WriteByte(index);
	PIPE_WriteByte((int)type);
}

void PacketRead_OnScreenKeyboard(void)
{
	int type, xpos, ypos, xsize, ysize;
	EFloatingGamepadTextInputMode type_inputmode;

	type = PIPE_ReadByte();
	xpos = PIPE_ReadLong();
	ypos = PIPE_ReadLong();
	xsize = PIPE_ReadLong();
	ysize = PIPE_ReadLong();

	char s[MAX_PIPESTRING];
	bool ret;
	#if 1
	type_inputmode = (EFloatingGamepadTextInputMode)type;
	ret = SteamUtils()->ShowFloatingGamepadTextInput(type_inputmode, xpos, ypos, xsize, ysize);
	sprintf(s, "STEAM PIPE: (server) ShowFloatingGamepadTextInput returned %i\n", (int)ret);
	#else
	ret = SteamUtils()->ShowGamepadTextInput(k_EGamepadTextInputModeNormal, k_EGamepadTextInputLineModeSingleLine, "Profile Name: ", 16, "");
	sprintf(s, "STEAM PIPE: (server) ShowGamepadTextInput returned %i\n", (int)ret);
	#endif
	Con_Print(s);
}


void DRM_Init(void)
{
	func_readarray[CL_HANDSHAKE] = PacketRead_Handshake;
	func_readarray[CL_ACHIEVEMENT_SET] = PacketRead_Achievement_Set;
	func_readarray[CL_ACHIEVEMENT_GET] = PacketRead_Achievement_Get;
	func_readarray[CL_STAT_INCREMENT] = PacketRead_Stat_Increment;
	func_readarray[CL_STAT_SET] = PacketRead_Stat_Set;
	func_readarray[CL_STAT_GET] = PacketRead_Stat_Get;
	func_readarray[CL_STAT_WIPE_ALL] = PacketRead_Stat_WipeAll;
	func_readarray[CL_REGISTER_ACHIEVEMENT] = PacketRead_Register_Achievement;
	func_readarray[CL_REGISTER_STAT] = PacketRead_Register_Stat;
	func_readarray[CL_CONTROLLER_GETTYPE] = PacketRead_Controller_GetType;
	func_readarray[CL_ONSCREENKEYBOARD] = PacketRead_OnScreenKeyboard;
}

#endif


