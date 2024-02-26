
#ifdef EPIC

#include "wrath_common.h"
#include "wrath_launcher.h"

#include "epic/eos_achievements.h"
#include "epic/eos_stats.h"

#include <iostream>
#include <string.h>
#include <stdio.h>

using namespace std;
EOS_EpicAccountId epicID;
EOS_ProductUserId productUserID;

steamachievement_t g_Achievements[MAX_STEAM_REGISTERED_FIELDS];
steamstat_t g_Stats[MAX_STEAM_REGISTERED_FIELDS];

void EOS_CALL E_StatsIngest(const EOS_Stats_IngestStatCompleteCallbackInfo *data)
{

}

int Stat_GetType(const char *stat_name)
{
	for (int iStat = 0; iStat < MAX_STEAM_REGISTERED_FIELDS; ++iStat)
	{
		steamstat_t *stat = &g_Stats[iStat];
		if (!stat->m_pchStatID[0]) // empty stat name means we're out of registered territory
			break;
		if (stricmp(stat->m_pchStatID, stat_name))
			continue;
		return stat->m_iType;
	}

	return -1;
}

float Stat_GetValue(const char *stat_name)
{
	float value;
	uint32_t ivalue;

	EOS_Stats_Stat *sOutput = NULL;
	EOS_Stats_CopyStatByNameOptions sOptions = {};
	sOptions.ApiVersion = EOS_STATS_COPYSTATBYNAME_API_LATEST;
	sOptions.TargetUserId = productUserID;
	sOptions.Name = stat_name;

	EOS_Stats_CopyStatByName(GEpicStats, &sOptions, &sOutput);
	
	value = 0;
	if (sOutput)
		value = (float)sOutput->Value;

	if (Stat_GetType(stat_name)) // stat is a float
	{
		value /= 1000.0f;
	}

	return value;
}


int Stat_ChangeValue(const char *stat_name, float val, bool increment)
{
	float value;
	float delta_val;
	float epic_val;
	
	if (Stat_GetType(stat_name)) // stat is a float
	{
		epic_val = value = Stat_GetValue(stat_name); // get the info from epic
		if (increment)
		{
			value += val;
			epic_val = val;
		}
		else
		{
			value = val;
			epic_val = (val - epic_val);
		}

		epic_val *= 1000;
	}
	else // stat is an int
	{
		if (increment)
		{
			value += val;
			epic_val = val;
		}
		else
		{
			value = val;
			epic_val = (val - epic_val);
		}
	}
	
	EOS_Stats_IngestData datalist[1];
	datalist[0].ApiVersion = EOS_STATS_INGESTDATA_API_LATEST;
	datalist[0].IngestAmount = epic_val;
	datalist[0].StatName = stat_name;

	EOS_Stats_IngestStatOptions isOptions = {};
	isOptions.ApiVersion = EOS_STATS_INGESTSTAT_API_LATEST;
	isOptions.Stats = datalist;
	isOptions.StatsCount = 1;
	isOptions.LocalUserId = productUserID;
	isOptions.TargetUserId = productUserID;

	EOS_Stats_IngestStat(GEpicStats, &isOptions, NULL, E_StatsIngest);

	for (int i = 0; i < MAX_STEAM_REGISTERED_FIELDS; i++)
	{
		steamstat_t *stat = &g_Stats[i];
		if (stricmp(stat->m_pchStatID, stat_name))
			continue;
		stat->m_fValue = value;
		break;
	}

	PIPE_WriteByte(SV_STAT_VALUE);
	PIPE_WriteString(stat_name);
	PIPE_WriteFloat(value);
	return value;
}

void EOS_CALL E_OnAchievementUnlock(const EOS_Achievements_OnUnlockAchievementsCompleteCallbackInfo *data)
{
	if (data->ResultCode != EOS_EResult::EOS_Success)
	{
		if (data->ClientData)
			free(data->ClientData);
		return;
	}

	PIPE_WriteByte(SV_ACHIEVEMENT_VALUE);
	PIPE_WriteString((const char *)data->ClientData);
	PIPE_WriteByte(1);
}

void PacketRead_Handshake(void)
{
	if (epicID == NULL)
	{
		PIPE_WriteByte(SV_STEAMID);
		PIPE_WriteString("EOS_USER"); // epic doesn't give us IDs unless we jump through hoops... cool
	}
}

void PacketRead_Achievement_Set(void)
{
	char ach_name[MAX_PIPESTRING];
	const char *ach_ptr[1];
	ach_ptr[0] = ach_name;
	bool achieved; // for the callback
	PIPE_ReadString(ach_name);

	cout << __func__ << ": " << ach_name << "\n";

	void *clientdata = malloc(MAX_PIPESTRING);
	strncpy((char *)clientdata, ach_name, MAX_PIPESTRING);

	EOS_Achievements_UnlockAchievementsOptions uaOptions = {};
	uaOptions.ApiVersion = EOS_ACHIEVEMENTS_UNLOCKACHIEVEMENTS_API_LATEST;
	uaOptions.AchievementIds = ach_ptr;
	uaOptions.AchievementsCount = 1;
	uaOptions.UserId = productUserID;
	EOS_Achievements_UnlockAchievements(GEpicAchievements, &uaOptions, clientdata, E_OnAchievementUnlock);
}

void PacketRead_Achievement_Get(void)
{
	char ach_name[MAX_PIPESTRING];
	PIPE_ReadString(ach_name);

	EOS_Achievements_PlayerAchievement *aOutput;
	EOS_Achievements_CopyPlayerAchievementByAchievementIdOptions aOptions = {};
	aOptions.ApiVersion = EOS_ACHIEVEMENTS_COPYPLAYERACHIEVEMENTBYACHIEVEMENTID_API_LATEST;
	aOptions.AchievementId = ach_name;
	aOptions.LocalUserId = productUserID;
	aOptions.TargetUserId = productUserID;
	EOS_EResult result = EOS_Achievements_CopyPlayerAchievementByAchievementId(GEpicAchievements, &aOptions, &aOutput);

	cout << __func__ << ": " << to_string((int)result).c_str() << "\n";

	// write packet to the engine
	PIPE_WriteByte(SV_ACHIEVEMENT_VALUE);
	PIPE_WriteString(ach_name);
	if (aOutput)
	{
		PIPE_WriteByte(aOutput->Progress >= 1);
	}
	else
		PIPE_WriteByte(0);
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


void PacketRead_Stat_WipeAll(void)
{

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

	PIPE_WriteByte(SV_CONTROLLER_TYPE);
	PIPE_WriteByte(index);
	PIPE_WriteByte(CONTROLLER_XBOX);
}

void PacketRead_OnScreenKeyboard(void)
{
	int type, xpos, ypos, xsize, ysize;
	type = PIPE_ReadByte();
	xpos = PIPE_ReadLong();
	ypos = PIPE_ReadLong();
	xsize = PIPE_ReadLong();
	ysize = PIPE_ReadLong();
}





void EOS_CALL Epic_LoggingFunc(const EOS_LogMessage *Message)
{
	cout << "EPIC: " << Message->Message << "\n";
}

void EOS_CALL E_ConnectCreateUser(const EOS_Connect_CreateUserCallbackInfo *data)
{
	cout << __func__ << ": " << to_string((int)data->ResultCode) << "\n";

	if (data->ResultCode != EOS_EResult::EOS_Success)
		return;

	productUserID = data->LocalUserId;
}

void EOS_CALL E_ConnectLogon(const EOS_Connect_LoginCallbackInfo *data)
{
	cout << __func__ << ": " << to_string((int)data->ResultCode) << "\n";

	if (data->ResultCode != EOS_EResult::EOS_Success)
	{
		if (data->ResultCode == EOS_EResult::EOS_InvalidUser && data->ContinuanceToken)
		{
			EOS_Connect_CreateUserOptions cuOptions;
			cuOptions.ApiVersion = EOS_CONNECT_CREATEUSER_API_LATEST;
			cuOptions.ContinuanceToken = data->ContinuanceToken;

			EOS_Connect_CreateUser(GEpicConnect, &cuOptions, NULL, E_ConnectCreateUser);
		}
		
		return;
	}

	productUserID = data->LocalUserId;
}

typedef struct {
	EOS_ELoginCredentialType type;
} logindata_t;
void EOS_CALL E_LoginCallback(const EOS_Auth_LoginCallbackInfo *data)
{
	cout << __func__ << ": " << to_string((int)data->ResultCode) << "\n";
	if (data->ResultCode != EOS_EResult::EOS_Success)
	{
		// if old login type was Persistent Auth, fall back to Account Portal
		logindata_t *old_logindata = (logindata_t*)data->ClientData;
		if (old_logindata->type == EOS_ELoginCredentialType::EOS_LCT_PersistentAuth)
		{
			EOS_Auth_Credentials authCreds = {};
			authCreds.ApiVersion = EOS_AUTH_CREDENTIALS_API_LATEST;
			authCreds.Type = EOS_ELoginCredentialType::EOS_LCT_AccountPortal;
			EOS_Auth_LoginOptions aOptions = {};
			aOptions.ApiVersion = EOS_AUTH_LOGIN_API_LATEST;
			aOptions.Credentials = &authCreds;
			logindata_t *cData = (logindata_t*)malloc(sizeof(logindata_t));
			cData->type = authCreds.Type;
			EOS_Auth_Login(GEpicAuth, &aOptions, cData, E_LoginCallback);
		}
		free(old_logindata);
		return;
	}

	char buf[1024] = {};
	int sz = 1024;
	epicID = data->SelectedAccountId;
	EOS_EpicAccountId_ToString(epicID, buf, &sz);

	#if 1
	#if 0
	EOS_Auth_Token *tokenout;
	EOS_Auth_CopyUserAuthTokenOptions CopyTokenOptions = { 0 };
	CopyTokenOptions.ApiVersion = EOS_AUTH_COPYUSERAUTHTOKEN_API_LATEST;
	EOS_Auth_CopyUserAuthToken(GEpicAuth, &CopyTokenOptions, epicID, &tokenout);

	EOS_Connect_Credentials cCreds = {};
	cCreds.ApiVersion = EOS_CONNECT_CREDENTIALS_API_LATEST;
	cCreds.Type = EOS_EExternalCredentialType::EOS_ECT_EPIC;
	cCreds.Token = tokenout->AccessToken;
	#else
	EOS_Auth_IdToken *cIDToken;
	EOS_Auth_CopyIdTokenOptions idtOptions;
	idtOptions.AccountId = epicID;
	idtOptions.ApiVersion = EOS_AUTH_COPYIDTOKEN_API_LATEST;
	EOS_EResult tokenresult = EOS_Auth_CopyIdToken(GEpicAuth, &idtOptions, &cIDToken);

	EOS_Connect_Credentials cCreds = {};
	cCreds.ApiVersion = EOS_CONNECT_CREDENTIALS_API_LATEST;
	cCreds.Type = EOS_EExternalCredentialType::EOS_ECT_EPIC_ID_TOKEN;
	cCreds.Token = cIDToken->JsonWebToken;
	#endif

	EOS_Connect_LoginOptions lOptions = {};
	lOptions.ApiVersion = EOS_CONNECT_LOGIN_API_LATEST;
	lOptions.Credentials = &cCreds;
	EOS_Connect_Login(GEpicConnect, &lOptions, NULL, E_ConnectLogon);
	#endif

	PIPE_WriteByte(SV_STEAMID);
	PIPE_WriteString(buf);
}

void EOS_CALL E_QueryPlayerAchievements(const EOS_Achievements_OnQueryPlayerAchievementsCompleteCallbackInfo *data)
{
	
}

void EOS_CALL E_QueryStats(const EOS_Stats_OnQueryStatsCompleteCallbackInfo *data)
{
	
}

bool epic_auth_checked = false;
void Epic_Auth(void)
{
	EOS_Auth_Credentials authCreds = {};
	authCreds.ApiVersion = EOS_AUTH_CREDENTIALS_API_LATEST;
	if (epic_auth_exchangecode)
	{
		authCreds.Type = EOS_ELoginCredentialType::EOS_LCT_ExchangeCode;
		authCreds.Token = epic_auth_password;
	}
	else if (epic_devauth_ip[0] && epic_devauth_name[0])
	{
		authCreds.Type = EOS_ELoginCredentialType::EOS_LCT_Developer;
		authCreds.Id = epic_devauth_ip;
		authCreds.Token = epic_devauth_name;
		cout << __func__ << ": " << authCreds.Id << " (" << authCreds.Token << ")\n";
	}
	else
	{
		authCreds.Type = EOS_ELoginCredentialType::EOS_LCT_PersistentAuth;
	}
	EOS_Auth_LoginOptions aOptions = {};
	aOptions.ApiVersion = EOS_AUTH_LOGIN_API_LATEST;
	aOptions.Credentials = &authCreds;
	logindata_t *cData = (logindata_t*)malloc(sizeof(logindata_t));
	cData->type = authCreds.Type;

	EOS_Auth_Login(GEpicAuth, &aOptions, cData, E_LoginCallback);
}

void Epic_QueryAchievements(void)
{
	static int epic_achievementCooldown = 0;
	epic_achievementCooldown--;
	if (epic_achievementCooldown <= 0 && productUserID != NULL)
	{
		EOS_Achievements_QueryPlayerAchievementsOptions pOptions = {};
		pOptions.ApiVersion = EOS_ACHIEVEMENTS_QUERYPLAYERACHIEVEMENTS_API_LATEST;
		pOptions.LocalUserId = productUserID;
		pOptions.TargetUserId = productUserID;

		EOS_Achievements_QueryPlayerAchievements(GEpicAchievements, &pOptions, NULL, E_QueryPlayerAchievements);
		
		EOS_Stats_QueryStatsOptions sOptions = {};
		sOptions.ApiVersion = EOS_STATS_QUERYSTATS_API_LATEST;
		sOptions.LocalUserId = productUserID;
		sOptions.TargetUserId = productUserID;
		sOptions.StartTime = EOS_STATS_TIME_UNDEFINED; sOptions.EndTime = EOS_STATS_TIME_UNDEFINED;
		EOS_Stats_QueryStats(GEpicStats, &sOptions, NULL, E_QueryStats);

		cout << __func__ << ": " << productUserID << "\n";
		#ifdef _WIN32 // lazy fix for unix running at 1hz instead of 10hz because of the sleep command
		epic_achievementCooldown = 80;
		#else
		epic_achievementCooldown = 8;
		#endif
	}
}

void EOS_CALL E_AchievementNotificationUnlocked(const EOS_Achievements_OnAchievementsUnlockedCallbackV2Info *data)
{
	if (data->UserId != productUserID)
		return;

	PIPE_WriteByte(SV_ACHIEVEMENT_VALUE);
	PIPE_WriteString(data->AchievementId);
	PIPE_WriteByte(1);
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

	EOS_Achievements_AddNotifyAchievementsUnlockedV2Options aOptions = {};
	aOptions.ApiVersion = EOS_ACHIEVEMENTS_ADDNOTIFYACHIEVEMENTSUNLOCKEDV2_API_LATEST;
	EOS_Achievements_AddNotifyAchievementsUnlockedV2(GEpicAchievements, &aOptions, NULL, E_AchievementNotificationUnlocked);
}








#endif
