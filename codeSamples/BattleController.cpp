#include "BattleController.h"
#include "HumanController.h"
#include "BattlefieldProvider.h"
#include "CommonDefinitions.h"
#include "GameEventRelay.h"
#include "BattleScene.h"
#include "CustomScreen.h"
#include "CustomTimer.h"
#include "BattleField.h"
#include "Actor.h"
#include "GameObject.h"
#include "Projectile.h"
#include "Megaman.h"
#include "Chip.h"
#include "FontManager.h"
#include <map>
USING_NS_CC;

namespace {
	bool isProjectile(GameObject* object) {
		auto projectile = dynamic_cast<Projectile*>(object);
		return projectile;
	}
}

static const std::map<EventKeyboard::KeyCode, Command> bindings = {
	{ EventKeyboard::KeyCode::KEY_W, Command::UP },
	{ EventKeyboard::KeyCode::KEY_A, Command::LEFT },
	{ EventKeyboard::KeyCode::KEY_S, Command::DOWN },
	{ EventKeyboard::KeyCode::KEY_D, Command::RIGHT },
	{ EventKeyboard::KeyCode::KEY_SPACE, Command::USE },
	{ EventKeyboard::KeyCode::KEY_ALT, Command::BACK },
	{ EventKeyboard::KeyCode::KEY_Q, Command::TRIGGER_LEFT },
	{ EventKeyboard::KeyCode::KEY_E, Command::TRIGGER_RIGHT },
	{ EventKeyboard::KeyCode::KEY_P, Command::START },
	{ EventKeyboard::KeyCode::KEY_MINUS, Command::SLOW },
	{ EventKeyboard::KeyCode::KEY_EQUAL, Command::FAST },
};

BattleController::BattleController(BattleField* _battlefield) //TODO: Set paused to true when cust is activated at start
	: rewardSystem(new RewardSystem()), battleField(_battlefield), customScreen(nullptr), listener(EventListenerKeyboard::create()), paused(false), 
		customTimer(new CustomTimer()), gameIsActive(true), rewardScreen(nullptr), scheduler(battleField->getScheduler()), megaman(nullptr) {

	using GE = GameEvent;
	auto events = { 
		GE::ENTITY_DIED, GE::GAME_OVER, GE::GAME_WON, GE::PAUSE, GE::RESUME, GE::SPAWN, GE::CUST_DONE, GE::CHIP_USED, GE::REQUEST_REMOVAL, GE::MEGAMAN_HIT 
	};
	GameEventRelay::instance().addListener(this, events);

	retrieveObjectsFromBattlefield();
	scheduleObjectUpdates();

	battleScene = BattleScene::create();
	battleScene->addChild(battleField, 0, "battlefield");
	battleScene->addChild(customTimer);
	battleField->setPosition(Vec2(250, 100));

	rewardSystem->initializeRewards("rewards.xml");

	nextChipLabel = FontManager::getInstance()->getHealthBarLabel("");
	nextChipLabel->setAnchorPoint(Vec2(0, 0));
	nextChipLabel->setPosition(Vec2(10, 10));
	battleScene->addChild(nextChipLabel);

	healthBox = Sprite::createWithSpriteFrameName("Box.png");
	healthBox->setAnchorPoint(Vec2(0, 1));
	healthBox->setPosition(Vec2(20, Director::getInstance()->getVisibleSize().height));
	battleScene->addChild(healthBox);

	healthLabel = FontManager::getInstance()->getHealthBarLabel(std::to_string(megaman->getHealth()));
	healthLabel->setPositionNormalized(Vec2(0.7f, 0.5f));
	healthBox->addChild(healthLabel);
}

BattleController::~BattleController() {
	GameEventRelay::instance().removeListener(this);
	battleField->getEventDispatcher()->removeEventListener(listener);
	scheduler->unscheduleAllForTarget(this);
	unscheduleObjectUpdates();

	delete customScreen;
	delete customTimer;
	delete battleField;
	delete rewardScreen;
}

void BattleController::runBattle() {
	resumeObjectUpdates();

	megaman->setDispatcher(battleField->getEventDispatcher());
	const auto& chips = megaman->getData()->getChips();
	customScreen = new CustomScreen(battleField->getEventDispatcher(), chips);
	battleScene->addChild(customScreen);
	customScreen->setVisible(false);
	listener->onKeyPressed = [this](cocos2d::EventKeyboard::KeyCode keyCode, cocos2d::Event* event) {
		if (!customScreen->isActive()) {
			Command command = createCommand(keyCode, bindings);
			switch (command) {
			case Command::TRIGGER_LEFT:
			case Command::TRIGGER_RIGHT:
				if (customTimer->isReady()) {
					customTimer->setVisible(false);
					customScreen->enterScreen();
					customTimer->reset();
				}
				break;
			case Command::START:
				if (paused)
					GameEventRelay::instance().sendResumeEvent();
				else
					GameEventRelay::instance().sendPauseEvent();				
				break;
			case Command::SLOW:
				cocos2d::Director::getInstance()->setAnimationInterval(cocos2d::Director::getInstance()->getAnimationInterval() * 2.0f); //FIXME: NOTE: remove these two later, only for debugging
				break;
			case Command::FAST:
				cocos2d::Director::getInstance()->setAnimationInterval(cocos2d::Director::getInstance()->getAnimationInterval() / 2.0f);
				break;
			default:
				break;
			}
		}
	};
	listener->onKeyReleased = [](cocos2d::EventKeyboard::KeyCode keyCode, cocos2d::Event* event) {

	};
	
	battleField->getEventDispatcher()->addEventListenerWithFixedPriority(listener, 3);

	scheduler->scheduleUpdate(rewardSystem, 4, false);

	auto director = cocos2d::Director::getInstance();
	director->pushScene(battleScene);
}


void BattleController::interpretDiedEvent(GameObject* subject) {
	//Prevents double removal errors when damage is done twice to a subject in a single frame
	if (deadObjects.find(subject) == deadObjects.end() && objects.find(subject) != objects.end()) {
		deadObjects.insert(subject);

		if (gameIsActive && !isProjectile(subject)) {
			rewardSystem->interpretDiedEvent(subject);

			int friendlies = 0;
			int enemies = 0;

			for (const auto& object : objects) {
				if (dynamic_cast<Actor*>(object) && !deadObjects.count(object)) {
					if (object->getOwner() == Owner::FRIENDLY)
						++friendlies;
					else
						++enemies;
				}
			}

			if (friendlies == 0 || enemies == 0) {
				cocos2d::CallFunc* endFunc;
				if (friendlies == 0) {
					endFunc = CallFunc::create(std::bind(&GameEventRelay::sendGameOverEvent, GameEventRelay::instance()));
				}
				else if (enemies == 0) {
					endFunc = CallFunc::create(std::bind(&GameEventRelay::sendGameWonEvent, GameEventRelay::instance()));
					if (megaman)
						megaman->getSprite()->stopAllActions();
				}
				
				auto delay = DelayTime::create(0.69f);
				battleScene->runAction(Sequence::create(delay, endFunc, nullptr));
				GameEventRelay::instance().removeAllListenersForEvent(GameEvent::DAMAGE);
				rewardSystem->setActive(false);
				gameIsActive = false;
			}
		}
	}
}

void BattleController::interpretRequestRemovalEvent(GameObject* subject) {
	if (objects.find(subject) != objects.end()) {
		scheduler->unscheduleUpdate(subject);
		GameEventRelay::instance().removeListener(subject);
		battleField->removeObject(subject);
		objects.erase(subject);
		deadObjects.erase(subject);
		if (dynamic_cast<Megaman*>(subject))
			megaman = nullptr;
	}
}


void BattleController::interpretGameOverEvent() {
	GameEventRelay::instance().sendPauseEvent();

	results.victory = false;

	auto director = cocos2d::Director::getInstance();
	director->popScene();
}

void BattleController::interpretGameWonEvent() {
	float elapsedSeconds = rewardSystem->getDeleteTime();
	results.deleteTime = elapsedSeconds;
	int rating = rewardSystem->getRating();
	results.rewardTuple = rewardSystem->getReward(battleField->getArea(), enemyTypes);
	results.victory = true;

	float elapsedMilliseconds = elapsedSeconds * 1000;
	rewardScreen = new RewardScreen(results.rewardTuple, elapsedMilliseconds, rating);
	battleScene->addChild(rewardScreen->getScreenLayer());
}

void BattleController::interpretPauseEvent() {
	paused = true;
	pauseObjectUpdates();
	if (megaman)
		megaman->setPaused(true);
}

void BattleController::interpretResumeEvent() {
	paused = false;
	resumeObjectUpdates();
	if (megaman)
		megaman->setPaused(false);
}

void BattleController::interpretSpawnEvent(GameObject* subject, Coordinate position) {
	objects.insert(subject);
	scheduler->scheduleUpdate(subject, 5, false);
}

void BattleController::interpretCustDoneEvent() {
	customTimer->setVisible(true);
	auto chips = customScreen->getSelectedChips();
	megaman->setSelectedChips(chips);
	updateNextChipLabel();
}

void BattleController::interpretChipUsedEvent(Chip* usedChip) {
	updateNextChipLabel();
}

void BattleController::interpretMegamanHitEvent() {
	healthLabel->setString(std::to_string(megaman->getHealth()));
}


void BattleController::retrieveObjectsFromBattlefield() {
	for (int x = 0; x < FIELD_WIDTH; ++x) {
		for (int y = 0; y < FIELD_HEIGHT; ++y) {
			auto tile = battleField->getTileAtCoordinate(Coordinate(x, y));
			auto contents = tile->getContents();
			for (const auto& object : contents) {
				objects.insert(object);
				Actor* actor = dynamic_cast<Actor*>(object);
				if (actor && object->getOwner() == Owner::ENEMY)
					enemyTypes.insert(actor->getType());
				Megaman* mega = dynamic_cast<Megaman*>(actor);
				if (mega)
					megaman = mega;
			}
		}
	}
}

void BattleController::scheduleObjectUpdates() {
	for (const auto& object : objects) {
		scheduler->scheduleUpdate(object, 5, true);
	}
}

void BattleController::unscheduleObjectUpdates() {
	for (const auto& object : objects) {
		scheduler->unscheduleUpdate(object);
	}
}

void BattleController::resumeObjectUpdates() {
	battleField->resume();
	for (const auto& object : objects) {
		scheduler->resumeTarget(object);
		object->getSprite()->resume();
	}
}

void BattleController::pauseObjectUpdates() {
	battleField->pause();
	for (const auto& object : objects) {
		scheduler->pauseTarget(object);
		object->getSprite()->pause();
	}
}

void BattleController::updateNextChipLabel() {
	auto chips = megaman->getSelectedChips();
	if (!chips.empty()) {
		auto next = chips.front();
		auto newString = next->getNameString();
		if (next->getDamage())
			newString += "  " + std::to_string(next->getDamage());
		nextChipLabel->setString(newString);
	}
	else {
		nextChipLabel->setString("");
	}
}







