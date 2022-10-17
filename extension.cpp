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

CUtlRBTreeFix g_CUtlRBTreeFix;
SMEXT_LINK(&g_CUtlRBTreeFix);

int g_offset_m_PackedEntitiesPool = 0;
CDetour *g_LevelChangedDetour = NULL;


class CFrameSnapshotManager
{
public:
	CClassMemoryPoolExt<PackedEntity>& m_PackedEntitiesPool()
	{
		return *reinterpret_cast<CClassMemoryPoolExt<PackedEntity>*>(reinterpret_cast<byte*>(this) + g_offset_m_PackedEntitiesPool);
	}
};


DETOUR_DECL_MEMBER0(CFrameSnapshotManager_LevelChanged, void)
{
	//smutils->LogMessage(myself, "------- CFrameSnapshotManager_LevelChanged");

	CFrameSnapshotManager* _this = reinterpret_cast<CFrameSnapshotManager*>(this);

	// bug##: Underlying method CClassMemoryPool::Clear creates CUtlRBTree with insufficient iterator size (unsigned short)
	// memory object of which fails to iterate over more than 65535 of packed entities
	_this->m_PackedEntitiesPool().Clear();

	// CFrameSnapshotManager::m_PackedEntitiesPool shouldn't have elements to free from now on
	DETOUR_MEMBER_CALL(CFrameSnapshotManager_LevelChanged)();
}

bool CUtlRBTreeFix::SDK_OnLoad( char *error, size_t maxlength, bool late )
{
	IGameConfig *gc = NULL;
	if (!gameconfs->LoadGameConfigFile("cutlrbtreefix", &gc, error, maxlength))
	{
		ke::SafeStrcpy(error, maxlength, "Unable to load cutlrbtreefix.txt file");
		return false;
	}

	if (!gc->GetOffset("CFrameSnapshotManager::m_PackedEntitiesPool", &g_offset_m_PackedEntitiesPool))
	{
		ke::SafeStrcpy(error, maxlength, "Failed to get CFrameSnapshotManager::m_PackedEntitiesPool offset");
		return false;
	}

	void *addr = 0;
	if (!gc->GetMemSig("CFrameSnapshotManager::LevelChanged", &addr) || !addr)
	{
		ke::SafeStrcpy(error, maxlength, "Failed to get CFrameSnapshotManager::LevelChanged function address");
		return false;
	}

	//smutils->LogMessage(myself, "------- addr = %x, offset = %i", addr, g_offset_m_PackedEntitiesPool);
	CDetourManager::Init(smutils->GetScriptingEngine(), gc);

	g_LevelChangedDetour = DETOUR_CREATE_MEMBER(CFrameSnapshotManager_LevelChanged, addr);
	if (!g_LevelChangedDetour)
	{
		ke::SafeStrcpy(error, maxlength, "Failed to detour CFrameSnapshotManager::LevelChanged");
		return false;
	}

	g_LevelChangedDetour->EnableDetour();
	gameconfs->CloseGameConfigFile(gc);
	return true;
}

void CUtlRBTreeFix::SDK_OnUnload()
{
	if (g_LevelChangedDetour != NULL) {
		g_LevelChangedDetour->Destroy();
		g_LevelChangedDetour = NULL;
	}
}


