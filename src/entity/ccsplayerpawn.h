#ifndef _INCLUDE_MENU_ENTITY_CCSPLAYERPAWN_H_
#define _INCLUDE_MENU_ENTITY_CCSPLAYERPAWN_H_

#include "schema.h"
#include "cbaseentity.h"
#include "in_buttons.h"

#include <cstdint>

// CBasePlayerPawn : CBaseEntity
class CBasePlayerPawn : public CBaseEntity
{
public:
	DECLARE_SCHEMA_CLASS(CBasePlayerPawn)

	// Currently-held button bitmask (m_pButtonStates[0]), or 0 if unavailable.
	uint64_t GetHeldButtons()
	{
		static int16_t offMovement =
			schema::GetOffset("CBasePlayerPawn", FNV1a("CBasePlayerPawn"), "m_pMovementServices", FNV1a("m_pMovementServices"));
		static int16_t offButtons =
			schema::GetOffset("CPlayer_MovementServices", FNV1a("CPlayer_MovementServices"), "m_nButtons", FNV1a("m_nButtons"));
		static int16_t offStates = schema::GetOffset("CInButtonState", FNV1a("CInButtonState"), "m_pButtonStates", FNV1a("m_pButtonStates"));

		if (offMovement <= 0 || offButtons <= 0 || offStates <= 0)
		{
			return 0;
		}

		void *services = *reinterpret_cast<void **>(reinterpret_cast<uintptr_t>(this) + offMovement);
		if (!services)
		{
			return 0;
		}

		uintptr_t buttonState = reinterpret_cast<uintptr_t>(services) + offButtons;
		return *reinterpret_cast<uint64_t *>(buttonState + offStates);
	}

	// Текущий режим наблюдения (m_pObserverServices->m_iObserverMode).
	// OBS_MODE_NONE (0) если игрок не наблюдает или схема недоступна.
	// Тот же безопасный паттерн с проверкой оффсетов, что и в GetHeldButtons.
	uint32_t GetObserverMode()
	{
		static int16_t offServices =
			schema::GetOffset("CBasePlayerPawn", FNV1a("CBasePlayerPawn"), "m_pObserverServices", FNV1a("m_pObserverServices"));
		static int16_t offMode =
			schema::GetOffset("CPlayer_ObserverServices", FNV1a("CPlayer_ObserverServices"), "m_iObserverMode", FNV1a("m_iObserverMode"));

		if (offServices <= 0 || offMode <= 0)
		{
			return 0; // OBS_MODE_NONE
		}

		void *services = *reinterpret_cast<void **>(reinterpret_cast<uintptr_t>(this) + offServices);
		if (!services)
		{
			return 0;
		}

		return *reinterpret_cast<uint32_t *>(reinterpret_cast<uintptr_t>(services) + offMode);
	}
};

// CCSPlayerPawn : CBasePlayerPawn
class CCSPlayerPawn : public CBasePlayerPawn
{
public:
	DECLARE_SCHEMA_CLASS(CCSPlayerPawn)
};

#endif // _INCLUDE_MENU_ENTITY_CCSPLAYERPAWN_H_
