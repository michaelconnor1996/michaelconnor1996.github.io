#include "BattlefieldProvider.h"
#include "BattleField.h"
#include "BattleTile.h"
#include "Actor.h"
#include "Megaman.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <string>
#include <exception>
#include <stdexcept>

using namespace std;
using namespace tinyxml2;
using namespace cocos2d;

inline static bool isGoodTier(XMLElement* map, int minimalTier) {
	int tier;
	return (map && map->QueryAttribute("tier", &tier) == XML_SUCCESS && tier >= minimalTier);
}


BattlefieldProvider::BattlefieldProvider(std::string _standardBackground, std::string _standardMusic) 
	: previousID(""), standardBackground(_standardBackground), standardMusic(_standardMusic), mapRoot(nullptr) {

}



bool BattlefieldProvider::initializeMaps(const char* mapsFilename) {
	FileUtils *fileUtils = FileUtils::getInstance(); 
	auto filename = fileUtils->fullPathForFilename("maps.xml");
	XMLError error = mapDocument.LoadFile(filename.c_str());
	if (error != XML_SUCCESS) {
		cocos2d::log("Failed to load map document\n");
		return false;
	}

	mapRoot = mapDocument.FirstChildElement("maps");

	return (mapRoot != nullptr);
}


BattleField* BattlefieldProvider::getBattlefield(Area area, MegamanData* data, int minimalTier) const {
	BattleField* battlefield = nullptr;

	XMLElement* map = chooseBattlefield(area, minimalTier);
	if (map) {
		battlefield = parseBattlefield(map, area);
		XMLElement* objectNode = map->FirstChildElement("objects");
		placeObjectsOnBattlefield(battlefield, data, objectNode);
	}

	return battlefield;
}


tinyxml2::XMLElement* BattlefieldProvider::chooseBattlefield(Area area, int minimalTier) const {
	XMLElement* areaContainer = findArea(area);

	XMLElement* map = areaContainer->FirstChildElement("map");
	int totalWeight = gatherTotalWeightOfMaps(map, minimalTier);


	//Choose the first valid map in the area, if none exist then nullptr is returned.
	//This is good because an effect might be active that prevents weak/easy battles.
	XMLElement* chosenMap = areaContainer->FirstChildElement("map");
	while (chosenMap && !isGoodTier(chosenMap, minimalTier)) 
		chosenMap = chosenMap->NextSiblingElement("map");
	if (!chosenMap)
		return nullptr;

	//Pick a random valid map
	int random = cocos2d::RandomHelper::random_int(0, totalWeight);
	map = chosenMap->NextSiblingElement("map");
	while (map && random >= 0) {
		if (isGoodTier(map, minimalTier)) {
			random -= map->IntAttribute("weight");
			if (random < 0) {
				chosenMap = map;
			}
		}

		map = map->NextSiblingElement("map");
	}
	
	return chosenMap;
}

int BattlefieldProvider::gatherTotalWeightOfMaps(XMLElement* map, int minimalTier) const {
	int totalWeight = 0;
	while (map) {
		if (isGoodTier(map, minimalTier)) {
			totalWeight += map->IntAttribute("weight");
			map = map->NextSiblingElement("map");
		}
	}
	return totalWeight;
}

tinyxml2::XMLElement* BattlefieldProvider::findArea(Area area) const {
	std::string areaString = toString(area);
	XMLElement* areaContainer = mapRoot->FirstChildElement("area");
	while (areaContainer && areaContainer->Attribute("name") != areaString) {
		areaContainer = areaContainer->NextSiblingElement("area");
	}
	if (!areaContainer) {
		if (area == Area::GENERAL) {
			throw(std::runtime_error("General area not found in maps file"));
		}
		std::string errorMessage = "Area not found in maps file: " + toString(area) + "\n";
		cocos2d::log(errorMessage.c_str());
		areaContainer = findArea(Area::GENERAL);
	}

	return areaContainer;
}


BattleField* BattlefieldProvider::parseBattlefield(tinyxml2::XMLElement* map, Area area) const {
	if (!map)
		return nullptr;
	const char* id = map->Attribute("id");
	if (!id)
		throw(std::runtime_error("Map doesn't have an ID"));
	previousID = id;
	
	BattleFieldFactory* battleFieldFactory = new BattleFieldFactory();
	XMLElement* tiles = map->FirstChildElement("tiles");
	if (!tiles) {
		std::string errorMessage = "Map doesn't have tiles section: " + previousID;
		delete battleFieldFactory;
		throw(std::runtime_error(errorMessage.c_str()));
	}

	XMLElement* row = tiles->FirstChildElement("frontRow");
	if (row)
		parseRow(row, RowPosition::FRONT, battleFieldFactory);
	row = tiles->FirstChildElement("middleRow");
	if (row)
		parseRow(row, RowPosition::MID, battleFieldFactory);
	row = tiles->FirstChildElement("backRow");
	if (row)
		parseRow(row, RowPosition::BACK, battleFieldFactory);

	BattleField* battleField = battleFieldFactory->getBattleField();
	delete battleFieldFactory;
	if (!battleField) {
		std::string errorMessage = "Map is missing some tiles: " + previousID;
		throw(std::runtime_error(errorMessage.c_str()));
	}

	XMLElement* element = map->FirstChildElement("background");
	if (element)
		battleField->setBackgroundName(element->GetText());
	else
		battleField->setBackgroundName(standardBackground);

	element = map->FirstChildElement("music");
	if (element)
		battleField->setMusicName(element->GetText());
	else
		battleField->setMusicName(standardMusic);
	battleField->setArea(area);
	return battleField;
}

void BattlefieldProvider::parseRow(tinyxml2::XMLElement* row, RowPosition rowPosition, BattleFieldFactory* factory) const {
	XMLElement* tile = row->FirstChildElement("tile");
	XMLElement* ownerNode;
	XMLElement* typeNode;
	int x = 0, y = 0;
	if (rowPosition == RowPosition::MID)
		y = 1;
	else if (rowPosition == RowPosition::BACK)
		y = 2;

	Owner owner;
	TileType type;
	while (tile) {
		ownerNode = tile->FirstChildElement("owner");
		typeNode = tile->FirstChildElement("type");
		if (!ownerNode || !typeNode)
			throw(std::runtime_error("Tile is missing owner or type"));

		owner = ownerFromString(ownerNode->GetText());
		type = typeFromString(typeNode->GetText());

		BattleTile battleTile(Coordinate(x, y), owner, type);
		factory->supplyNextTile(battleTile);

		++x;
		tile = tile->NextSiblingElement("tile");
	}

}

void BattlefieldProvider::placeObjectsOnBattlefield(BattleField* battlefield, MegamanData* data, tinyxml2::XMLElement* objectNode) const {
	if (!battlefield || !objectNode || !data)
		return;

	EnemyType type;
	XMLElement* nameNode = nullptr;
	XMLElement* coordinateNode = nullptr;
	XMLElement* object = objectNode->FirstChildElement("object");
	int x, y;
	Coordinate position;
	Owner owner;
	while (object) {
		nameNode = object->FirstChildElement("name");
		type = enemyTypeFromString(nameNode->GetText());
		
		coordinateNode = object->FirstChildElement("x");
		coordinateNode->QueryIntText(&x);
		coordinateNode = object->FirstChildElement("y");
		coordinateNode->QueryIntText(&y);
		position = Coordinate(x, y);
		
		auto tile = battlefield->getTileAtCoordinate(position);
		owner = tile->getOwner();

		if (type == EnemyType::MEGAMAN) {
			new Megaman(data, battlefield, position, owner);
		}
		else {
			Actor::createActor(type, battlefield, position, owner);
		}

		object = object->NextSiblingElement("object");
	}
}

