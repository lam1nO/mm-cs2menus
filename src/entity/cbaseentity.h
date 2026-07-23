#ifndef _INCLUDE_MENU_ENTITY_CBASEENTITY_H_
#define _INCLUDE_MENU_ENTITY_CBASEENTITY_H_

#include "schema.h"
#include <entity2/entityinstance.h>

// CBaseEntity : CEntityInstance
class CBaseEntity : public CEntityInstance
{
public:
	DECLARE_SCHEMA_CLASS(CBaseEntity)

	SCHEMA_FIELD(int32_t, m_iTeamNum)

	// Команда: 0 = none, 1 = наблюдатель (спектатор), 2 = T, 3 = CT (CS_TEAM_*).
	int GetTeam() { return m_iTeamNum(); }
};

#endif // _INCLUDE_MENU_ENTITY_CBASEENTITY_H_
