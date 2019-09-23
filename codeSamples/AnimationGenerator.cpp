#include "AnimationGenerator.h"
#include "cocos2d.h"
USING_NS_CC;

Animation* createAnimation(const std::string& baseName, const std::string& postfix, int numberOfFrames, float delay, bool reverse) {
	auto cache = SpriteFrameCache::getInstance();
	Animation* animation = Animation::create();
	animation->setDelayPerUnit(delay);
	int start, end, direction;
	if (reverse) {
		start = numberOfFrames - 1;
		end = -1;
		direction = -1;
	}
	else {
		start = 0;
		end = numberOfFrames;
		direction = 1;
	}

	std::string name;
	for (int i = start; i != end; i += direction) {
		name = baseName + std::to_string(i) + postfix;
		auto frame = cache->getSpriteFrameByName(name);
		if (!frame) {
			cocos2d::log("Couldn't retrieve sprite frame with name: %s", name.c_str());
		}
		animation->addSpriteFrame(frame);
	}
	return animation;
}

Animate* createAnimationAction(const std::string& baseName, const std::string& postfix, int numberOfFrames, float delay, bool reverse) {
	auto animation = createAnimation(baseName, postfix, numberOfFrames, delay, reverse);
	return Animate::create(animation);

}

cocos2d::Sprite* createAnimatedSprite(const std::string& baseName, const std::string& postfix, int numberOfFrames, float delay, bool reverse) {
	auto animationAction = createAnimationAction(baseName, postfix, numberOfFrames, delay, reverse);
	auto frame = animationAction->getAnimation()->getFrames().front()->getSpriteFrame();
	auto baseSprite = Sprite::createWithSpriteFrame(frame);
	baseSprite->runAction(animationAction);
	return baseSprite;
}
