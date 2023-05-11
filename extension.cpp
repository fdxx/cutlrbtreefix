/**
 * vim: set ts=4 :
 * =============================================================================
 * SourceMod Sample Extension
 * Copyright (C) 2004-2008 AlliedModders LLC.  All rights reserved.
 * =============================================================================
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License, version 3.0, as published by the
 * Free Software Foundation.
 * 
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * As a special exception, AlliedModders LLC gives you permission to link the
 * code of this program (as well as its derivative works) to "Half-Life 2," the
 * "Source Engine," the "SourcePawn JIT," and any Game MODs that run on software
 * by the Valve Corporation.  You must obey the GNU General Public License in
 * all respects for all other code used.  Additionally, AlliedModders LLC grants
 * this exception to all derivative works.  AlliedModders LLC defines further
 * exceptions, found in LICENSE.txt (as of this writing, version JULY-31-2007),
 * or <http://www.sourcemod.net/license.php>.
 *
 * Version: $Id$
 */
#include "h/mempool.h"
#include "h/packed_entity.h"

#include "extension.h"
#include <CDetour/detours.h>

#include <stdarg.h>
#include <stdio.h>

CUtlRBTreeFix g_CUtlRBTreeFix;
SMEXT_LINK(&g_CUtlRBTreeFix);

#define STRINGPOOL_LIMIT 65000

#if defined _WIN32
	typedef const char* (__thiscall *FuncPtr)(void*, const char*);
#else
	typedef const char* (__cdecl *FuncPtr)(void*, const char*);
#endif

static int g_offset_m_PackedEntitiesPool = 0;
static int g_offset_CStringPoolCount = 0;

static CDetour *g_Detour_LevelChanged = NULL;
static CDetour *g_Detour_CStringPoolAllocate = NULL;

static void *g_fnCStringPoolFind = 0;
static char g_sLogPath[PLATFORM_MAX_PATH];

class CFrameSnapshotManager
{
public:
	CClassMemoryPoolExt<PackedEntity>& m_PackedEntitiesPool()
	{
		return *reinterpret_cast<CClassMemoryPoolExt<PackedEntity>*>(reinterpret_cast<byte*>(this) + g_offset_m_PackedEntitiesPool);
	}
};

static void LogToFile(const char *msg, ...)
{
	FILE *fp = fopen(g_sLogPath, "at");
	if (!fp) return;

	char buffer[1024];
	va_list ap;
	va_start(ap, msg);
	ke::SafeVsprintf(buffer, sizeof(buffer), msg, ap);
	va_end(ap);

	char date[32];
	time_t t = smutils->GetAdjustedTime();
	tm *curtime = localtime(&t);
	strftime(date, sizeof(date), "%m/%d/%Y - %H:%M:%S", curtime);

	fprintf(fp, "L %s: %s\n", date, buffer);
	rootconsole->ConsolePrint("L %s: %s", date, buffer);

	fclose(fp);
}

DETOUR_DECL_MEMBER0(CFrameSnapshotManager_LevelChanged, void)
{
	//rootconsole->ConsolePrint("------- CFrameSnapshotManager_LevelChanged");
	CFrameSnapshotManager* _this = reinterpret_cast<CFrameSnapshotManager*>(this);

	// bug##: Underlying method CClassMemoryPool::Clear creates CUtlRBTree with insufficient iterator size (unsigned short)
	// memory object of which fails to iterate over more than 65535 of packed entities
	_this->m_PackedEntitiesPool().Clear();

	// CFrameSnapshotManager::m_PackedEntitiesPool shouldn't have elements to free from now on
	DETOUR_MEMBER_CALL(CFrameSnapshotManager_LevelChanged)();
}

DETOUR_DECL_MEMBER1(CStringPool_Allocate, const char *, const char *, pszValue)
{
	// The string pool increases with the start of each round and is not cleared until MapEnd.
	// So we reset the current map when the game is about to crash in order to auto clear the string pool.

	int count = *reinterpret_cast<uint16_t*>(this + g_offset_CStringPoolCount);
	//rootconsole->ConsolePrint("-- CStringPool_Allocate, count = %i, str = %s", count, pszValue);
	if (count > STRINGPOOL_LIMIT)
	{
		FuncPtr FindString = reinterpret_cast<FuncPtr>(g_fnCStringPoolFind);
		if (!FindString(this, pszValue))
		{
			const char *map = gamehelpers->GetCurrentMap();
			LogToFile("CurrentStringPoolCount %i exceeds %i, auto reset the current map %s, AllocString: %s", count, STRINGPOOL_LIMIT, map, pszValue);

			char buffer[256];
			ke::SafeSprintf(buffer, sizeof(buffer), "changelevel \"%s\"\n", map);
			gamehelpers->ServerCommand(buffer);
			return NULL;
		}
	}
	return DETOUR_MEMBER_CALL(CStringPool_Allocate)(pszValue);
}

bool CUtlRBTreeFix::SDK_OnLoad( char *error, size_t maxlength, bool late )
{
	IGameConfig *gamedata = NULL;
	char buffer[128];

	ke::SafeStrcpy(buffer, sizeof(buffer), "cutlrbtreefix");
	if (!gameconfs->LoadGameConfigFile(buffer, &gamedata, error, maxlength))
	{
		ke::SafeSprintf(error, maxlength, "Unable to load %s.txt file", buffer);
		return false;
	}

	ke::SafeStrcpy(buffer, sizeof(buffer), "CFrameSnapshotManager::m_PackedEntitiesPool");
	if (!gamedata->GetOffset(buffer, &g_offset_m_PackedEntitiesPool))
	{
		ke::SafeSprintf(error, maxlength, "Failed to GetOffset: %s", buffer);
		return false;
	}

	ke::SafeStrcpy(buffer, sizeof(buffer), "CStringPool::Count");
	if (!gamedata->GetOffset(buffer, &g_offset_CStringPoolCount))
	{
		ke::SafeSprintf(error, maxlength, "Failed to GetOffset: %s", buffer);
		return false;
	}

	ke::SafeStrcpy(buffer, sizeof(buffer), "CStringPool::Find");
	if (!gamedata->GetMemSig(buffer, &g_fnCStringPoolFind))
	{
		ke::SafeSprintf(error, maxlength, "Failed to GetMemSig: %s", buffer);
		return false;
	}

	CDetourManager::Init(smutils->GetScriptingEngine(), gamedata);
	
	ke::SafeStrcpy(buffer, sizeof(buffer), "CFrameSnapshotManager::LevelChanged");
	g_Detour_LevelChanged = DETOUR_CREATE_MEMBER(CFrameSnapshotManager_LevelChanged, buffer);
	if (g_Detour_LevelChanged)
		g_Detour_LevelChanged->EnableDetour();
	else
	{
		ke::SafeSprintf(error, maxlength, "Failed to detour %s", buffer);
		return false;
	}

	ke::SafeStrcpy(buffer, sizeof(buffer), "CStringPool::Allocate");
	g_Detour_CStringPoolAllocate = DETOUR_CREATE_MEMBER(CStringPool_Allocate, buffer);
	if (g_Detour_CStringPoolAllocate)
		g_Detour_CStringPoolAllocate->EnableDetour();
	else
	{
		ke::SafeSprintf(error, maxlength, "Failed to detour %s", buffer);
		return false;
	}

	gameconfs->CloseGameConfigFile(gamedata);
	smutils->BuildPath(Path_SM, g_sLogPath, sizeof(g_sLogPath), "logs/cutlrbtreefix.log");
	return true;
}

void CUtlRBTreeFix::SDK_OnUnload()
{
	if (g_Detour_LevelChanged != NULL)
	{
		g_Detour_LevelChanged->Destroy();
		g_Detour_LevelChanged = NULL;
	}

	if (g_Detour_CStringPoolAllocate != NULL)
	{
		g_Detour_CStringPoolAllocate->Destroy();
		g_Detour_CStringPoolAllocate = NULL;
	}
}


