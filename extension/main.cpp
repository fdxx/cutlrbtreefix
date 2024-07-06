#include "mempool.h"
#include "packed_entity.h"
#include "stringpool.h"
#include <stdio.h>
#include <string.h>
#include "smsdk_ext.h"
#include "safetyhook.hpp"


class CUtlRBTreeFix : public SDKExtension
{
public:
	virtual bool SDK_OnLoad(char *error, size_t maxlen, bool late);
	virtual void SDK_OnUnload();
};

CUtlRBTreeFix g_CUtlRBTreeFix;
SMEXT_LINK(&g_CUtlRBTreeFix);

static int g_iPackedEntitiesPool;
static char g_sLogPath[PLATFORM_MAX_PATH];

static SafetyHookInline g_hookLevelChanged{};
static SafetyHookInline g_hookStringPoolAllocate{};

#define STRINGPOOL_LIMIT 65000

template <typename R, typename T, typename... Args>
inline void *GetMemberFuncAddr(R (T::*memberFunc)(Args...))
{
	return *(void **)&memberFunc;
}

static void LogToFile(const char *file, const char *fmt, ...)
{
	FILE *fp = fopen(file, "at");
	if (!fp) return;

	static char buffer[1024];
	va_list args;
	va_start(args, fmt);
	vsnprintf(buffer, sizeof(buffer), fmt, args);
	va_end(args);

	char date[32];
	time_t now = time(nullptr);
	tm *curtime = localtime(&now);
	strftime(date, sizeof(date), "%m/%d/%Y - %H:%M:%S", curtime);

	fprintf(fp, "L %s: %s\n", date, buffer);
	printf("L %s: %s\n", date, buffer);
	fclose(fp);
}

class CFrameSnapshotManagerExt
{
public:
	void LevelChanged()
	{
		// bug##: Underlying method CClassMemoryPool::Clear creates CUtlRBTree with insufficient iterator size (unsigned short)
		// memory object of which fails to iterate over more than 65535 of packed entities
		// CUtlRBTree<void *> to CUtlRBTree<void *, int>
		m_PackedEntitiesPool()->Clear();
		g_hookLevelChanged.thiscall<void>(this);
	}

	CClassMemoryPoolExt<PackedEntity> *m_PackedEntitiesPool()
	{
		return (CClassMemoryPoolExt<PackedEntity>*)((char*)this + g_iPackedEntitiesPool);
	}
};


class CStringPoolExt : public CStringPool
{
public:
	const char *Allocate( const char *pszValue )
	{
		// The string pool increases with the start of each round and is not cleared until MapEnd.
		// So we reset the current map when the game is about to crash in order to auto clear the string pool.
		if (Count() > STRINGPOOL_LIMIT && !Find(pszValue))
		{
			const char *map = gamehelpers->GetCurrentMap();
			LogToFile(g_sLogPath, "CurrentStringPoolCount %i exceeds %i, auto reset the current map %s, AllocString: %s", Count(), STRINGPOOL_LIMIT, map, pszValue);

			char buffer[256];
			snprintf(buffer, sizeof(buffer), "changelevel \"%s\"\n", map);
			gamehelpers->ServerCommand(buffer);
			return nullptr;
		}
		return g_hookStringPoolAllocate.thiscall<const char*>(this, pszValue);
	}
};

bool CUtlRBTreeFix::SDK_OnLoad( char *error, size_t maxlength, bool late )
{
	IGameConfig *gamedata = nullptr;
	const char *buffer = nullptr;
	void *addr = nullptr;

	buffer = "cutlrbtreefix";
	if (!gameconfs->LoadGameConfigFile(buffer, &gamedata, error, maxlength))
	{
		snprintf(error, maxlength, "Failed to LoadGameConfigFile: %s", buffer);
		return false;
	}

	buffer = "CFrameSnapshotManager::m_PackedEntitiesPool";
	if (!gamedata->GetOffset(buffer, &g_iPackedEntitiesPool))
	{
		snprintf(error, maxlength, "Failed to GetOffset: %s", buffer);
		return false;
	}

	buffer = "CFrameSnapshotManager::LevelChanged";
	if (!gamedata->GetMemSig(buffer, &addr))
	{
		snprintf(error, maxlength, "Failed to GetMemSig: %s", buffer);
		return false;
	}

	g_hookLevelChanged = safetyhook::create_inline(addr, GetMemberFuncAddr(&CFrameSnapshotManagerExt::LevelChanged));
	if (!g_hookLevelChanged.enabled())
	{
		snprintf(error, maxlength, "Failed to create_inline: %s", buffer);
		return false;
	}

	buffer = "CStringPool::Allocate";
	if (!gamedata->GetMemSig(buffer, &addr))
	{
		snprintf(error, maxlength, "Failed to GetMemSig: %s", buffer);
		return false;
	}

	g_hookStringPoolAllocate = safetyhook::create_inline(addr, GetMemberFuncAddr(&CStringPoolExt::Allocate));
	if (!g_hookStringPoolAllocate.enabled())
	{
		snprintf(error, maxlength, "Failed to create_inline: %s", buffer);
		return false;
	}

	gameconfs->CloseGameConfigFile(gamedata);
	smutils->BuildPath(Path_SM, g_sLogPath, sizeof(g_sLogPath), "logs/cutlrbtreefix.log");
	return true;
}

void CUtlRBTreeFix::SDK_OnUnload()
{
	g_hookLevelChanged = {};
	g_hookStringPoolAllocate = {};
}


