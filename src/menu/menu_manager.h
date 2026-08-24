#ifndef _INCLUDE_MENU_MANAGER_H_
#define _INCLUDE_MENU_MANAGER_H_

#include "src/common.h"
#include "src/public/ics2menus.h"

#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

// Hard ceiling on chat-menu items per page: keys 1-7 select, 8/9 page, 0 exits.
static constexpr int MENU_MAX_ITEMS_PER_PAGE = 7;
static constexpr int MENU_MAX_HTML_VISIBLE = 7;

// Settings parsed from core.cfg and pushed in via Configure.
struct MenuManagerSettings
{
	// Chat: leading chars allowed before a selection number.
	std::string acceptedPrefixes = "!/";
	// Chat: items per page (clamped 1..MENU_MAX_ITEMS_PER_PAGE).
	int itemsPerPage = MENU_MAX_ITEMS_PER_PAGE;
	// Resolves MenuType::Default at CreateMenu time.
	MenuType defaultType = MenuType::Chat;
	// Exit-button default for new menus.
	bool defaultExitButton = true;
	// HTML: default for the inline selectable "Exit" row on new menus.
	bool defaultExitItem = false;

	// HTML: rows visible at once (clamped 1..MENU_MAX_HTML_VISIBLE).
	int htmlVisibleItems = MENU_MAX_HTML_VISIBLE;
	// HTML: button bitmasks (IN_*) for navigation. Defaults = WASD.
	uint64_t keyUp = 0x8;            // W (IN_FORWARD)
	uint64_t keyDown = 0x10;         // S (IN_BACK)
	uint64_t keySelect = 0x400;      // D (IN_MOVERIGHT)
	uint64_t keyBack = 0x2000;       // R (IN_RELOAD) — вверх к родителю (решение 23.07)
	uint64_t keyExit = 0x800000000;  // F (IN_LOOKATWEAPON) — закрыть меню (живой игрок)
	// Выход для СПЕКТАТОРА: SHIFT (IN_SPEED). В спеках F занят осмотром оружия
	// наблюдаемого игрока, поэтому спектатору выход — Shift (подставляется по слоту).
	uint64_t keyExitSpec = 0x10000;  // SHIFT (IN_SPEED)
	// 004: регулировка подсвеченной adjustable-строки. A/D свободны в раскладке
	// (выбор — E), поэтому «меньше/больше» на них.
	uint64_t keyAdjustDec = 0x200;   // A (IN_MOVELEFT) — уменьшить
	uint64_t keyAdjustInc = 0x400;   // D (IN_MOVERIGHT) — увеличить
	// HTML: display labels for the footer key hints (uppercased key names).
	std::string keyUpLabel = "W";
	std::string keyDownLabel = "S";
	std::string keySelectLabel = "D";
	std::string keyBackLabel = "R";
	std::string keyExitLabel = "F";
	std::string keyExitSpecLabel = "SHIFT";
	std::string keyAdjustDecLabel = "A";
	std::string keyAdjustIncLabel = "D";
	// HTML: hex colors for markup.
	std::string navColor = "#ff2ee7";
	std::string footerColor = "#909090";
	std::string disabledColor = "#808080";
	// 006: цвет захваченной adjustable-строки. Янтарный против розового курсора: захват должен
	// читаться мгновенно и не путаться с обычной подсветкой.
	std::string captureColor = "#f5c211";
	// HTML: минимальный интервал (сек) между перерисовками панели из-за смены строки
	// показаний (SetSlotStatus). 0 — перерисовывать на каждой смене, т.е. с частотой
	// игрового такта: столько же, сколько стоит обычный худ, который шлёт свою панель тем же
	// событием каждый тик. Поднимать только ради трафика (см. kHtmlStatusIntervalMax).
	float htmlStatusInterval = 0.0f;
};

// Backing store for the public ICS2Menus API. Holds menus by handle plus
// per-player display state, renders chat + HTML menus, and routes input.
class MenuManager
{
public:
	// --- API ---
	MenuHandle CreateMenu(MenuType type, const char *title, MenuItemSelectFn onSelect);
	int AddItem(MenuHandle menu, const char *text, const char *info, bool disabled);
	int AddAdjustableItem(MenuHandle menu, const char *text, const char *info, float step, float minValue, float maxValue);
	void SetAdjustCallback(MenuHandle menu, MenuItemAdjustFn onAdjust);
	// 006: режим захвата adjustable-строк для меню (см. ics2menus.h).
	void SetAdjustCapture(MenuHandle menu, bool enabled);
	int AddSubMenu(MenuHandle parent, const char *text, MenuHandle child, const char *info);
	void SetTitle(MenuHandle menu, const char *title);
	void SetExitButton(MenuHandle menu, bool enabled);
	void SetCloseOnSelect(MenuHandle menu, bool enabled);
	void SetMenuEndCallback(MenuHandle menu, MenuEndFn onEnd);
	void SetMenuKey(MenuHandle menu, MenuNavAction action, MenuButton button);
	void SetExitItem(MenuHandle menu, bool enabled);
	void SetMenuLabel(MenuHandle menu, MenuLabel label, const char *text);
	const char *GetMenuLabel(MenuHandle menu, MenuLabel label) const;

	// Live item mutation. Any player currently viewing the menu is re-rendered.
	void SetItemText(MenuHandle menu, int item, const char *text);
	void SetItemInfo(MenuHandle menu, int item, const char *info);
	void SetItemDisabled(MenuHandle menu, int item, bool disabled);
	void RemoveItem(MenuHandle menu, int item);
	void RemoveAllItems(MenuHandle menu);

	bool GetItemDisabled(MenuHandle menu, int item) const;

	// Item the menu opens on (HTML cursor / chat page). Clamped at display.
	void SetStartItem(MenuHandle menu, int item);
	int GetStartItem(MenuHandle menu) const;

	bool DisplayMenu(MenuHandle menu, int slot, float duration, float curtime);
	void CancelMenu(int slot);
	bool HasMenu(int slot) const;
	MenuHandle GetActiveMenu(int slot) const;
	MenuType GetActiveMenuType(int slot) const;
	void DestroyMenu(MenuHandle menu);

	// Abs index of the highlighted item for an HTML menu, or -1 if none / on the Exit row / chat.
	int GetSelectedItem(int slot) const;

	int GetItemCount(MenuHandle menu) const;
	const char *GetItemText(MenuHandle menu, int item) const;
	const char *GetItemInfo(MenuHandle menu, int item) const;

	// Run fn on the main thread: inline if already there, else queued for the next GameFrame.
	// Used by DisplayMenuToAll, which needs main-thread entity access to enumerate players.
	void RunOnMainThread(std::function<void()> fn);

	// Feed a player's raw say message. Returns true if it was consumed as a
	// chat-menu action (caller should suppress the chat line).
	bool ProcessInput(int slot, const char *text, float curtime);

	// True if `slot` has an active HTML menu and thus needs per-frame button polling.
	bool WantsButtonInput(int slot) const;

	// True if any player currently has an HTML menu open.
	bool AnyHtmlMenuActive() const;

	// Feed a player's current held-button bitmask (read from the pawn each frame).
	// Drives HTML-menu navigation on newly-pressed nav keys.
	void PollButtons(int slot, uint64_t heldButtons, float curtime);

	// Drive the open menu from a console/chat command (backup for button input).
	// No-op without an open menu.
	// Up/Down/Select apply to HTML menus only, Back closes either menu type.
	void CommandNav(int slot, MenuNavAction action, float curtime);

	// Select by number (backup for chat menus, e.g. bind a key to "mm_menu_select 3").
	// Routes the number through chat-menu paging/selection (clamped to the page).
	// HTML menus ignore the number and select the cursor row.
	void CommandSelectNumber(int slot, int number, float curtime);

	// Expire timed-out menus and re-send HTML. Call every GameFrame.
	void Tick(float curtime);

	// Close a leaving player's menu (fires MenuEnd=Disconnect).
	void OnPlayerDisconnect(int slot);

	// Yield this slot to a host menu system (SwiftlyS2 / CS#).
	// When busy is set, any open cs2menus menu for the slot is cancelled
	// and further DisplayMenu calls for it are refused until cleared.
	// The host drives this off its own menu open/close. cs2menus never auto-reopens on clear.
	void SetExternalBusy(int slot, bool busy);
	bool GetExternalBusy(int slot) const;

	// 005: строка показаний под футером панели. Подробности контракта — в ics2menus.h.
	void SetSlotStatus(int slot, const char *text);

	// Drop everything without firing callbacks. Call from plugin Unload().
	void Shutdown();

	// Apply settings parsed from core.cfg.
	void Configure(const MenuManagerSettings &settings);

	// Record the calling thread as the main/game thread. Call once from Load.
	// Lets the API run engine-touching work inline, or defer it to GameFrame off-thread.
	void SetMainThread();

	bool OnMainThread() const;

	// Whether HTML menus can render + receive input (set by the plugin after probing).
	// When false, CreateMenu downgrades any HTML menu to chat so it stays usable.
	void SetHtmlAvailable(bool available);

	// Resolve a viewing player's language key for label translation.
	// Set by the plugin from the optional ClientCvarValue interface.
	// When unset (or it returns ""), the translation default language is used.
	void SetLanguageResolver(std::function<std::string(int slot)> resolver);

	// Resolve whether a viewing player is currently spectating (team spectator or
	// active observer mode). Set by the plugin. When unset, players are treated as
	// live, so the Exit key stays keyExit (F). Queried per-slot each poll/render so a
	// mid-menu team switch flips the Exit key on the next frame (see keyExitSpec).
	void SetSpectatorResolver(std::function<bool(int slot)> resolver);

private:
	struct MenuItem
	{
		std::string text;
		std::string info;
		bool disabled = false;
		// Selecting this item navigates into another menu instead of firing onSelect.
		MenuHandle submenu = kInvalidMenuHandle;
		// 004: регулируемая строка — A/D зовут onAdjust с ±step. Значение хранит потребитель.
		bool adjustable = false;
		float step = 0.0f;
		float minValue = 0.0f;
		float maxValue = 0.0f;
	};

	// Per-menu HTML nav-key overrides, indexed by MenuNavAction (Up/Down/Select/Back/Exit).
	// mask 0 = inherit the server config binding for that action.
	struct NavOverride
	{
		uint64_t mask = 0;
		std::string label;
	};

	struct MenuDef
	{
		MenuType type = MenuType::Chat;
		std::string title;
		std::vector<MenuItem> items;
		MenuItemSelectFn onSelect;
		MenuItemAdjustFn onAdjust; // 004: A/D по adjustable-строке
		MenuEndFn onEnd;
		bool exitButton = true;
		bool closeOnSelect = true;
		bool exitItem = false; // HTML: show a selectable "Exit" row in the list
		int startItem = 0;     // item the menu opens on
		// 006: E на adjustable-строке — захват (см. SetAdjustCapture). Off = поведение 004/005.
		bool adjustCapture = false;
		// Set when this menu is reached as a submenu, so Back returns to the parent.
		MenuHandle parent = kInvalidMenuHandle;
		// Indexed by MenuNavAction (Up..AdjustInc). mask 0 = inherit the server config binding.
		NavOverride navOverride[static_cast<int>(MenuNavAction::AdjustInc) + 1];
		// Built-in labels, seeded from settings at CreateMenu, indexed by MenuLabel.
		std::string labels[static_cast<int>(MenuLabel::Count)];
	};

	struct PlayerMenu
	{
		bool active = false;
		MenuHandle handle = kInvalidMenuHandle;
		int page = 0;            // chat pagination
		int cursor = 0;          // html selected option (abs index)
		// 004: направление последней регулировки текущей adjustable-строки: -1 A, +1 D, 0 нет.
		// Красит соответствующую стрелку ◄/► акцентом. Сбрасывается при смене курсора/меню.
		int lastAdjustDir = 0;
		// 006: абсолютный индекс захваченной adjustable-строки (-1 = захвата нет). Живёт только
		// в capture-меню; сбрасывается при любой смене меню/строк и валидируется на каждый доступ
		// (EffectiveCaptureItem) — строка могла исчезнуть или посереть под открытым меню.
		int captureItem = -1;
		float expireTime = 0.0f; // absolute game time, 0 = no expire
		uint64_t prevButtons = 0;
		bool buttonsPrimed = false;
		float nextHtmlRender = 0.0f;
		// Last HTML actually sent + when, so identical refreshes can be skipped.
		std::string lastHtml;
		float lastHtmlSend = 0.0f;
		// A host UI (SwiftlyS2 / CS# menu) owns this slot's screen.
		// While set, we refuse to display so we never fight the host for input or the HTML channel.
		bool externalBusy = false;
		// 005: строка показаний под футером (SetSlotStatus). Живёт на слоте, а не на меню:
		// хэндл общий на всех зрителей, а показания у каждого свои.
		std::string status;
		// Статус меняется на игровом такте, а перерисовка панели — это сетевая отправка всей
		// разметки меню. Копим изменение и отдаём его из Tick, не чаще htmlStatusInterval
		// (деф. 0 — на каждой смене, т.е. с частотой кадра).
		bool statusDirty = false;
		float statusReadyAt = 0.0f;
	};

	MenuDef *Find(MenuHandle menu);
	const MenuDef *Find(MenuHandle menu) const;

	// Core of DisplayMenu.
	// Assumes m_mutex held, runs on main, schedules off m_curtime.
	bool DisplayLocked(MenuHandle menu, int slot, float duration);

	// Close slot's display and fire its MenuEnd.
	// Safe against the callback destroying the menu.
	void EndDisplay(int slot, MenuEndReason reason);

	// Run a selection: invoke onSelect, then close + fire End(Selected)
	// if closeOnSelect, else re-render. Shared by chat and html.
	void Select(int slot, int itemIndex);

	// Swap the slot's displayed menu to `handle` without firing end callbacks.
	// Used for submenu navigation (into a child, or Back to a parent).
	void SwitchMenu(int slot, MenuHandle handle);

	void Render(int slot);     // dispatch to RenderPage/RenderHtml by the slot's menu type
	void RenderPage(int slot); // chat
	void RenderHtml(int slot); // html

	// Re-render every player currently viewing `menu` (after a live mutation).
	// Defers to the next GameFrame if called off the main thread.
	void RefreshMenu(MenuHandle menu);

	// 006: индекс захваченной строки, если захват всё ещё валиден, иначе -1 (и чистит pm).
	int EffectiveCaptureItem(const MenuDef &def, PlayerMenu &pm) const;

	// html navigation
	void HtmlMoveCursor(int slot, int delta);
	// 004: route A/D on a highlighted adjustable row to onAdjust with ±step. HTML only.
	// No-op if the cursor isn't on an adjustable item or no callback is set. dir: +1 inc, -1 dec.
	void HtmlAdjust(int slot, int dir);
	// Activate the cursor row (exit row closes, else selects the item). HTML only.
	void HtmlNavSelect(int slot);
	// Step back to the parent submenu, or exit the menu. Chat or HTML.
	// Back: только вверх к родителю (no-op на корне); Exit: закрыть меню.
	void NavBack(int slot);
	void NavExit(int slot);
	// Apply a chat-menu number (1..page select, Next/Prev/Exit reserved). True if consumed.
	bool ApplyChatNumber(int slot, int num);

	// Effective nav binding for an action (per-menu override, else server config).
	uint64_t EffectiveNavMask(const MenuDef &def, MenuNavAction action) const;
	std::string EffectiveNavLabel(const MenuDef &def, MenuNavAction action) const;

	// Exit binding with the player's context folded in: a per-menu Exit override
	// wins for everyone; otherwise a spectator gets keyExitSpec (Shift) and a live
	// player keyExit (F). Slot-aware because "is spectating" is per-player + dynamic.
	uint64_t EffectiveExitMask(const MenuDef &def, int slot) const;
	std::string EffectiveExitLabel(const MenuDef &def, int slot) const;

	// True if the player in `slot` is currently spectating (via the resolver; false if unset).
	bool IsSpectator(int slot) const;

	// Built-in phrase key for a label (seeds MenuDef, restores on SetMenuLabel("")).
	static const char *DefaultLabelKey(MenuLabel label);
	// Translate this menu's label for the player viewing in `slot`.
	// Resolves the viewer's language, then looks the per-menu key up in the phrase table.
	std::string ResolveLabel(int slot, const MenuDef &def, MenuLabel label) const;
	// То же с УЖЕ разрешённым языком слота. RenderHtml переводит до шести подписей за проход,
	// а сам проход теперь идёт каждый кадр (строка показаний) — резолвить язык на каждую
	// подпись значило бы шесть лишних ToLower+аллокаций на кадр на зрителя.
	std::string ResolveLabelLang(const std::string &lang, const MenuDef &def, MenuLabel label) const;
	// Язык слота для переводов (пустая строка = язык по умолчанию).
	std::string SlotLanguage(int slot) const;

	// HTML: whether to render the selectable "Exit" row (after the last item).
	// Shown when the menu is exitable and either the toggle is on or the Back key
	// is disabled, so a menu is never left unexitable.
	bool HtmlShowsExitRow(const MenuDef &def) const;
	// HTML: total navigable rows = items + (exit row ? 1 : 0).
	int HtmlRowCount(const MenuDef &def) const;

	std::unordered_map<MenuHandle, MenuDef> m_menus;
	MenuHandle m_nextHandle = 1;
	PlayerMenu m_players[MAXPLAYERS + 1];

	// Guards all of the above. Recursive: a callback fired while held may re-enter the API.
	mutable std::recursive_mutex m_mutex;

	// The game thread (render/callbacks must run here). Set by SetMainThread in Load.
	std::thread::id m_mainThread;

	// Queued by off-thread callers, drained on main at the top of Tick (lock held).
	std::vector<std::function<void()>> m_pending;

	MenuManagerSettings m_settings;
	// Effective clamped copies of the size settings.
	int m_itemsPerPage = MENU_MAX_ITEMS_PER_PAGE;
	int m_htmlVisibleItems = MENU_MAX_HTML_VISIBLE;
	// Клампнутый htmlStatusInterval (0..kHtmlStatusIntervalMax). Недоверенное значение из
	// конфига могло прийти отрицательным или в минутах.
	float m_htmlStatusInterval = 0.0f;
	// HTML rendering+input usable (see SetHtmlAvailable). Off until proven.
	bool m_htmlAvailable = false;
	// Guards against unbounded reentrancy when a consumer callback re-enters API.
	int m_callbackDepth = 0;
	// Latest game time, refreshed by Tick/PollButtons/DisplayMenu so HTML render
	// scheduling doesn't need curtime threaded through every call.
	float m_curtime = 0.0f;

	// Maps a slot to its language key for label translation (see SetLanguageResolver).
	std::function<std::string(int)> m_langResolver;

	// Maps a slot to "is spectating" for the spectator-safe Exit key (see SetSpectatorResolver).
	std::function<bool(int)> m_isSpectatorResolver;
};

extern MenuManager g_MenuManager;

#endif // _INCLUDE_MENU_MANAGER_H_
