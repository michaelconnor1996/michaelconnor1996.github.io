#ifndef SCRIPT_INTERFACE_H
#define SCRIPT_INTERFACE_H

#include <iostream>
#include <string>
#include "luaHeader.h"

/* Maintains a lua state which contains tables with parameters */
class ScriptInterface {
public:
	ScriptInterface();

	bool addScript(const std::string& scriptName);

	void update();

	template<class T>
	T retrieveParameter(const std::string& tableName, const std::string& entryName, const std::string& parameter);

private:
	lua_State* state;
	std::vector<std::string> scriptNames;

	luabind::object retrieveObject(const std::string& tableName, const std::string& entryName, const std::string& parameter);
};



template<class T>
T ScriptInterface::retrieveParameter(const std::string& tableName, const std::string& entryName, const std::string& parameter) {
	return luabind::object_cast<T>(retrieveObject(tableName, entryName, parameter));
}

#endif
