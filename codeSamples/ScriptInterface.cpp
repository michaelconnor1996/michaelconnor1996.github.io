#include "ScriptInterface.h"
#include "cocos2d.h"


ScriptInterface::ScriptInterface() {
	state = luaL_newstate();
}


bool ScriptInterface::addScript(const std::string& scriptName) {
	if (luaL_dofile(state, scriptName.c_str()) == 0) {
		scriptNames.push_back(scriptName);
		return true;
	}
	
	std::string errorMessage = "Script with filename " + scriptName + " is not a valid lua file or does not exist\n";
	cocos2d::log(errorMessage.c_str());
	return false;
}

void ScriptInterface::update() {
	for (const auto& script : scriptNames) {
		luaL_dofile(state, script.c_str());
	}
}

luabind::object ScriptInterface::retrieveObject(const std::string& tableName, const std::string& entryName, const std::string& parameter) {
	luabind::object table = luabind::globals(state)[tableName];
	if (luabind::type(table) == LUA_TTABLE) {
		luabind::object entry = table[entryName];
		if (luabind::type(entry) == LUA_TTABLE) {
			return entry[parameter];
		}
	}
	std::string errorMessage = "Failed to retrieve parameter from script";
	errorMessage += ", tablename: " + tableName + ", entryname : " + entryName + ", parameter : " + parameter + "\n";
	cocos2d::log(errorMessage.c_str());
	
	
	return luabind::object();
}
