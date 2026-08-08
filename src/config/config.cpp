#include "config.h"
#include "kv_parser.h"
// src/common.h — ради META_CONPRINTF (он из ISmmPlugin.h); тем же путём его берёт
// src/entity/schema.cpp.
#include "src/common.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <string>

MenusConfig g_MenusConfig;

static std::string ToLower(const std::string &s)
{
	std::string r = s;
	std::transform(r.begin(), r.end(), r.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	return r;
}

// Неотрицательные секунды с дробной частью. Разбор ручной, БЕЗ atof/strtod: те смотрят на
// LC_NUMERIC, и на сервере с запятой-разделителем "0.05" молча стало бы нулём — то есть ровно
// тем режимом, ради ухода от которого этот ключ и трогают. Принимаем оба разделителя.
// Мусор/минус/пустое = 0 (дефолт), верхнюю границу держит кламп в MenuManager::Configure.
static float ParseSeconds(const std::string &s)
{
	size_t i = 0;
	while (i < s.size() && (s[i] == ' ' || s[i] == '\t'))
	{
		i++;
	}
	double value = 0.0;
	bool any = false;
	for (; i < s.size() && s[i] >= '0' && s[i] <= '9'; i++)
	{
		value = value * 10.0 + (s[i] - '0');
		any = true;
	}
	if (i < s.size() && (s[i] == '.' || s[i] == ','))
	{
		i++;
		double scale = 0.1;
		for (; i < s.size() && s[i] >= '0' && s[i] <= '9'; i++)
		{
			value += (s[i] - '0') * scale;
			scale *= 0.1;
			any = true;
		}
	}
	// Хвост после числа — почти всегда опечатка в ключе («0.5s», «+0.5», «1e-2»), и молчать
	// тут нельзя: значение применится не то, о котором думал оператор, а трогают этот ключ
	// ровно на инциденте. Разобранное печатаем, чтобы расхождение было видно сразу.
	while (i < s.size() && (s[i] == ' ' || s[i] == '\t'))
	{
		i++;
	}
	const float parsed = any ? static_cast<float>(value) : 0.0f;
	if (i < s.size() || !any)
	{
		META_CONPRINTF("[cs2menus] WARNING: не разобрано значение \"%s\", применено %.5f\n", s.c_str(), parsed);
	}
	return parsed;
}

static void ConfigHandler(const std::string &section, const std::string &key, const std::string &value, void *userdata)
{
	MenusConfig *cfg = static_cast<MenusConfig *>(userdata);
	std::string sec = ToLower(section);
	std::string k = ToLower(key);

	if (sec == "general")
	{
		if (k == "commandprefix")
		{
			cfg->general.commandPrefix = value;
		}
		else if (k == "silentcommandprefix")
		{
			cfg->general.silentCommandPrefix = value;
		}
	}
	else if (sec == "menu")
	{
		if (k == "defaulttype")
		{
			cfg->menu.defaultType = ToLower(value);
		}
		else if (k == "defaultlanguage")
		{
			cfg->menu.defaultLanguage = ToLower(value);
		}
		else if (k == "itemsperpage")
		{
			cfg->menu.itemsPerPage = std::atoi(value.c_str());
		}
		else if (k == "exitbutton")
		{
			cfg->menu.exitButton = (value != "0");
		}
		else if (k == "htmlvisibleitems")
		{
			cfg->menu.htmlVisibleItems = std::atoi(value.c_str());
		}
		else if (k == "htmlexititem")
		{
			cfg->menu.htmlExitItem = (value != "0");
		}
		else if (k == "htmlnavcolor")
		{
			cfg->menu.htmlNavColor = value;
		}
		else if (k == "htmlfootercolor")
		{
			cfg->menu.htmlFooterColor = value;
		}
		else if (k == "htmldisabledcolor")
		{
			cfg->menu.htmlDisabledColor = value;
		}
		else if (k == "htmlstatusinterval")
		{
			cfg->menu.htmlStatusInterval = ParseSeconds(value);
		}
		else if (k == "htmlfixflashing")
		{
			cfg->menu.htmlFixFlashing = (value != "0");
		}
		else if (k == "navup")
		{
			cfg->menu.navUp = ToLower(value);
		}
		else if (k == "navdown")
		{
			cfg->menu.navDown = ToLower(value);
		}
		else if (k == "navselect")
		{
			cfg->menu.navSelect = ToLower(value);
		}
		else if (k == "navback")
		{
			cfg->menu.navBack = ToLower(value);
		}
		else if (k == "navexit")
		{
			cfg->menu.navExit = ToLower(value);
		}
		else if (k == "navexitspec")
		{
			cfg->menu.navExitSpec = ToLower(value);
		}
		else if (k == "navadjustdec")
		{
			cfg->menu.navAdjustDec = ToLower(value);
		}
		else if (k == "navadjustinc")
		{
			cfg->menu.navAdjustInc = ToLower(value);
		}
	}
}

bool MENU_LoadConfig(const char *path, MenusConfig &config)
{
	std::ifstream file(path);
	if (!file.is_open())
	{
		return false;
	}

	// "cs2menus" { ... }
	kv::Token root = kv::NextToken(file);
	if (root.kind != kv::TokenType::String)
	{
		return false;
	}

	kv::Token brace = kv::NextToken(file);
	if (brace.kind != kv::TokenType::OpenBrace)
	{
		return false;
	}

	kv::ParseSection(file, root.value, ConfigHandler, &config);
	return true;
}
