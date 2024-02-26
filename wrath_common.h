#pragma once

#define MAX_PIPEBUFFSIZE	8192
#define MAX_PIPESTRING		256
#define ACHIEVEMENT_HARDCODE 0

extern char steam_UserName[MAX_PIPESTRING];


enum cl_packets
{
	CL_HANDSHAKE,
	CL_ACHIEVEMENT_SET,
	CL_ACHIEVEMENT_GET,
	CL_STAT_INCREMENT,
	CL_STAT_SET,
	CL_STAT_GET,
	CL_STAT_WIPE_ALL,
	CL_REGISTER_ACHIEVEMENT,	// gamecode needs to register stats/achievements
	CL_REGISTER_STAT,			// (to minimize the need to recompile to add new achievements)
	CL_CONTROLLER_GETTYPE,
	CL_ONSCREENKEYBOARD,
	CL_MAX,
};

enum sv_packets
{
	SV_PRINT,
	SV_SETNAME,
	SV_STEAMID,
	SV_ACHIEVEMENT_VALUE,
	SV_STAT_VALUE,
	SV_SETLANGUAGE,
	SV_CONTROLLER_TYPE,
	SV_MAX,
};

#if ACHIEVEMENT_HARDCODE
typedef struct Achievement_s
{
	int m_eAchievementID;
	const char *m_pchAchievementID;
	char m_rgchName[128];
	char m_rgchDescription[256];
#ifdef __cplusplus
	bool m_bAchieved;
#else
	unsigned char m_bAchieved;
#endif
	int m_iIconImage;
} Achievement_t;

#define ACHIEVEMENT_LIST \
	_ACH_ID(SP_1000_KILLS), \
	_ACH_ID(SP_HUB1), \
	_ACH_ID(SP_BLADELUNGE), \
	_ACH_ID(SP_CONFOUNDINGATTAR), \
	_ACH_ID(AL_ROCKETJUMP), \
	_ACH_ID(SP_DROWN), \

#define _ACH_ID( id ) id
enum EAchievements
{
	ACHIEVEMENT_LIST
	ACHIEVEMENTS_MAX
};

#undef _ACH_ID
#define _ACH_ID( id ) { id, #id, "", "", 0, 0 }

typedef struct Stat_s
{
	int m_eStatID;
	const char *m_pchStatID;
	char m_rgchName[128];
	char m_rgchDescription[256];
	float m_fValue;
	int m_iType;
} Stat_t;

#define STAT_LIST \
	_STAT_ID(total_kills, 0), \
	_STAT_ID(monsters_killed, 0), \
	_STAT_ID(blade_lunge_kills, 0), \
	_STAT_ID(confounding_kills, 0), \

#define _STAT_ID( id, type ) id
enum EStats
{
	STAT_LIST
	STATS_MAX
};

#undef _STAT_ID
#define _STAT_ID( id, type ) { id, #id, "", "", 0, type }

extern Achievement_t g_Achievements[];
extern Stat_t g_Stats[];
#else
#define MAX_STEAM_REGISTERED_FIELDS	256

typedef struct steamachievement_s
{
	char m_pchAchievementID[MAX_PIPESTRING];
#ifdef __cplusplus
	bool m_bAchieved;
#else
	int m_bAchieved;
#endif
	int m_iType;
} steamachievement_t;

typedef struct steamstat_s
{
	char m_pchStatID[MAX_PIPESTRING];
	float m_fValue;
	int m_iType;
} steamstat_t;

extern steamachievement_t g_Achievements[];
extern steamstat_t g_Stats[];
#endif

float PIPE_ReadFloat(void);
signed long PIPE_ReadLong(void);
long long PIPE_ReadLongLong(void);
signed short PIPE_ReadShort(void);
unsigned char PIPE_ReadByte(void);
int PIPE_ReadString(char *buff);
void PIPE_ReadCharArray(char *into, unsigned long *size);
int PIPE_WriteFloat(float dat_float);
int PIPE_WriteLong(signed long dat);
int PIPE_WriteLongLong(long long dat);
int PIPE_WriteShort(signed short dat);
int PIPE_WriteByte(unsigned char dat);
int PIPE_WriteString(const char *str);
int PIPE_WriteCharArray(char *dat, unsigned long size);