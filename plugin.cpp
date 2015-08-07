/*
 * Poke Blocker :: TeamSpeak 3 Client plugin to block all pokes if enabled
 *
 * Developed by sk0r / Czybik
 */
#ifdef _WIN32
#pragma warning (disable : 4100)  /* Disable unreferenced parameter warning */
#include <Windows.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <string>
#include <time.h>
#include <cctype>
#include <iomanip>
#include <sstream>
#include "public_errors.h"
#include "public_errors_rare.h"
#include "public_definitions.h"
#include "public_rare_definitions.h"
#include "ts3_functions.h"
#include "plugin.h"
static struct TS3Functions ts3Functions;
#ifdef _WIN32
#define _strcpy(dest, destSize, src) strcpy_s(dest, destSize, src)
#define snprintf sprintf_s
#else
#define _strcpy(dest, destSize, src) { strncpy(dest, src, destSize-1); (dest)[destSize-1] = '\0'; }
#endif

#define PLUGIN_API_VERSION 20
#define PATH_BUFSIZE 512
#define COMMAND_BUFSIZE 128
#define INFODATA_BUFSIZE 128
#define SERVERINFO_BUFSIZE 256
#define CHANNELINFO_BUFSIZE 512
#define RETURNCODE_BUFSIZE 128
#define PLUGIN_NAME "Anti-Spam"
#define PLUGIN_AUTHOR "Bluscream"
#define PLUGIN_VERSION "0.1"
#define PLUGIN_CONTACT "admin@timo.de.vc"
static char* pluginID = NULL;

bool g_AntiSpamEnabled = true; //Temporary blocks all other functions when false
bool g_IgnorePokes = false; //Ignores all pokes (Does not show anything about any pokes)
bool g_IgnoreEmptyPokes = false; //Ignores empty pokes
bool g_IgnoreDuplicatePokes = true; //Ignores duplicate pokes
bool g_IgnoreLongPokes = false; //Ignores pokes longer then 3/4 of max
bool g_ConvertPokes = false; //Converts pokes to chat messages
bool g_ShowBlockedPokes = false; //Temporary shows pokes that normally would be ignored by the client

bool g_IgnoreMessages = false; //Ignores all messages (Does not open any new chat window)
bool g_IgnoreEmptyMessages = false; //Ingores empty chat messages
bool g_IgnoreDuplicateMessages = true; //Ingores duplicate chat messages
bool g_IgnoreLongMessages = true; //Ignores messages longer then 3/4 of max
bool g_ConvertMessages = false; //Converts chat to log messages
bool g_ShowBlockedMessages = false; //Temporary shows chat messages that normally would be ignored by the client

char* lastPokeMSG = "";
char* lastChatMSG = "";

#ifdef _WIN32
static int wcharToUtf8(const wchar_t* str, char** result) {
	int outlen = WideCharToMultiByte(CP_UTF8, 0, str, -1, 0, 0, 0, 0);
	*result = (char*)malloc(outlen);
	if(WideCharToMultiByte(CP_UTF8, 0, str, -1, *result, outlen, 0, 0) == 0) {
		*result = NULL;
		return -1;
	}
	return 0;
}
#endif
using namespace std;
string url_encode(const string &value) {
	ostringstream escaped;
	escaped.fill('0');
	escaped << hex;
	for (string::const_iterator i = value.begin(), n = value.end(); i != n; ++i) {
		string::value_type c = (*i);
		if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
			escaped << c;
			continue;
		}
		escaped << '%' << setw(2) << int((unsigned char)c);
	}
	return escaped.str();
}
/*********************************** Required functions ************************************/
/*
 * If any of these required functions is not implemented, TS3 will refuse to load the plugin
 */
/* Unique name identifying this plugin */
const char* ts3plugin_name() {
#ifdef _WIN32
	/* TeamSpeak expects UTF-8 encoded characters. Following demonstrates a possibility how to convert UTF-16 wchar_t into UTF-8. */
	static char* result = NULL;  /* Static variable so it's allocated only once */
	if(!result) {
		const wchar_t* name = TEXT(PLUGIN_NAME);
		if(wcharToUtf8(name, &result) == -1) {  /* Convert name into UTF-8 encoded result */
			result = PLUGIN_NAME;  /* Conversion failed, fallback here */
		}
	}
	return result;
#else
	return PLUGIN_NAME;
#endif
}
/* Plugin version */
const char* ts3plugin_version() {
    return PLUGIN_VERSION;
}
/* Plugin API version. Must be the same as the clients API major version, else the plugin fails to load. */
int ts3plugin_apiVersion() {
	return PLUGIN_API_VERSION;
}
/* Plugin author */
const char* ts3plugin_author() {
	/* If you want to use wchar_t, see ts3plugin_name() on how to use */
    return PLUGIN_AUTHOR;
}
/* Plugin description */
const char* ts3plugin_description() {
	/* If you want to use wchar_t, see ts3plugin_name() on how to use */
    return PLUGIN_NAME "\n\n"
		"Purpose:\n"
		"A shield against spam.\n\n"
		"Developed by " PLUGIN_AUTHOR " (" PLUGIN_CONTACT ")\n";
}
/* Set TeamSpeak 3 callback functions */
void ts3plugin_setFunctionPointers(const struct TS3Functions funcs) {
    ts3Functions = funcs;
}
/*
 * Custom code called right after loading the plugin. Returns 0 on success, 1 on failure.
 * If the function returns 1 on failure, the plugin will be unloaded again.
 */
int ts3plugin_init() {
    char appPath[PATH_BUFSIZE];
    char resourcesPath[PATH_BUFSIZE];
    char configPath[PATH_BUFSIZE];
	char pluginPath[PATH_BUFSIZE];
    /* Your plugin init code here */
    
    /* Example on how to query application, resources and configuration paths from client */
    /* Note: Console client returns empty string for app and resources path */
    ts3Functions.getAppPath(appPath, PATH_BUFSIZE);
    ts3Functions.getResourcesPath(resourcesPath, PATH_BUFSIZE);
    ts3Functions.getConfigPath(configPath, PATH_BUFSIZE);
	ts3Functions.getPluginPath(pluginPath, PATH_BUFSIZE);
	
    return 0;  /* 0 = success, 1 = failure, -2 = failure but client will not show a "failed to load" warning */
	/* -2 is a very special case and should only be used if a plugin displays a dialog (e.g. overlay) asking the user to disable
	 * the plugin again, avoiding the show another dialog by the client telling the user the plugin failed to load.
	 * For normal case, if a plugin really failed to load because of an error, the correct return value is 1. */
#
	if (g_IgnorePokes) {
		ts3Functions.printMessageToCurrentTab(PLUGIN_NAME" started. Now blocking pokes!");
	}
	else {
		ts3Functions.printMessageToCurrentTab(PLUGIN_NAME" started.");
	}
}
/* Custom code called right before the plugin is unloaded */
void ts3plugin_shutdown() {
    /* Your plugin cleanup code here */
    
	/*
	 * Note:
	 * If your plugin implements a settings dialog, it must be closed and deleted here, else the
	 * TeamSpeak client will most likely crash (DLL removed but dialog from DLL code still open).
	 */
	/* Free pluginID if we registered it */
	if(pluginID) {
		free(pluginID);
		pluginID = NULL;
	}
}
/****************************** Optional functions ********************************/
/*
 * Following functions are optional, if not needed you don't need to implement them.
 */
/* Tell client if plugin offers a configuration window. If this function is not implemented, it's an assumed "does not offer" (PLUGIN_OFFERS_NO_CONFIGURE). */
int ts3plugin_offersConfigure() {
	
	/*
	 * Return values:
	 * PLUGIN_OFFERS_NO_CONFIGURE         - Plugin does not implement ts3plugin_configure
	 * PLUGIN_OFFERS_CONFIGURE_NEW_THREAD - Plugin does implement ts3plugin_configure and requests to run this function in an own thread
	 * PLUGIN_OFFERS_CONFIGURE_QT_THREAD  - Plugin does implement ts3plugin_configure and requests to run this function in the Qt GUI thread
	 */
	return PLUGIN_OFFERS_NO_CONFIGURE;  /* In this case ts3plugin_configure does not need to be implemented */
}
/* Plugin might offer a configuration window. If ts3plugin_offersConfigure returns 0, this function does not need to be implemented. */
void ts3plugin_configure(void* handle, void* qParentWidget) {
    
}
/*
 * If the plugin wants to use error return codes, plugin commands, hotkeys or menu items, it needs to register a command ID. This function will be
 * automatically called after the plugin was initialized. This function is optional. If you don't use these features, this function can be omitted.
 * Note the passed pluginID parameter is no longer valid after calling this function, so you must copy it and store it in the plugin.
 */
void ts3plugin_registerPluginID(const char* id) {
	const size_t sz = strlen(id) + 1;
	pluginID = (char*)malloc(sz * sizeof(char));
	_strcpy(pluginID, sz, id);  /* The id buffer will invalidate after exiting this function */
	
}
/* Plugin command keyword. Return NULL or "" if not used. */
const char* ts3plugin_commandKeyword() {
	return NULL;
}
/*
 * Implement the following three functions when the plugin should display a line in the server/channel/client info.
 * If any of ts3plugin_infoTitle, ts3plugin_infoData or ts3plugin_freeMemory is missing, the info text will not be displayed.
 */
/* Static title shown in the left column in the info frame */
/*
 * Dynamic content shown in the right column in the info frame. Memory for the data string needs to be allocated in this
 * function. The client will call ts3plugin_freeMemory once done with the string to release the allocated memory again.
 * Check the parameter "type" if you want to implement this feature only for specific item types. Set the parameter
 * "data" to NULL to have the client ignore the info data.
 */
/* Required to release the memory for parameter "data" allocated in ts3plugin_infoData and ts3plugin_initMenus */
void ts3plugin_freeMemory(void* data) {
	free(data);
}
/*
 * Plugin requests to be always automatically loaded by the TeamSpeak 3 client unless
 * the user manually disabled it in the plugin dialog.
 * This function is optional. If missing, no autoload is assumed.
 */
int ts3plugin_requestAutoload() {
	return 1;  /* 1 = request autoloaded, 0 = do not request autoload */
}
/* Helper function to create a menu item */
static struct PluginMenuItem* createMenuItem(enum PluginMenuType type, int id, const char* text, const char* icon) {
	struct PluginMenuItem* menuItem = (struct PluginMenuItem*)malloc(sizeof(struct PluginMenuItem));
	menuItem->type = type;
	menuItem->id = id;
	_strcpy(menuItem->text, PLUGIN_MENU_BUFSZ, text);
	_strcpy(menuItem->icon, PLUGIN_MENU_BUFSZ, icon);
	return menuItem;
}
/* Some makros to make the code to create menu items a bit more readable */
#define BEGIN_CREATE_MENUS(x) const size_t sz = x + 1; size_t n = 0; *menuItems = (struct PluginMenuItem**)malloc(sizeof(struct PluginMenuItem*) * sz);
#define CREATE_MENU_ITEM(a, b, c, d) (*menuItems)[n++] = createMenuItem(a, b, c, d);
#define END_CREATE_MENUS (*menuItems)[n++] = NULL; assert(n == sz);
/*
 * Menu IDs for this plugin. Pass these IDs when creating a menuitem to the TS3 client. When the menu item is triggered,
 * ts3plugin_onMenuItemEvent will be called passing the menu ID of the triggered menu item.
 * These IDs are freely choosable by the plugin author. It's not really needed to use an enum, it just looks prettier.
 */
enum {
	MENU_ID_GLOBAL_1,
	MENU_ID_GLOBAL_2,
	MENU_ID_GLOBAL_3,
	MENU_ID_GLOBAL_4,
	MENU_ID_GLOBAL_5,
	MENU_ID_GLOBAL_6,
	MENU_ID_GLOBAL_7,
	MENU_ID_GLOBAL_8,
	MENU_ID_GLOBAL_9,
	MENU_ID_GLOBAL_10,
	MENU_ID_GLOBAL_11,
	MENU_ID_GLOBAL_12,
	MENU_ID_GLOBAL_13,
	MENU_ID_GLOBAL_14,
	MENU_ID_GLOBAL_15,
	MENU_ID_GLOBAL_16,
	MENU_ID_GLOBAL_17,
	MENU_ID_GLOBAL_18,
	MENU_ID_GLOBAL_19,
	MENU_ID_GLOBAL_20,
	MENU_ID_GLOBAL_21
};
/*
 * Initialize plugin menus.
 * This function is called after ts3plugin_init and ts3plugin_registerPluginID. A pluginID is required for plugin menus to work.
 * Both ts3plugin_registerPluginID and ts3plugin_freeMemory must be implemented to use menus.
 * If plugin menus are not used by a plugin, do not implement this function or return NULL.
 */
void ts3plugin_initMenus(struct PluginMenuItem*** menuItems, char** menuIcon) {
	*menuIcon = (char*)malloc(PLUGIN_MENU_BUFSZ * sizeof(char));
	_strcpy(*menuIcon, PLUGIN_MENU_BUFSZ, "main.png");
	BEGIN_CREATE_MENUS(21);  /* IMPORTANT: Number of menu items must be correct! */
	CREATE_MENU_ITEM(PLUGIN_MENU_TYPE_GLOBAL, MENU_ID_GLOBAL_1, "De-/Activate", "toggle.png");
	CREATE_MENU_ITEM(PLUGIN_MENU_TYPE_GLOBAL, MENU_ID_GLOBAL_2, "-----------------------", "");
	CREATE_MENU_ITEM(PLUGIN_MENU_TYPE_GLOBAL, MENU_ID_GLOBAL_3, "Ignore Pokes... ", "poke.png");
	CREATE_MENU_ITEM(PLUGIN_MENU_TYPE_GLOBAL, MENU_ID_GLOBAL_4, "...Empty Pokes", "poke.png");
	CREATE_MENU_ITEM(PLUGIN_MENU_TYPE_GLOBAL, MENU_ID_GLOBAL_5, "...Duplicate Pokes", "poke.png");
	CREATE_MENU_ITEM(PLUGIN_MENU_TYPE_GLOBAL, MENU_ID_GLOBAL_6, "...Long Pokes", "poke.png");
	CREATE_MENU_ITEM(PLUGIN_MENU_TYPE_GLOBAL, MENU_ID_GLOBAL_7, "-----------------------", "");
	CREATE_MENU_ITEM(PLUGIN_MENU_TYPE_GLOBAL, MENU_ID_GLOBAL_8, "Ignore messages... ", "msg.png");
	CREATE_MENU_ITEM(PLUGIN_MENU_TYPE_GLOBAL, MENU_ID_GLOBAL_9, "...Empty messages", "msg.png");
	CREATE_MENU_ITEM(PLUGIN_MENU_TYPE_GLOBAL, MENU_ID_GLOBAL_10, "...Duplicate messages", "msg.png");
	CREATE_MENU_ITEM(PLUGIN_MENU_TYPE_GLOBAL, MENU_ID_GLOBAL_11, "...Long messages", "msg.png");
	CREATE_MENU_ITEM(PLUGIN_MENU_TYPE_GLOBAL, MENU_ID_GLOBAL_12, "-----------------------", "");
	CREATE_MENU_ITEM(PLUGIN_MENU_TYPE_GLOBAL, MENU_ID_GLOBAL_13, "Poke to message", "poke.png");
	//CREATE_MENU_ITEM(PLUGIN_MENU_TYPE_GLOBAL, MENU_ID_GLOBAL_22, "Poke to log", "poke.png");
	CREATE_MENU_ITEM(PLUGIN_MENU_TYPE_GLOBAL, MENU_ID_GLOBAL_14, "Message to log", "msg.png");
	//CREATE_MENU_ITEM(PLUGIN_MENU_TYPE_GLOBAL, MENU_ID_GLOBAL_23, "Message to poke", "msg.png");
	CREATE_MENU_ITEM(PLUGIN_MENU_TYPE_GLOBAL, MENU_ID_GLOBAL_15, "-----------------------", "");
	CREATE_MENU_ITEM(PLUGIN_MENU_TYPE_GLOBAL, MENU_ID_GLOBAL_16, "Show blocked Pokes", "poke.png");
	CREATE_MENU_ITEM(PLUGIN_MENU_TYPE_GLOBAL, MENU_ID_GLOBAL_17, "Show blocked Messages", "msg.png");
	CREATE_MENU_ITEM(PLUGIN_MENU_TYPE_GLOBAL, MENU_ID_GLOBAL_18, "-----------------------", "");
	CREATE_MENU_ITEM(PLUGIN_MENU_TYPE_GLOBAL, MENU_ID_GLOBAL_19, "Info", "info.png");
	CREATE_MENU_ITEM(PLUGIN_MENU_TYPE_GLOBAL, MENU_ID_GLOBAL_20, "Help", "help.png");
	CREATE_MENU_ITEM(PLUGIN_MENU_TYPE_GLOBAL, MENU_ID_GLOBAL_21, "About", "about.png");
	END_CREATE_MENUS;  /* Includes an assert checking if the number of menu items matched */
}
template <typename T>
std::string tostring(const T& t)
{
	std::ostringstream ss;
	ss << t;
	return ss.str();
}
std::string String(double Val) {
	std::ostringstream Stream;
	Stream << Val;
	return Stream.str();
}
int ts3plugin_onTextMessageEvent(uint64 serverConnectionHandlerID, anyID targetMode, anyID toID, anyID fromID, const char* fromName, const char* fromUniqueIdentifier, const char* message, int ffIgnored) {
	if (g_AntiSpamEnabled) {
		if (g_IgnoreMessages) {
			return 1;
		}
		if (ffIgnored) {
			if (g_ShowBlockedMessages) {
				if (g_ConvertMessages) {
					std::string str = " > ";
					std::string szInfoMsg = PLUGIN_NAME + std::string(str) + std::string(fromName) + ": " + std::string(message);
					ts3Functions.logMessage(szInfoMsg.c_str(), LogLevel_INFO, "Plugin", serverConnectionHandlerID);
				}
				else {
					std::string(fromNameEncoded) = url_encode(fromName);
					std::string szInfoMsg = "[color=green]\\./[/color][color=black] [color=red][url=client://0/" + std::string(fromUniqueIdentifier) + "~" + std::string(fromNameEncoded) + "]\"" + std::string(fromName) + "\"[/url][/color]: [color=black]" + std::string(message) + "[/color]";
					ts3Functions.printMessageToCurrentTab(szInfoMsg.c_str());
				}
				return 1;
				std::string lastChatMSG = message;
			}
			else {
				return 0;
				std::string lastChatMSG = message;
			}
		}
	}
	else if (g_IgnoreDuplicateMessages) {
		if (std::string(message) == lastChatMSG) {
			return 1;
			std::string lastChatMSG = message;
		}
		else {
			return 0;
			std::string lastChatMSG = message;
		}
	}
	else if (g_IgnoreEmptyMessages) {
		if (std::string(message) == "") {
			return 1;
			std::string lastChatMSG = message;
		}
		else {
			return 0;
			std::string lastChatMSG = message;
		}
	}
	else if (g_IgnoreLongMessages) {
		std::string msg = message;
		if (msg.length() > 450) {
			return 1;
			std::string lastChatMSG = message;
		}
		else {
			return 0;
			std::string lastChatMSG = message;
		}
	}
	else {
		return 0;
	}
}
int ts3plugin_onClientPokeEvent(uint64 serverConnectionHandlerID, anyID fromClientID, const char* pokerName, const char* pokerUniqueIdentity, const char* message, int ffIgnored) {
	uint64 clientChannelID;
	char* clientChannelIDString;
	char* clientChannelName;
	if (g_AntiSpamEnabled) {
		if (g_IgnorePokes) {
			return 1;
		}
		else if (ffIgnored) {
			if (g_ShowBlockedPokes) {
				if (g_ConvertPokes) {
					ts3Functions.getChannelOfClient(serverConnectionHandlerID, fromClientID, &clientChannelID);
					std::string clientChannelIDString = tostring(clientChannelID);
					ts3Functions.getChannelVariableAsString(serverConnectionHandlerID, clientChannelID, CHANNEL_NAME, &clientChannelName);
					std::string(pokerNameEncoded) = url_encode(pokerName);
					if (std::string(message) == "") {
						std::string szInfoMsg = "[color=black] Blocked poke from user \"[color=red][url=client://0/" + std::string(pokerUniqueIdentity) + "~" + std::string(pokerNameEncoded) + "]" + std::string(pokerName) + "[/url][/color]\" in channel \'[URL=channelid://" + std::string(clientChannelIDString) + "]" + clientChannelName + "[/url]\'\"\n[/color]";
						ts3Functions.printMessageToCurrentTab(szInfoMsg.c_str());
					}
					else {
						std::string szInfoMsg = "[color=black] Blocked poke from user \"[color=red][url=client://0/" + std::string(pokerUniqueIdentity) + "~" + std::string(pokerNameEncoded) + "]" + std::string(pokerName) + "[/url][/color]\" in channel \'[URL=channelid://" + std::string(clientChannelIDString) + "]" + clientChannelName + "[/url]\' with message \"[color=black]" + std::string(message) + "[/color]\"\n[/color]";
						ts3Functions.printMessageToCurrentTab(szInfoMsg.c_str());
					}
					return 1;
				}
				else {
					if (std::string(message) == "") {
						MessageBoxA(0, pokerName, "You Have Been Poked", MB_ICONINFORMATION);
					}
					else {
						MessageBoxA(0, message, pokerName, MB_ICONINFORMATION);
					}
					return 1;
				}
			}
		}
		else if (g_ConvertPokes) {
			ts3Functions.getChannelOfClient(serverConnectionHandlerID, fromClientID, &clientChannelID);
			std::string clientChannelIDString = tostring(clientChannelID);
			ts3Functions.getChannelVariableAsString(serverConnectionHandlerID, clientChannelID, CHANNEL_NAME, &clientChannelName);
			std::string(pokerNameEncoded) = url_encode(pokerName);
			if (std::string(message) == "") {
				std::string szInfoMsg = "[color=black] Blocked poke from user \"[color=red][url=client://0/" + std::string(pokerUniqueIdentity) + "~" + std::string(pokerNameEncoded) + "]" + std::string(pokerName) + "[/url][/color]\" in channel \'[URL=channelid://" + std::string(clientChannelIDString) + "]" + clientChannelName + "[/url]\'\"\n[/color]";
				ts3Functions.printMessageToCurrentTab(szInfoMsg.c_str());
			}
			else {
				std::string szInfoMsg = "[color=black] Blocked poke from user \"[color=red][url=client://0/" + std::string(pokerUniqueIdentity) + "~" + std::string(pokerNameEncoded) + "]" + std::string(pokerName) + "[/url][/color]\" in channel \'[URL=channelid://" + std::string(clientChannelIDString) + "]" + clientChannelName + "[/url]\' with message \"[color=black]" + std::string(message) + "[/color]\"\n[/color]";
				ts3Functions.printMessageToCurrentTab(szInfoMsg.c_str());
			}
			return 1;
		}
		else if (g_IgnoreDuplicatePokes) {
			if (std::string(message) == lastPokeMSG) {
				return 1;
			}
			else {
				return 0;
			}
		}
		else if (g_IgnoreEmptyPokes) {
			if (std::string(message) == "") {
				return 1;
			}
			else {
				return 0;
			}
		}
		else {
			return 0;
		}
	}
	std::string lastPokeMSG = message;
}
/*
 * Called when a plugin menu item (see ts3plugin_initMenus) is triggered. Optional function, when not using plugin menus, do not implement this.
 * 
 * Parameters:
 * - serverConnectionHandlerID: ID of the current server tab
 * - type: Type of the menu (PLUGIN_MENU_TYPE_CHANNEL, PLUGIN_MENU_TYPE_CLIENT or PLUGIN_MENU_TYPE_GLOBAL)
 * - menuItemID: Id used when creating the menu item
 * - selectedItemID: Channel or Client ID in the case of PLUGIN_MENU_TYPE_CHANNEL and PLUGIN_MENU_TYPE_CLIENT. 0 for PLUGIN_MENU_TYPE_GLOBAL.
 */
void ts3plugin_onMenuItemEvent(uint64 serverConnectionHandlerID, enum PluginMenuType type, int menuItemID, uint64 selectedItemID) {
	
	if (type == PLUGIN_MENU_TYPE_GLOBAL) {
		/* Global menu item was triggered. selectedItemID is unused and set to zero. */
		switch(menuItemID) {
			case MENU_ID_GLOBAL_1: {
				if (!g_AntiSpamEnabled) {
					g_AntiSpamEnabled = true;
					ts3Functions.printMessageToCurrentTab("[color=black][[/color]" PLUGIN_NAME "[color=black]] **[/color] [color=green]Plugin enabled[/color] [color=black]**[/color]\n");
				}
				else {
					g_AntiSpamEnabled = false;
					ts3Functions.printMessageToCurrentTab("[color=black][[/color]" PLUGIN_NAME "[color=black]] **[/color][color=red]Plugin disabled[/color] [color=black]**[/color]\n");
				}
				break;
			} case MENU_ID_GLOBAL_3: {
				if (!g_IgnorePokes) {
					g_IgnorePokes = true;
					ts3Functions.printMessageToCurrentTab("[" PLUGIN_NAME "] ** [color=green]Now completely ignoring pokes[/color] **\n");
				}
				else {
					g_IgnorePokes = false;
					ts3Functions.printMessageToCurrentTab("[" PLUGIN_NAME "] ** [color=red]No longer ignoring pokes[/color] **\n");
				}
				break;
			} case MENU_ID_GLOBAL_4: {
				if (!g_IgnoreEmptyPokes) {
					g_IgnoreEmptyPokes = true;
					ts3Functions.printMessageToCurrentTab("[" PLUGIN_NAME "] ** [color=green]Now ignoring empty pokes[/color] **\n");
				}
				else {
					g_IgnoreEmptyPokes = false;
					ts3Functions.printMessageToCurrentTab("[" PLUGIN_NAME "] ** [color=red]No longer ignoring empty pokes[/color] **\n");
				}
				break;
			} case MENU_ID_GLOBAL_5: {
				if (!g_IgnoreDuplicatePokes) {
					g_IgnoreDuplicatePokes = true;
					ts3Functions.printMessageToCurrentTab("[" PLUGIN_NAME "] ** [color=green]Now ignoring duplicate pokes[/color] **\n");
				}
				else {
					g_IgnoreDuplicatePokes = false;
					ts3Functions.printMessageToCurrentTab("[" PLUGIN_NAME "] ** [color=red]No longer ignoring duplicate pokes[/color] **\n");
				}
				break;
			} case MENU_ID_GLOBAL_6: {
				if (!g_IgnoreLongPokes) {
					g_IgnoreLongPokes = true;
					ts3Functions.printMessageToCurrentTab("[" PLUGIN_NAME "] ** [color=green]Now ignoring long pokes[/color] **\n");
				}
				else {
					g_IgnoreLongPokes = false;
					ts3Functions.printMessageToCurrentTab("[" PLUGIN_NAME "] ** [color=red]No longer ignoring long pokes[/color] **\n");
				}
				break;
			} case MENU_ID_GLOBAL_8: {
				if (!g_IgnoreMessages) {
					g_IgnoreMessages = true;
					ts3Functions.printMessageToCurrentTab("[" PLUGIN_NAME "] ** [color=green]Now ignoring messages[/color] **\n");
				}
				else {
					g_IgnoreMessages = false;
					ts3Functions.printMessageToCurrentTab("[" PLUGIN_NAME "] ** [color=red]No longer ignoring messages[/color] **\n");
				}
				break;
			} case MENU_ID_GLOBAL_9: {
				if (!g_IgnoreEmptyMessages) {
					g_IgnoreEmptyMessages = true;
					ts3Functions.printMessageToCurrentTab("[" PLUGIN_NAME "] ** [color=green]Now ignoring empty messages[/color] **\n");
				}
				else {
					g_IgnoreEmptyMessages = false;
					ts3Functions.printMessageToCurrentTab("[" PLUGIN_NAME "] ** [color=red]No longer ignoring empty messages[/color] **\n");
				}
				break;
			} case MENU_ID_GLOBAL_10: {
				if (!g_IgnoreDuplicateMessages) {
					g_IgnoreDuplicateMessages = true;
					ts3Functions.printMessageToCurrentTab("[" PLUGIN_NAME "] **[color=green] Now ignoring duplicate messages[/color] **\n");
				}
				else {
					g_IgnoreDuplicateMessages = false;
					ts3Functions.printMessageToCurrentTab("[" PLUGIN_NAME "] ** [color=red]No longer ignoring duplicate messages[/color] **\n");
				}
				break;
			} case MENU_ID_GLOBAL_11: {
				if (!g_IgnoreLongMessages) {
					g_IgnoreLongMessages = true;
					ts3Functions.printMessageToCurrentTab("[" PLUGIN_NAME "] ** [color=green]Now ignoring long messages[/color] **\n");
				}
				else {
					g_IgnoreLongMessages = false;
					ts3Functions.printMessageToCurrentTab("[" PLUGIN_NAME "] ** [color=red]No longer ignoring long messages[/color] **\n");
				}
				break;
			} case MENU_ID_GLOBAL_13: {
				if (!g_ConvertPokes) {
					g_ConvertPokes = true;
					ts3Functions.printMessageToCurrentTab("[" PLUGIN_NAME "] ** [color=green]Now converting pokes to messages[/color] **\n");
				}
				else {
					g_ConvertPokes = false;
					ts3Functions.printMessageToCurrentTab("[" PLUGIN_NAME "] ** [color=red]No longer converting pokes to messages[/color] **\n");
				}
				break;
			} case MENU_ID_GLOBAL_14: {
				if (!g_ConvertMessages) {
					g_ConvertMessages = true;
					ts3Functions.printMessageToCurrentTab("[" PLUGIN_NAME "] ** [color=green]Now converting messages to log lines[/color] **\n");
				}
				else {
					g_ConvertMessages = false;
					ts3Functions.printMessageToCurrentTab("[" PLUGIN_NAME "] ** [color=red]No longer converting messages to log lines[/color] **\n");
				}
				break;
			} case MENU_ID_GLOBAL_16: {
				if (!g_ShowBlockedPokes) {
					g_ShowBlockedPokes = true;
					ts3Functions.printMessageToCurrentTab("[" PLUGIN_NAME "] ** [color=green]Now showing blocked pokes[/color] **\n");
				}
				else {
					g_ShowBlockedPokes = false;
					ts3Functions.printMessageToCurrentTab("[" PLUGIN_NAME "] ** [color=red]No longer showing blocked pokes[/color] **\n");
				}
				break;
			} case MENU_ID_GLOBAL_17: {
				if (!g_ShowBlockedMessages) {
					g_ShowBlockedMessages = true;
					ts3Functions.printMessageToCurrentTab("[" PLUGIN_NAME "] ** [color=green]Now showing blocked messages[/color] **\n");
				}
				else {
					g_ShowBlockedMessages = false;
					ts3Functions.printMessageToCurrentTab("[" PLUGIN_NAME "] ** [color=red]No longer showing blocked messages[/color] **\n");
				}
				break;
			} case MENU_ID_GLOBAL_19: {
				if (g_AntiSpamEnabled == true){
					ts3Functions.printMessageToCurrentTab("[color=black][[/color]" PLUGIN_NAME "[color=black]] **[/color] [color=green]Plugin is enabled[/color] [color=black]**[/color]\n");
				}
				else {
					ts3Functions.printMessageToCurrentTab("[color=black][[/color]" PLUGIN_NAME "[color=black]] **[/color][color=red]Plugin is disabled[/color] [color=black]**[/color]\n");
				}
				if (g_IgnorePokes == true){
					ts3Functions.printMessageToCurrentTab("[color=black][[/color]" PLUGIN_NAME "[color=black]] **[/color] [color=green]Ignoring all pokes[/color] [color=black]**[/color]\n");
				}
				else {
					ts3Functions.printMessageToCurrentTab("[color=black][[/color]" PLUGIN_NAME "[color=black]] **[/color][color=red]Not ignoring all pokes[/color] [color=black]**[/color]\n");
				}
				if (g_IgnoreEmptyPokes == true){
					ts3Functions.printMessageToCurrentTab("[color=black][[/color]" PLUGIN_NAME "[color=black]] **[/color] [color=green]Ignoring empty pokes[/color] [color=black]**[/color]\n");
				}
				else {
					ts3Functions.printMessageToCurrentTab("[color=black][[/color]" PLUGIN_NAME "[color=black]] **[/color][color=red]Not ignoring empty pokes[/color] [color=black]**[/color]\n");
				}
				if (g_IgnoreDuplicatePokes == true){
					ts3Functions.printMessageToCurrentTab("[color=black][[/color]" PLUGIN_NAME "[color=black]] **[/color] [color=green]Ignoring duplicate pokes[/color] [color=black]**[/color]\n");
				}
				else {
					ts3Functions.printMessageToCurrentTab("[color=black][[/color]" PLUGIN_NAME "[color=black]] **[/color][color=red]Not ignoring duplicate pokes[/color] [color=black]**[/color]\n");
				}
				if (g_IgnoreLongPokes == true){
					ts3Functions.printMessageToCurrentTab("[color=black][[/color]" PLUGIN_NAME "[color=black]] **[/color] [color=green]Ignoring long pokes[/color] [color=black]**[/color]\n");
				}
				else {
					ts3Functions.printMessageToCurrentTab("[color=black][[/color]" PLUGIN_NAME "[color=black]] **[/color][color=red]Not ignoring long pokes[/color] [color=black]**[/color]\n");
				}
				if (g_ConvertPokes == true){
					ts3Functions.printMessageToCurrentTab("[color=black][[/color]" PLUGIN_NAME "[color=black]] **[/color] [color=green]Converting pokes to messages[/color] [color=black]**[/color]\n");
				}
				else {
					ts3Functions.printMessageToCurrentTab("[color=black][[/color]" PLUGIN_NAME "[color=black]] **[/color][color=red]Not converting pokes to messages[/color] [color=black]**[/color]\n");
				}
				if (g_ShowBlockedPokes == true){
					ts3Functions.printMessageToCurrentTab("[color=black][[/color]" PLUGIN_NAME "[color=black]] **[/color] [color=green]Showing blocked pokes[/color] [color=black]**[/color]\n");
				}
				else {
					ts3Functions.printMessageToCurrentTab("[color=black][[/color]" PLUGIN_NAME "[color=black]] **[/color][color=red]Not showing blocked pokes[/color] [color=black]**[/color]\n");
				}
				if (g_IgnoreMessages == true){
					ts3Functions.printMessageToCurrentTab("[color=black][[/color]" PLUGIN_NAME "[color=black]] **[/color] [color=green]Ignoring all messages[/color] [color=black]**[/color]\n");
				}
				else {
					ts3Functions.printMessageToCurrentTab("[color=black][[/color]" PLUGIN_NAME "[color=black]] **[/color][color=red]Not ignoring all messages[/color] [color=black]**[/color]\n");
				}
				if (g_IgnoreEmptyMessages == true){
					ts3Functions.printMessageToCurrentTab("[color=black][[/color]" PLUGIN_NAME "[color=black]] **[/color] [color=green]Ignoring empty messages[/color] [color=black]**[/color]\n");
				}
				else {
					ts3Functions.printMessageToCurrentTab("[color=black][[/color]" PLUGIN_NAME "[color=black]] **[/color][color=red]Not ignoring empty messages[/color] [color=black]**[/color]\n");
				}
				if (g_IgnoreDuplicateMessages == true){
					ts3Functions.printMessageToCurrentTab("[color=black][[/color]" PLUGIN_NAME "[color=black]] **[/color] [color=green]Ignoring duplicate messages[/color] [color=black]**[/color]\n");
				}
				else {
					ts3Functions.printMessageToCurrentTab("[color=black][[/color]" PLUGIN_NAME "[color=black]] **[/color][color=red]Not ignoring duplicate messages[/color] [color=black]**[/color]\n");
				}
				if (g_IgnoreLongMessages == true){
					ts3Functions.printMessageToCurrentTab("[color=black][[/color]" PLUGIN_NAME "[color=black]] **[/color] [color=green]Ignoring long messages[/color] [color=black]**[/color]\n");
				}
				else {
					ts3Functions.printMessageToCurrentTab("[color=black][[/color]" PLUGIN_NAME "[color=black]] **[/color][color=red]Not ignoring long messages[/color] [color=black]**[/color]\n");
				}
				if (g_ConvertMessages == true){
					ts3Functions.printMessageToCurrentTab("[color=black][[/color]" PLUGIN_NAME "[color=black]] **[/color] [color=green]Converting messages to log lines[/color] [color=black]**[/color]\n");
				}
				else {
					ts3Functions.printMessageToCurrentTab("[color=black][[/color]" PLUGIN_NAME "[color=black]] **[/color][color=red]Not converting messages to log lines[/color] [color=black]**[/color]\n");
				}
				if (g_ShowBlockedMessages == true){
					ts3Functions.printMessageToCurrentTab("[color=black][[/color]" PLUGIN_NAME "[color=black]] **[/color] [color=green]Showing blocked messages[/color] [color=black]**[/color]\n");
				}
				else {
					ts3Functions.printMessageToCurrentTab("[color=black][[/color]" PLUGIN_NAME "[color=black]] **[/color][color=red]Not showing blocked messages[/color] [color=black]**[/color]\n");
				}
					break;
			} case MENU_ID_GLOBAL_20: {
				ts3Functions.printMessageToCurrentTab("[color=black][[/color]" PLUGIN_NAME "[color=black]] **[/color] De-/Activate: Temporary blocks all other functions when off [color=black]**[/color]\n");
				ts3Functions.printMessageToCurrentTab("[color=black][[/color]" PLUGIN_NAME "[color=black]] **[/color] Ignore Pokes...: Ignores all pokes (Does not show anything about any pokes) [color=black]**[/color]\n");
				ts3Functions.printMessageToCurrentTab("[color=black][[/color]" PLUGIN_NAME "[color=black]] **[/color] ...Empty Pokes: Ignores empty pokes [color=black]**[/color]\n");
				ts3Functions.printMessageToCurrentTab("[color=black][[/color]" PLUGIN_NAME "[color=black]] **[/color] ...Duplicate Pokes: Ignores duplicate pokes [color=black]**[/color]\n");
				ts3Functions.printMessageToCurrentTab("[color=black][[/color]" PLUGIN_NAME "[color=black]] **[/color] ...Long Pokes: Ignores pokes longer then 3/4 of max [color=black]**[/color]\n");
				ts3Functions.printMessageToCurrentTab("[color=black][[/color]" PLUGIN_NAME "[color=black]] **[/color] Ignore messages...: Ignores all messages (Does not open any new chat window) [color=black]**[/color]\n");
				ts3Functions.printMessageToCurrentTab("[color=black][[/color]" PLUGIN_NAME "[color=black]] **[/color] ...Empty messages: Ingores empty chat messages [color=black]**[/color]\n");
				ts3Functions.printMessageToCurrentTab("[color=black][[/color]" PLUGIN_NAME "[color=black]] **[/color] ...Duplicate messages: Ingores duplicate chat messages [color=black]**[/color]\n");
				ts3Functions.printMessageToCurrentTab("[color=black][[/color]" PLUGIN_NAME "[color=black]] **[/color] ...Long messages: Ignores messages longer then 3/4 of max [color=black]**[/color]\n");
				ts3Functions.printMessageToCurrentTab("[color=black][[/color]" PLUGIN_NAME "[color=black]] **[/color] Convert Pokes: Converts pokes to chat messages [color=black]**[/color]\n");
				ts3Functions.printMessageToCurrentTab("[color=black][[/color]" PLUGIN_NAME "[color=black]] **[/color] Convert Messages: Converts chat to log messages [color=black]**[/color]\n");
				ts3Functions.printMessageToCurrentTab("[color=black][[/color]" PLUGIN_NAME "[color=black]] **[/color] Show blocked Pokes: Temporary shows pokes that normally would be ignored by the client [color=black]**[/color]\n");
				ts3Functions.printMessageToCurrentTab("[color=black][[/color]" PLUGIN_NAME "[color=black]] **[/color] Show blocked Messages: Temporary shows chat messages that normally would be ignored by the client [color=black]**[/color]\n");
				break;
			} case MENU_ID_GLOBAL_21: {
				MessageBoxA(0, PLUGIN_NAME " v" PLUGIN_VERSION " developed by " PLUGIN_AUTHOR " (" PLUGIN_CONTACT ")", "About " PLUGIN_NAME, MB_ICONINFORMATION);
				break;
			} default: {
					break;
			}
		}
	}
}