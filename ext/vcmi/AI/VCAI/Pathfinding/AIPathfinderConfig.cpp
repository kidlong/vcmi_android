/*
* AIhelper.h, part of VCMI engine
*
* Authors: listed in file AUTHORS in main folder
*
* License: GNU General Public License v2.0 or later
* Full text of license available in license.txt file, in main folder
*
*/
#include "StdInc.h"
#include "AIPathfinderConfig.h"
#include "../../../CCallback.h"

class AIMovementAfterDestinationRule : public MovementAfterDestinationRule
{
private:
	CPlayerSpecificInfoCallback * cb;
	std::shared_ptr<AINodeStorage> nodeStorage;

public:
	AIMovementAfterDestinationRule(CPlayerSpecificInfoCallback * cb, std::shared_ptr<AINodeStorage> nodeStorage)
		:cb(cb), nodeStorage(nodeStorage)
	{
	}

	virtual void process(
		const PathNodeInfo & source,
		CDestinationNodeInfo & destination,
		const PathfinderConfig * pathfinderConfig,
		CPathfinderHelper * pathfinderHelper) const override
	{
		if(nodeStorage->hasBetterChain(source, destination))
		{
			destination.blocked = true;

			return;
		}

		auto blocker = getBlockingReason(source, destination, pathfinderConfig, pathfinderHelper);

		if(blocker == BlockingReason::NONE)
			return;

		if(blocker == BlockingReason::DESTINATION_BLOCKVIS && destination.nodeObject)
		{
			auto objID = destination.nodeObject->ID;
			if((objID == Obj::HERO && destination.objectRelations != PlayerRelations::ENEMIES)
				|| objID == Obj::SUBTERRANEAN_GATE || objID == Obj::MONOLITH_TWO_WAY
				|| objID == Obj::MONOLITH_ONE_WAY_ENTRANCE || objID == Obj::MONOLITH_ONE_WAY_EXIT
				|| objID == Obj::WHIRLPOOL)
			{
				destination.blocked = true;
			}

			return;
		}

		if(blocker == BlockingReason::DESTINATION_VISIT)
		{
			return;
		}

		if(blocker == BlockingReason::DESTINATION_GUARDED)
		{
			auto srcGuardians = cb->getGuardingCreatures(source.coord);
			auto destGuardians = cb->getGuardingCreatures(destination.coord);

			if(destGuardians.empty())
			{
				destination.blocked = true;

				return;
			}

			vstd::erase_if(destGuardians, [&](const CGObjectInstance * destGuard) -> bool
			{
				return vstd::contains(srcGuardians, destGuard);
			});

			auto guardsAlreadyBypassed = destGuardians.empty() && srcGuardians.size();
			if(guardsAlreadyBypassed && nodeStorage->isBattleNode(source.node))
			{
				logAi->trace(
					"Bypass guard at destination while moving %s -> %s",
					source.coord.toString(),
					destination.coord.toString());

				return;
			}

			auto destNode = nodeStorage->getAINode(destination.node);
			auto battleNode = nodeStorage->getNode(destination.coord, destination.node->layer, destNode->chainMask | AINodeStorage::BATTLE_CHAIN);

			if(battleNode->locked)
			{
				logAi->trace(
					"Block bypass guard at destination while moving %s -> %s",
					source.coord.toString(),
					destination.coord.toString());

				destination.blocked = true;

				return;
			}

			auto hero = nodeStorage->getHero();
			auto danger = evaluateDanger(destination.coord, hero);

			destination.node = battleNode;
			nodeStorage->commit(destination, source);

			if(battleNode->danger < danger)
			{
				battleNode->danger = danger;
			}

			logAi->trace(
				"Begin bypass guard at destination with danger %s while moving %s -> %s",
				std::to_string(danger),
				source.coord.toString(),
				destination.coord.toString());

			return;
		}

		destination.blocked = true;
	}
};

class AIMovementToDestinationRule : public MovementToDestinationRule
{
private:
	CPlayerSpecificInfoCallback * cb;
	std::shared_ptr<AINodeStorage> nodeStorage;

public:
	AIMovementToDestinationRule(CPlayerSpecificInfoCallback * cb, std::shared_ptr<AINodeStorage> nodeStorage)
		:cb(cb), nodeStorage(nodeStorage)
	{
	}

	virtual void process(
		const PathNodeInfo & source,
		CDestinationNodeInfo & destination,
		const PathfinderConfig * pathfinderConfig,
		CPathfinderHelper * pathfinderHelper) const override
	{
		auto blocker = getBlockingReason(source, destination, pathfinderConfig, pathfinderHelper);

		if(blocker == BlockingReason::NONE)
			return;

		if(blocker == BlockingReason::SOURCE_GUARDED && nodeStorage->isBattleNode(source.node))
		{
			auto srcGuardians = cb->getGuardingCreatures(source.coord);
			auto destGuardians = cb->getGuardingCreatures(destination.coord);

			for(auto srcGuard : srcGuardians)
			{
				if(!vstd::contains(destGuardians, srcGuard))
					continue;

				auto guardPos = srcGuard->visitablePos();
				if(guardPos != source.coord && guardPos != destination.coord)
				{
					destination.blocked = true; // allow to pass monster only through guard tile
				}
			}

			if(!destination.blocked)
			{
				logAi->trace(
					"Bypass src guard while moving from %s to %s",
					source.coord.toString(),
					destination.coord.toString());
			}

			return;
		}

		destination.blocked = true;
	}
};

class AIPreviousNodeRule : public MovementToDestinationRule
{
private:
	CPlayerSpecificInfoCallback * cb;
	std::shared_ptr<AINodeStorage> nodeStorage;

public:
	AIPreviousNodeRule(CPlayerSpecificInfoCallback * cb, std::shared_ptr<AINodeStorage> nodeStorage)
		:cb(cb), nodeStorage(nodeStorage)
	{
	}

	virtual void process(
		const PathNodeInfo & source,
		CDestinationNodeInfo & destination,
		const PathfinderConfig * pathfinderConfig,
		CPathfinderHelper * pathfinderHelper) const override
	{
//FIXME: unused
//		auto blocker = getBlockingReason(source, destination, pathfinderConfig, pathfinderHelper);

		if(source.guarded)
		{
			auto srcGuardian = cb->guardingCreaturePosition(source.node->coord);

			if(srcGuardian == source.node->coord)
			{
				// guardian tile is used as chain junction
				destination.node->theNodeBefore = source.node;

				logAi->trace(
					"Link src node %s to destination node %s while bypassing guard",
					source.coord.toString(),
					destination.coord.toString());
			}
		}

		if(source.node->action == CGPathNode::ENodeAction::BLOCKING_VISIT || source.node->action == CGPathNode::ENodeAction::VISIT)
		{
			// we can not directly bypass objects, we need to interact with them first
			destination.node->theNodeBefore = source.node;

			logAi->trace(
				"Link src node %s to destination node %s while bypassing visitable obj",
				source.coord.toString(),
				destination.coord.toString());
		}
	}
};

std::vector<std::shared_ptr<IPathfindingRule>> makeRuleset(
	CPlayerSpecificInfoCallback * cb,
	std::shared_ptr<AINodeStorage> nodeStorage)
{
	std::vector<std::shared_ptr<IPathfindingRule>> rules = {
		std::make_shared<LayerTransitionRule>(),
		std::make_shared<DestinationActionRule>(),
		std::make_shared<AIMovementToDestinationRule>(cb, nodeStorage),
		std::make_shared<MovementCostRule>(),
		std::make_shared<AIPreviousNodeRule>(cb, nodeStorage),
		std::make_shared<AIMovementAfterDestinationRule>(cb, nodeStorage)
	};

	return rules;
}

AIPathfinderConfig::AIPathfinderConfig(
	CPlayerSpecificInfoCallback * cb,
	std::shared_ptr<AINodeStorage> nodeStorage)
	:PathfinderConfig(nodeStorage, makeRuleset(cb, nodeStorage))
{
}
