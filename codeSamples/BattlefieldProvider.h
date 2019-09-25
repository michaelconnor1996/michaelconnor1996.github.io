#ifndef BATTLEFIELD_PROVIDER_H
#define BATTLEFIELD_PROVIDER_H

#include <map>
#include <unordered_map>
#include <vector>
#include "CommonDefinitions.h"
#include "tinyxml2.h"
class BattleField;
class BattleFieldFactory;
class MegamanData;

/* Persistent object that provides an appropriate battlefield map each time it is called */
class BattlefieldProvider {
public:
	BattlefieldProvider(std::string standardBackground, std::string standardMusic);
	bool initializeMaps(const char* mapsFilename);

	BattleField* getBattlefield(Area area, MegamanData* data, int minimalTier = 0) const;

private:
	mutable std::string previousID;
	tinyxml2::XMLDocument mapDocument;
	tinyxml2::XMLNode* mapRoot;
	std::string standardBackground;
	std::string standardMusic;

	tinyxml2::XMLElement* chooseBattlefield(Area area, int minimalTier) const;
	int gatherTotalWeightOfMaps(tinyxml2::XMLElement* map, int minimalTier) const;
	tinyxml2::XMLElement* findArea(Area area) const;
	BattleField* parseBattlefield(tinyxml2::XMLElement* map) const;
	void parseRow(tinyxml2::XMLElement* row, RowPosition rowPosition, BattleFieldFactory& factory) const;
	void placeObjectsOnBattlefield(BattleField* battlefield, MegamanData* data, tinyxml2::XMLElement* objectNode) const; //Pass megaman data to place on battlefield
};



#endif