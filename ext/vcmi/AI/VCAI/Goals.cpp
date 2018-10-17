/*
 * Goals.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "StdInc.h"
#include "Goals.h"
#include "VCAI.h"
#include "FuzzyHelper.h"
#include "ResourceManager.h"
#include "BuildingManager.h"
#include "../../lib/mapping/CMap.h" //for victory conditions
#include "../../lib/CPathfinder.h"
#include "../../lib/StringConstants.h"

#include "AIhelper.h"

extern boost::thread_specific_ptr<CCallback> cb;
extern boost::thread_specific_ptr<VCAI> ai;
extern FuzzyHelper * fh;

using namespace Goals;

TSubgoal Goals::sptr(const AbstractGoal & tmp)
{
	TSubgoal ptr;
	ptr.reset(tmp.clone());
	return ptr;
}

std::string Goals::AbstractGoal::name() const //TODO: virtualize
{
	std::string desc;
	switch(goalType)
	{
	case INVALID:
		return "INVALID";
	case WIN:
		return "WIN";
	case DO_NOT_LOSE:
		return "DO NOT LOOSE";
	case CONQUER:
		return "CONQUER";
	case BUILD:
		return "BUILD";
	case EXPLORE:
		desc = "EXPLORE";
		break;
	case GATHER_ARMY:
		desc = "GATHER ARMY";
		break;
	case BUY_ARMY:
		return "BUY ARMY";
		break;
	case BOOST_HERO:
		desc = "BOOST_HERO (unsupported)";
		break;
	case RECRUIT_HERO:
		return "RECRUIT HERO";
	case BUILD_STRUCTURE:
		return "BUILD STRUCTURE";
	case COLLECT_RES:
		desc = "COLLECT RESOURCE " + GameConstants::RESOURCE_NAMES[resID] + " (" + boost::lexical_cast<std::string>(value) + ")";
		break;
	case TRADE:
	{
		auto obj = cb->getObjInstance(ObjectInstanceID(objid));
		if (obj)
			desc = (boost::format("TRADE %d of %s at %s") % value % GameConstants::RESOURCE_NAMES[resID] % obj->getObjectName()).str();
	}
	break;
	case GATHER_TROOPS:
		desc = "GATHER TROOPS";
		break;
	case VISIT_OBJ:
	{
		auto obj = cb->getObjInstance(ObjectInstanceID(objid));
		if(obj)
			desc = "GET OBJ " + obj->getObjectName();
	}
	break;
	case FIND_OBJ:
		desc = "FIND OBJ " + boost::lexical_cast<std::string>(objid);
		break;
	case VISIT_HERO:
	{
		auto obj = cb->getObjInstance(ObjectInstanceID(objid));
		if(obj)
			desc = "VISIT HERO " + obj->getObjectName();
	}
	break;
	case GET_ART_TYPE:
		desc = "GET ARTIFACT OF TYPE " + VLC->arth->artifacts[aid]->Name();
		break;
	case ISSUE_COMMAND:
		return "ISSUE COMMAND (unsupported)";
	case VISIT_TILE:
		desc = "VISIT TILE " + tile.toString();
		break;
	case CLEAR_WAY_TO:
		desc = "CLEAR WAY TO " + tile.toString();
		break;
	case DIG_AT_TILE:
		desc = "DIG AT TILE " + tile.toString();
		break;
	default:
		return boost::lexical_cast<std::string>(goalType);
	}
	if(hero.get(true)) //FIXME: used to crash when we lost hero and failed goal
		desc += " (" + hero->name + ")";
	return desc;
}

bool Goals::AbstractGoal::operator==(AbstractGoal & g)
{
	/*this operator checks if goals are EQUIVALENT, ie. if they represent same objective
	it does not not check isAbstract or isElementar, as this is up to VCAI decomposition logic
	*/
	if(g.goalType != goalType)
		return false;

	switch(goalType)
	{
	//no parameters
	case INVALID:
	case WIN:
	case DO_NOT_LOSE:
	case RECRUIT_HERO: //overloaded
		return true;
		break;

	//assigned to hero, no parameters
	case CONQUER:
	case EXPLORE:
	case BOOST_HERO:
		return g.hero.h == hero.h; //how comes HeroPtrs are equal for different heroes?
		break;

	case GATHER_ARMY: //actual value is indifferent
		return (g.hero.h == hero.h || town == g.town); //TODO: gather army for town maybe?
		break;

	//assigned hero and tile
	case VISIT_TILE:
	case CLEAR_WAY_TO:
	case DIG_AT_TILE:
		return (g.hero.h == hero.h && g.tile == tile);
		break;

	//assigned hero and object
	case VISIT_OBJ:
	case FIND_OBJ: //TODO: use subtype?
	case VISIT_HERO:
	case GET_ART_TYPE:
		return (g.hero.h == hero.h && g.objid == objid);
		break;

	case BUILD_STRUCTURE:
		return (town == g.town && bid == g.bid); //build specific structure in specific town
		break;

	case BUY_ARMY:
		return town == g.town;

	//no check atm
	case COLLECT_RES:
	case TRADE: //TODO
		return (resID == g.resID); //every hero may collect resources
		break;
	case BUILD: //abstract build is indentical, TODO: consider building anything in town
		return true;
		break;
	case GATHER_TROOPS:
	case ISSUE_COMMAND:
	default:
		return false;
	}
}

bool Goals::AbstractGoal::operator<(AbstractGoal & g) //for std::unique
{
	//TODO: make sure it gets goals consistent with == operator
	if (goalType < g.goalType)
		return true;
	if (goalType > g.goalType)
		return false;
	if (hero < g.hero)
		return true;
	if (hero > g.hero)
		return false;
	if (tile < g.tile)
		return true;
	if (g.tile < tile)
		return false;
	if (objid < g.objid)
		return true;
	if (objid > g.objid)
		return false;
	if (town < g.town)
		return true;
	if (town > g.town)
		return false;
	if (value < g.value)
		return true;
	if (value > g.value)
		return false;
	if (priority < g.priority)
		return true;
	if (priority > g.priority)
		return false;
	if (resID < g.resID)
		return true;
	if (resID > g.resID)
		return false;
	if (bid < g.bid)
		return true;
	if (bid > g.bid)
		return false;
	if (aid < g.aid)
		return true;
	if (aid > g.aid)
		return false;
	return false;
}

//TODO: find out why the following are not generated automatically on MVS?

namespace Goals
{
	template<>
	void CGoal<Win>::accept(VCAI * ai)
	{
		ai->tryRealize(static_cast<Win &>(*this));
	}

	template<>
	void CGoal<Build>::accept(VCAI * ai)
	{
		ai->tryRealize(static_cast<Build &>(*this));
	}
	template<>
	float CGoal<Win>::accept(FuzzyHelper * f)
	{
		return f->evaluate(static_cast<Win &>(*this));
	}

	template<>
	float CGoal<Build>::accept(FuzzyHelper * f)
	{
		return f->evaluate(static_cast<Build &>(*this));
	}
	bool TSubgoal::operator==(const TSubgoal & rhs) const
	{
		return *get() == *rhs.get(); //comparison for Goals is overloaded, so they don't need to be identical to match
	}

	bool TSubgoal::operator<(const TSubgoal & rhs) const
	{
		return get() < rhs.get(); //compae by value
	}

	bool BuyArmy::operator==(AbstractGoal & g)
	{
		if (g.goalType != goalType)
			return false;
		//if (hero && hero != g.hero)
		//	return false;
		return town == g.town;
	}
	bool BuyArmy::fulfillsMe(TSubgoal goal)
	{
		//if (hero && hero != goal->hero)
		//	return false;
		return town == goal->town && goal->value >= value; //can always buy more army
	}
	TSubgoal BuyArmy::whatToDoToAchieve()
	{
		//TODO: calculate the actual cost of units instead
		TResources price;
		price[Res::GOLD] = value * 0.4f; //some approximate value
		return ai->ah->whatToDo(price, iAmElementar()); //buy right now or gather resources
	}
	std::string BuyArmy::completeMessage() const
	{
		return boost::format("Bought army of value %d in town of %s") % value, town->name;
	}
}

TSubgoal Trade::whatToDoToAchieve()
{
	return iAmElementar();
}
bool Trade::operator==(AbstractGoal & g)
{
	if (g.goalType != goalType)
		return false;
	if (g.resID == resID)
		if (g.value == value) //TODO: not sure if that logic is consitent
			return true;

	return false;
}

//TSubgoal AbstractGoal::whatToDoToAchieve()
//{
//	logAi->debug("Decomposing goal of type %s",name());
//	return sptr (Goals::Explore());
//}

TSubgoal Win::whatToDoToAchieve()
{
	auto toBool = [=](const EventCondition &)
	{
		// TODO: proper implementation
		// Right now even already fulfilled goals will be included into generated list
		// Proper check should test if event condition is already fulfilled
		// Easiest way to do this is to call CGameState::checkForVictory but this function should not be
		// used on client side or in AI code
		return false;
	};

	std::vector<EventCondition> goals;

	for(const TriggeredEvent & event : cb->getMapHeader()->triggeredEvents)
	{
		//TODO: try to eliminate human player(s) using loss conditions that have isHuman element

		if(event.effect.type == EventEffect::VICTORY)
		{
			boost::range::copy(event.trigger.getFulfillmentCandidates(toBool), std::back_inserter(goals));
		}
	}

	//TODO: instead of returning first encountered goal AI should generate list of possible subgoals
	for(const EventCondition & goal : goals)
	{
		switch(goal.condition)
		{
		case EventCondition::HAVE_ARTIFACT:
			return sptr(Goals::GetArtOfType(goal.objectType));
		case EventCondition::DESTROY:
		{
			if(goal.object)
			{
				auto obj = cb->getObj(goal.object->id);
				if(obj)
					if(obj->getOwner() == ai->playerID) //we can't capture our own object
						return sptr(Goals::Conquer());


				return sptr(Goals::VisitObj(goal.object->id.getNum()));
			}
			else
			{
				// TODO: destroy all objects of type goal.objectType
				// This situation represents "kill all creatures" condition from H3
				break;
			}
		}
		case EventCondition::HAVE_BUILDING:
		{
			// TODO build other buildings apart from Grail
			// goal.objectType = buidingID to build
			// goal.object = optional, town in which building should be built
			// Represents "Improve town" condition from H3 (but unlike H3 it consists from 2 separate conditions)

			if(goal.objectType == BuildingID::GRAIL)
			{
				if(auto h = ai->getHeroWithGrail())
				{
					//hero is in a town that can host Grail
					if(h->visitedTown && !vstd::contains(h->visitedTown->forbiddenBuildings, BuildingID::GRAIL))
					{
						const CGTownInstance * t = h->visitedTown;
						return sptr(Goals::BuildThis(BuildingID::GRAIL, t).setpriority(10));
					}
					else
					{
						auto towns = cb->getTownsInfo();
						towns.erase(boost::remove_if(towns,
									     [](const CGTownInstance * t) -> bool
							{
								return vstd::contains(t->forbiddenBuildings, BuildingID::GRAIL);
							}),
							    towns.end());
						boost::sort(towns, CDistanceSorter(h.get()));
						if(towns.size())
						{
							return sptr(Goals::VisitTile(towns.front()->visitablePos()).sethero(h));
						}
					}
				}
				double ratio = 0;
				// maybe make this check a bit more complex? For example:
				// 0.75 -> dig randomly within 3 tiles radius
				// 0.85 -> radius now 2 tiles
				// 0.95 -> 1 tile radius, position is fully known
				// AFAIK H3 AI does something like this
				int3 grailPos = cb->getGrailPos(&ratio);
				if(ratio > 0.99)
				{
					return sptr(Goals::DigAtTile(grailPos));
				} //TODO: use FIND_OBJ
				else if(const CGObjectInstance * obj = ai->getUnvisitedObj(objWithID<Obj::OBELISK>)) //there are unvisited Obelisks
					return sptr(Goals::VisitObj(obj->id.getNum()));
				else
					return sptr(Goals::Explore());
			}
			break;
		}
		case EventCondition::CONTROL:
		{
			if(goal.object)
			{
				return sptr(Goals::VisitObj(goal.object->id.getNum()));
			}
			else
			{
				//TODO: control all objects of type "goal.objectType"
				// Represents H3 condition "Flag all mines"
				break;
			}
		}

		case EventCondition::HAVE_RESOURCES:
			//TODO mines? piles? marketplace?
			//save?
			return sptr(Goals::CollectRes(static_cast<Res::ERes>(goal.objectType), goal.value));
		case EventCondition::HAVE_CREATURES:
			return sptr(Goals::GatherTroops(goal.objectType, goal.value));
		case EventCondition::TRANSPORT:
		{
			//TODO. merge with bring Grail to town? So AI will first dig grail, then transport it using this goal and builds it
			// Represents "transport artifact" condition:
			// goal.objectType = type of artifact
			// goal.object = destination-town where artifact should be transported
			break;
		}
		case EventCondition::STANDARD_WIN:
			return sptr(Goals::Conquer());

		// Conditions that likely don't need any implementation
		case EventCondition::DAYS_PASSED:
			break; // goal.value = number of days for condition to trigger
		case EventCondition::DAYS_WITHOUT_TOWN:
			break; // goal.value = number of days to trigger this
		case EventCondition::IS_HUMAN:
			break; // Should be only used in calculation of candidates (see toBool lambda)
		case EventCondition::CONST_VALUE:
			break;

		case EventCondition::HAVE_0:
		case EventCondition::HAVE_BUILDING_0:
		case EventCondition::DESTROY_0:
			//TODO: support new condition format
			return sptr(Goals::Conquer());
		default:
			assert(0);
		}
	}
	return sptr(Goals::Invalid());
}

TSubgoal FindObj::whatToDoToAchieve()
{
	const CGObjectInstance * o = nullptr;
	if(resID > -1) //specified
	{
		for(const CGObjectInstance * obj : ai->visitableObjs)
		{
			if(obj->ID == objid && obj->subID == resID)
			{
				o = obj;
				break; //TODO: consider multiple objects and choose best
			}
		}
	}
	else
	{
		for(const CGObjectInstance * obj : ai->visitableObjs)
		{
			if(obj->ID == objid)
			{
				o = obj;
				break; //TODO: consider multiple objects and choose best
			}
		}
	}
	if(o && ai->isAccessible(o->pos)) //we don't use isAccessibleForHero as we don't know which hero it is
		return sptr(Goals::VisitObj(o->id.getNum()));
	else
		return sptr(Goals::Explore());
}

bool Goals::FindObj::fulfillsMe(TSubgoal goal)
{
	if (goal->goalType == Goals::VISIT_TILE) //visiting tile visits object at same time
	{
		if (!hero || hero == goal->hero)
			for (auto obj : cb->getVisitableObjs(goal->tile)) //check if any object on that tile matches criteria
				if (obj->visitablePos() == goal->tile) //object could be removed
					if (obj->ID == objid && obj->subID == resID) //same type and subtype
						return true;
	}
	return false;
}

std::string VisitObj::completeMessage() const
{
	return "hero " + hero.get()->name + " captured Object ID = " + boost::lexical_cast<std::string>(objid);
}

TGoalVec VisitObj::getAllPossibleSubgoals()
{
	TGoalVec goalList;
	const CGObjectInstance * obj = cb->getObjInstance(ObjectInstanceID(objid));
	if(!obj)
	{
		throw cannotFulfillGoalException("Object is missing - goal is invalid now!");
	}

	int3 pos = obj->visitablePos();
	if(hero)
	{
		if(ai->isAccessibleForHero(pos, hero))
		{
			if(isSafeToVisit(hero, pos))
				goalList.push_back(sptr(Goals::VisitObj(obj->id.getNum()).sethero(hero)));
			else
				goalList.push_back(sptr(Goals::GatherArmy(evaluateDanger(pos, hero.h) * SAFE_ATTACK_CONSTANT).sethero(hero).setisAbstract(true)));

			return goalList;
		}
	}
	else
	{
		for(auto potentialVisitor : cb->getHeroesInfo())
		{
			if(ai->isAccessibleForHero(pos, potentialVisitor))
			{
				if(isSafeToVisit(potentialVisitor, pos))
					goalList.push_back(sptr(Goals::VisitObj(obj->id.getNum()).sethero(potentialVisitor)));
				else
					goalList.push_back(sptr(Goals::GatherArmy(evaluateDanger(pos, potentialVisitor) * SAFE_ATTACK_CONSTANT).sethero(potentialVisitor).setisAbstract(true)));
			}
		}
		if(!goalList.empty())
		{
			return goalList;
		}
	}

	goalList.push_back(sptr(Goals::ClearWayTo(pos)));
	return goalList;
}

TSubgoal VisitObj::whatToDoToAchieve()
{
	auto bestGoal = fh->chooseSolution(getAllPossibleSubgoals());

	if(bestGoal->goalType == Goals::VISIT_OBJ && bestGoal->hero)
		bestGoal->setisElementar(true);

	return bestGoal;
}

Goals::VisitObj::VisitObj(int Objid) : CGoal(Goals::VISIT_OBJ)
{
	objid = Objid;
	tile = ai->myCb->getObjInstance(ObjectInstanceID(objid))->visitablePos();
	priority = 3;
}

bool Goals::VisitObj::operator==(AbstractGoal & g)
{
	if (g.goalType != goalType)
		return false;
	return g.objid == objid;
}

bool VisitObj::fulfillsMe(TSubgoal goal)
{
	if(goal->goalType == Goals::VISIT_TILE)
	{
		if (!hero || hero == goal->hero)
		{
			auto obj = cb->getObjInstance(ObjectInstanceID(objid));
			if (obj && obj->visitablePos() == goal->tile) //object could be removed
				return true;
		}
	}
	return false;
}

std::string VisitHero::completeMessage() const
{
	return "hero " + hero.get()->name + " visited hero " + boost::lexical_cast<std::string>(objid);
}

TSubgoal VisitHero::whatToDoToAchieve()
{
	const CGObjectInstance * obj = cb->getObj(ObjectInstanceID(objid));
	if(!obj)
		return sptr(Goals::Explore());
	int3 pos = obj->visitablePos();

	if(hero && ai->isAccessibleForHero(pos, hero, true) && isSafeToVisit(hero, pos)) //enemy heroes can get reinforcements
	{
		if(hero->pos == pos)
			logAi->error("Hero %s tries to visit himself.", hero.name);
		else
		{
			//can't use VISIT_TILE here as tile appears blocked by target hero
			//FIXME: elementar goal should not be abstract
			return sptr(Goals::VisitHero(objid).sethero(hero).settile(pos).setisElementar(true));
		}
	}
	return sptr(Goals::Invalid());
}

bool Goals::VisitHero::operator==(AbstractGoal & g)
{
	if (g.goalType != goalType)
		return false;
	return g.hero == hero && g.objid == objid;
}

bool VisitHero::fulfillsMe(TSubgoal goal)
{
	//TODO: VisitObj shoudl not be used for heroes, but...
	if(goal->goalType == Goals::VISIT_TILE)
	{
		auto obj = cb->getObj(ObjectInstanceID(objid));
		if (!obj)
		{
			logAi->error("Hero %s: VisitHero::fulfillsMe at %s: object %d not found", hero.name, goal->tile.toString(), objid);
			return false;
		}
		return obj->visitablePos() == goal->tile;
	}
	return false;
}

TSubgoal GetArtOfType::whatToDoToAchieve()
{
	TSubgoal alternativeWay = CGoal::lookForArtSmart(aid); //TODO: use
	if(alternativeWay->invalid())
		return sptr(Goals::FindObj(Obj::ARTIFACT, aid));
	return sptr(Goals::Invalid());
}

TSubgoal ClearWayTo::whatToDoToAchieve()
{
	assert(cb->isInTheMap(tile)); //set tile
	if(!cb->isVisible(tile))
	{
		logAi->error("Clear way should be used with visible tiles!");
		return sptr(Goals::Explore());
	}

	return (fh->chooseSolution(getAllPossibleSubgoals()));
}

bool Goals::ClearWayTo::operator==(AbstractGoal & g)
{
	if (g.goalType != goalType)
		return false;
	return g.goalType == goalType && g.tile == tile;
}

bool Goals::ClearWayTo::fulfillsMe(TSubgoal goal)
{
	if (goal->goalType == Goals::VISIT_TILE)
	{
		if (!hero || hero == goal->hero)
			return tile == goal->tile;
	}
	return false;
}

TGoalVec ClearWayTo::getAllPossibleSubgoals()
{
	TGoalVec ret;

	std::vector<const CGHeroInstance *> heroes;
	if(hero)
		heroes.push_back(hero.h);
	else
		heroes = cb->getHeroesInfo();

	for(auto h : heroes)
	{
		//TODO: handle clearing way to allied heroes that are blocked
		//if ((hero && hero->visitablePos() == tile && hero == *h) || //we can't free the way ourselves
		//	h->visitablePos() == tile) //we are already on that tile! what does it mean?
		//	continue;

		//if our hero is trapped, make sure we request clearing the way from OUR perspective

		vstd::concatenate(ret, ai->ah->howToVisitTile(h, tile));
	}

	if(ret.empty() && ai->canRecruitAnyHero())
		ret.push_back(sptr(Goals::RecruitHero()));

	if(ret.empty())
	{
		logAi->warn("There is no known way to clear the way to tile %s", tile.toString());
		throw goalFulfilledException(sptr(Goals::ClearWayTo(tile))); //make sure asigned hero gets unlocked
	}

	return ret;
}

std::string Explore::completeMessage() const
{
	return "Hero " + hero.get()->name + " completed exploration";
}

TSubgoal Explore::whatToDoToAchieve()
{
	auto ret = fh->chooseSolution(getAllPossibleSubgoals());
	if(hero) //use best step for this hero
	{
		return ret;
	}
	else
	{
		if(ret->hero.get(true))
			return sptr(sethero(ret->hero.h).setisAbstract(true)); //choose this hero and then continue with him
		else
			return ret; //other solutions, like buying hero from tavern
	}
}

TGoalVec Explore::getAllPossibleSubgoals()
{
	TGoalVec ret;
	std::vector<const CGHeroInstance *> heroes;

	if(hero)
	{
		heroes.push_back(hero.h);
	}
	else
	{
		//heroes = ai->getUnblockedHeroes();
		heroes = cb->getHeroesInfo();
		vstd::erase_if(heroes, [](const HeroPtr h)
		{
			if(ai->getGoal(h)->goalType == Goals::EXPLORE) //do not reassign hero who is already explorer
				return true;

			if(!ai->isAbleToExplore(h))
				return true;

			return !h->movement; //saves time, immobile heroes are useless anyway
		});
	}

	//try to use buildings that uncover map
	std::vector<const CGObjectInstance *> objs;
	for(auto obj : ai->visitableObjs)
	{
		if(!vstd::contains(ai->alreadyVisited, obj))
		{
			switch(obj->ID.num)
			{
			case Obj::REDWOOD_OBSERVATORY:
			case Obj::PILLAR_OF_FIRE:
			case Obj::CARTOGRAPHER:
				objs.push_back(obj);
				break;
			case Obj::MONOLITH_ONE_WAY_ENTRANCE:
			case Obj::MONOLITH_TWO_WAY:
			case Obj::SUBTERRANEAN_GATE:
				auto tObj = dynamic_cast<const CGTeleport *>(obj);
				assert(ai->knownTeleportChannels.find(tObj->channel) != ai->knownTeleportChannels.end());
				if(TeleportChannel::IMPASSABLE != ai->knownTeleportChannels[tObj->channel]->passability)
					objs.push_back(obj);
				break;
			}
		}
		else
		{
			switch(obj->ID.num)
			{
			case Obj::MONOLITH_TWO_WAY:
			case Obj::SUBTERRANEAN_GATE:
				auto tObj = dynamic_cast<const CGTeleport *>(obj);
				if(TeleportChannel::IMPASSABLE == ai->knownTeleportChannels[tObj->channel]->passability)
					break;
				for(auto exit : ai->knownTeleportChannels[tObj->channel]->exits)
				{
					if(!cb->getObj(exit))
					{ // Always attempt to visit two-way teleports if one of channel exits is not visible
						objs.push_back(obj);
						break;
					}
				}
				break;
			}
		}
	}

	auto primaryHero = ai->primaryHero().h;
	for(auto h : heroes)
	{
		for(auto obj : objs) //double loop, performance risk?
		{
			auto waysToVisitObj = ai->ah->howToVisitObj(h, obj);

			vstd::concatenate(ret, waysToVisitObj);
		}

		int3 t = whereToExplore(h);
		if(t.valid())
		{
			ret.push_back(sptr(Goals::VisitTile(t).sethero(h)));
		}
		else
		{
			//FIXME: possible issues when gathering army to break
			if(hero.h == h || //exporation is assigned to this hero
				(!hero && h == primaryHero)) //not assigned to specific hero, let our main hero do the job
			{
				t = ai->explorationDesperate(h);  //check this only ONCE, high cost
				if (t.valid()) //don't waste time if we are completely blocked
				{
					auto waysToVisitTile = ai->ah->howToVisitTile(h, t);

					vstd::concatenate(ret, waysToVisitTile);
					continue;
				}
			}
			ai->markHeroUnableToExplore(h); //there is no freely accessible tile, do not poll this hero anymore
		}
	}
	//we either don't have hero yet or none of heroes can explore
	if((!hero || ret.empty()) && ai->canRecruitAnyHero())
		ret.push_back(sptr(Goals::RecruitHero()));

	if(ret.empty())
	{
		throw goalFulfilledException(sptr(Goals::Explore().sethero(hero)));
	}
	//throw cannotFulfillGoalException("Cannot explore - no possible ways found!");

	return ret;
}

bool Explore::fulfillsMe(TSubgoal goal)
{
	if(goal->goalType == Goals::EXPLORE)
	{
		if(goal->hero)
			return hero == goal->hero;
		else
			return true; //cancel ALL exploration
	}
	return false;
}

TSubgoal RecruitHero::whatToDoToAchieve()
{
	const CGTownInstance * t = ai->findTownWithTavern();
	if(!t)
		return sptr(Goals::BuildThis(BuildingID::TAVERN).setpriority(2));

	TResources res;
	res[Res::GOLD] = GameConstants::HERO_GOLD_COST;
	return ai->ah->whatToDo(res, iAmElementar()); //either buy immediately, or collect res
}

bool Goals::RecruitHero::operator==(AbstractGoal & g)
{
	if (g.goalType != goalType)
		return false;
	//TODO: check town and hero
	return true; //for now, recruiting any hero will do
}

std::string VisitTile::completeMessage() const
{
	return "Hero " + hero.get()->name + " visited tile " + tile.toString();
}

TSubgoal VisitTile::whatToDoToAchieve()
{
	auto ret = fh->chooseSolution(getAllPossibleSubgoals());

	if(ret->hero)
	{
		if(isSafeToVisit(ret->hero, tile) && ai->isAccessibleForHero(tile, ret->hero))
		{
			ret->setisElementar(true);
			return ret;
		}
		else
		{
			return sptr(Goals::GatherArmy(evaluateDanger(tile, *ret->hero) * SAFE_ATTACK_CONSTANT)
				    .sethero(ret->hero).setisAbstract(true));
		}
	}
	return ret;
}

bool Goals::VisitTile::operator==(AbstractGoal & g)
{
	if (g.goalType != goalType)
		return false;
	return g.goalType == goalType && g.tile == tile;
}

TGoalVec VisitTile::getAllPossibleSubgoals()
{
	assert(cb->isInTheMap(tile));

	TGoalVec ret;
	if(!cb->isVisible(tile))
		ret.push_back(sptr(Goals::Explore())); //what sense does it make?
	else
	{
		std::vector<const CGHeroInstance *> heroes;
		if(hero)
			heroes.push_back(hero.h); //use assigned hero if any
		else
			heroes = cb->getHeroesInfo(); //use most convenient hero

		for(auto h : heroes)
		{
			if(ai->isAccessibleForHero(tile, h))
				ret.push_back(sptr(Goals::VisitTile(tile).sethero(h)));
		}
		if(ai->canRecruitAnyHero())
			ret.push_back(sptr(Goals::RecruitHero()));
	}
	if(ret.empty())
	{
		auto obj = vstd::frontOrNull(cb->getVisitableObjs(tile));
		if(obj && obj->ID == Obj::HERO && obj->tempOwner == ai->playerID) //our own hero stands on that tile
		{
			if(hero.get(true) && hero->id == obj->id) //if it's assigned hero, visit tile. If it's different hero, we can't visit tile now
				ret.push_back(sptr(Goals::VisitTile(tile).sethero(dynamic_cast<const CGHeroInstance *>(obj)).setisElementar(true)));
			else
				throw cannotFulfillGoalException("Tile is already occupied by another hero "); //FIXME: we should give up this tile earlier
		}
		else
			ret.push_back(sptr(Goals::ClearWayTo(tile)));
	}

	//important - at least one sub-goal must handle case which is impossible to fulfill (unreachable tile)
	return ret;
}

TSubgoal DigAtTile::whatToDoToAchieve()
{
	const CGObjectInstance * firstObj = vstd::frontOrNull(cb->getVisitableObjs(tile));
	if(firstObj && firstObj->ID == Obj::HERO && firstObj->tempOwner == ai->playerID) //we have hero at dest
	{
		const CGHeroInstance * h = dynamic_cast<const CGHeroInstance *>(firstObj);
		sethero(h).setisElementar(true);
		return sptr(*this);
	}

	return sptr(Goals::VisitTile(tile));
}

bool Goals::DigAtTile::operator==(AbstractGoal & g)
{
	if (g.goalType != goalType)
		return false;
	return g.goalType == goalType && g.tile == tile;
}

TSubgoal BuildThis::whatToDoToAchieve()
{
	auto b = BuildingID(bid);

	// find town if not set
	if (!town && hero)
		town = hero->visitedTown;

	if (!town)
	{
		for (const CGTownInstance * t : cb->getTownsInfo())
		{
			switch (cb->canBuildStructure(town, b))
			{
			case EBuildingState::ALLOWED:
				town = t;
				break; //TODO: look for prerequisites? this is not our reponsibility
			default:
				continue;
			}
		}
	}
	if (town) //we have specific town to build this
	{
		switch (cb->canBuildStructure(town, b))
		{
			case EBuildingState::ALLOWED:
			case EBuildingState::NO_RESOURCES:
			{
				auto res = town->town->buildings.at(BuildingID(bid))->resources;
				return ai->ah->whatToDo(res, iAmElementar()); //realize immediately or gather resources
			}
				break;
			default:
				throw cannotFulfillGoalException("Not possible to build");
		}
	}
	else
		throw cannotFulfillGoalException("Cannot find town to build this");
}

TGoalVec Goals::CollectRes::getAllPossibleSubgoals()
{
	TGoalVec ret;

	auto givesResource = [this](const CGObjectInstance * obj) -> bool
	{
		//TODO: move this logic to object side
		//TODO: remember mithril exists
		//TODO: water objects
		//TODO: Creature banks

		//return false first from once-visitable, before checking if they were even visited
		switch (obj->ID.num)
		{
		case Obj::TREASURE_CHEST:
			return resID == Res::GOLD;
			break;
		case Obj::RESOURCE:
			return obj->subID == resID;
			break;
		case Obj::MINE:
			return (obj->subID == resID &&
				(cb->getPlayerRelations(obj->tempOwner, ai->playerID) == PlayerRelations::ENEMIES)); //don't capture our mines
			break;
		case Obj::CAMPFIRE:
			return true; //contains all resources
			break;
		case Obj::WINDMILL:
			switch (resID)
			{
			case Res::GOLD:
			case Res::WOOD:
				return false;
			}
			break;
		case Obj::WATER_WHEEL:
			if (resID != Res::GOLD)
				return false;
			break;
		case Obj::MYSTICAL_GARDEN:
			if ((resID != Res::GOLD) && (resID != Res::GEMS))
				return false;
			break;
		case Obj::LEAN_TO:
		case Obj::WAGON:
			if (resID != Res::GOLD)
				return false;
			break;
		default:
			return false;
			break;
		}
		return !vstd::contains(ai->alreadyVisited, obj); //for weekly / once visitable
	};

	std::vector<const CGObjectInstance *> objs;
	for (auto obj : ai->visitableObjs)
	{
		if (givesResource(obj))
			objs.push_back(obj);
	}
	for (auto h : cb->getHeroesInfo())
	{
		std::vector<const CGObjectInstance *> ourObjs(objs); //copy common objects

		for (auto obj : ai->reservedHeroesMap[h]) //add objects reserved by this hero
		{
			if (givesResource(obj))
				ourObjs.push_back(obj);
		}

		for (auto obj : ourObjs)
		{
			auto waysToGo = ai->ah->howToVisitObj(h, ObjectIdRef(obj));

			vstd::concatenate(ret, waysToGo);
		}
	}
	return ret;
}

TSubgoal CollectRes::whatToDoToAchieve()
{
	auto goals = getAllPossibleSubgoals();
	auto trade = whatToDoToTrade();
	if (!trade->invalid())
		goals.push_back(trade);

	if (goals.empty())
		return sptr(Goals::Explore()); //we can always do that
	else
		return fh->chooseSolution(goals); //TODO: evaluate trading
}

TSubgoal Goals::CollectRes::whatToDoToTrade()
{
	std::vector<const IMarket *> markets;

	std::vector<const CGObjectInstance *> visObjs;
	ai->retrieveVisitableObjs(visObjs, true);
	for (const CGObjectInstance * obj : visObjs)
	{
		if (const IMarket * m = IMarket::castFrom(obj, false))
		{
			if (obj->ID == Obj::TOWN && obj->tempOwner == ai->playerID && m->allowsTrade(EMarketMode::RESOURCE_RESOURCE))
				markets.push_back(m);
			else if (obj->ID == Obj::TRADING_POST)
				markets.push_back(m);
		}
	}

	boost::sort(markets, [](const IMarket * m1, const IMarket * m2) -> bool
	{
		return m1->getMarketEfficiency() < m2->getMarketEfficiency();
	});

	markets.erase(boost::remove_if(markets, [](const IMarket * market) -> bool
	{
		if (!(market->o->ID == Obj::TOWN && market->o->tempOwner == ai->playerID))
		{
			if (!ai->isAccessible(market->o->visitablePos()))
				return true;
		}
		return false;
	}), markets.end());

	if (!markets.size())
	{
		for (const CGTownInstance * t : cb->getTownsInfo())
		{
			if (cb->canBuildStructure(t, BuildingID::MARKETPLACE) == EBuildingState::ALLOWED)
				return sptr(Goals::BuildThis(BuildingID::MARKETPLACE, t).setpriority(2));
		}
	}
	else
	{
		const IMarket * m = markets.back();
		//attempt trade at back (best prices)
		int howManyCanWeBuy = 0;
		for (Res::ERes i = Res::WOOD; i <= Res::GOLD; vstd::advance(i, 1))
		{
			if (i == resID)
				continue;
			int toGive = -1, toReceive = -1;
			m->getOffer(i, resID, toGive, toReceive, EMarketMode::RESOURCE_RESOURCE);
			assert(toGive > 0 && toReceive > 0);
			howManyCanWeBuy += toReceive * (ai->ah->freeResources()[i] / toGive);
		}

		if (howManyCanWeBuy >= value)
		{
			auto backObj = cb->getTopObj(m->o->visitablePos()); //it'll be a hero if we have one there; otherwise marketplace
			assert(backObj);
			auto objid = m->o->id.getNum();
			if (backObj->tempOwner != ai->playerID) //top object not owned
			{
				return sptr(Goals::VisitObj(objid)); //just go there
			}
			else //either it's our town, or we have hero there
			{
				Goals::Trade trade(resID, value, objid);
				return sptr(trade.setisElementar(true)); //we can do this immediately
			}
		}
	}
	return sptr(Goals::Invalid()); //cannot trade
}

bool CollectRes::fulfillsMe(TSubgoal goal)
{
	if (goal->resID == resID)
		if (goal->value >= value)
			return true;

	return false;
}

bool Goals::CollectRes::operator==(AbstractGoal & g)
{
	if (g.goalType != goalType)
		return false;
	if (g.resID == resID)
		if (g.value == value) //TODO: not sure if that logic is consitent
			return true;

	return false;
}

TSubgoal GatherTroops::whatToDoToAchieve()
{
	std::vector<const CGDwelling *> dwellings;
	for(const CGTownInstance * t : cb->getTownsInfo())
	{
		auto creature = VLC->creh->creatures[objid];
		if(t->subID == creature->faction) //TODO: how to force AI to build unupgraded creatures? :O
		{
			auto creatures = vstd::tryAt(t->town->creatures, creature->level - 1);
			if(!creatures)
				continue;

			int upgradeNumber = vstd::find_pos(*creatures, creature->idNumber);
			if(upgradeNumber < 0)
				continue;

			BuildingID bid(BuildingID::DWELL_FIRST + creature->level - 1 + upgradeNumber * GameConstants::CREATURES_PER_TOWN);
			if(t->hasBuilt(bid)) //this assumes only creatures with dwellings are assigned to faction
			{
				dwellings.push_back(t);
			}
			else
			{
				return sptr(Goals::BuildThis(bid, t).setpriority(priority));
			}
		}
	}
	for(auto obj : ai->visitableObjs)
	{
		if(obj->ID != Obj::CREATURE_GENERATOR1) //TODO: what with other creature generators?
			continue;

		auto d = dynamic_cast<const CGDwelling *>(obj);
		for(auto creature : d->creatures)
		{
			if(creature.first) //there are more than 0 creatures avaliabe
			{
				for(auto type : creature.second)
				{
					if(type == objid && ai->ah->freeResources().canAfford(VLC->creh->creatures[type]->cost))
						dwellings.push_back(d);
				}
			}
		}
	}
	if(dwellings.size())
	{
		typedef std::map<const CGHeroInstance *, const CGDwelling *> TDwellMap;

		// sorted helper
		auto comparator = [](const TDwellMap::value_type & a, const TDwellMap::value_type & b) -> bool
		{
			const CGPathNode * ln = ai->myCb->getPathsInfo(a.first)->getPathInfo(a.second->visitablePos());
			const CGPathNode * rn = ai->myCb->getPathsInfo(b.first)->getPathInfo(b.second->visitablePos());

			if(ln->turns != rn->turns)
				return ln->turns < rn->turns;

			return (ln->moveRemains > rn->moveRemains);
		};

		// for all owned heroes generate map <hero -> nearest dwelling>
		TDwellMap nearestDwellings;
		for(const CGHeroInstance * hero : cb->getHeroesInfo(true))
		{
			nearestDwellings[hero] = *boost::range::min_element(dwellings, CDistanceSorter(hero));
		}
		if(nearestDwellings.size())
		{
			// find hero who is nearest to a dwelling
			const CGDwelling * nearest = boost::range::min_element(nearestDwellings, comparator)->second;
			if(!nearest)
				throw cannotFulfillGoalException("Cannot find nearest dwelling!");

			return sptr(Goals::VisitObj(nearest->id.getNum()));
		}
		else
			return sptr(Goals::Explore());
	}
	else
	{
		return sptr(Goals::Explore());
	}
	//TODO: exchange troops between heroes
}

bool Goals::GatherTroops::fulfillsMe(TSubgoal goal)
{
	if (!hero || hero == goal->hero) //we got army for desired hero or any hero
		if (goal->objid == objid) //same creature type //TODO: consider upgrades?
			if (goal->value >= value) //notify every time we get resources?
				return true;
	return false;
}

TSubgoal Conquer::whatToDoToAchieve()
{
	return fh->chooseSolution(getAllPossibleSubgoals());
}
TGoalVec Conquer::getAllPossibleSubgoals()
{
	TGoalVec ret;

	auto conquerable = [](const CGObjectInstance * obj) -> bool
	{
		if(cb->getPlayerRelations(ai->playerID, obj->tempOwner) == PlayerRelations::ENEMIES)
		{
			switch(obj->ID.num)
			{
			case Obj::TOWN:
			case Obj::HERO:
			case Obj::CREATURE_GENERATOR1:
			case Obj::MINE: //TODO: check ai->knownSubterraneanGates
				return true;
			}
		}
		return false;
	};

	std::vector<const CGObjectInstance *> objs;
	for(auto obj : ai->visitableObjs)
	{
		if(conquerable(obj))
			objs.push_back(obj);
	}

	for(auto h : cb->getHeroesInfo())
	{
		std::vector<const CGObjectInstance *> ourObjs(objs); //copy common objects

		for(auto obj : ai->reservedHeroesMap[h]) //add objects reserved by this hero
		{
			if(conquerable(obj))
				ourObjs.push_back(obj);
		}
		for(auto obj : ourObjs)
		{
			auto waysToGo = ai->ah->howToVisitObj(h, ObjectIdRef(obj));

			vstd::concatenate(ret, waysToGo);
		}
	}
	if(!objs.empty() && ai->canRecruitAnyHero()) //probably no point to recruit hero if we see no objects to capture
		ret.push_back(sptr(Goals::RecruitHero()));

	if(ret.empty())
		ret.push_back(sptr(Goals::Explore())); //we need to find an enemy
	return ret;
}

TGoalVec Goals::Build::getAllPossibleSubgoals()
{
	TGoalVec ret;

	for (const CGTownInstance * t : cb->getTownsInfo())
	{
		//start fresh with every town
		ai->ah->getBuildingOptions(t);
		auto ib = ai->ah->immediateBuilding();
		if (ib.is_initialized())
		{
			ret.push_back(sptr(Goals::BuildThis(ib.get().bid, t).setpriority(2))); //prioritize buildings we can build quick
		}
		else //try build later
		{
			auto eb = ai->ah->expensiveBuilding();
			if (eb.is_initialized())
			{
				auto pb = eb.get(); //gather resources for any we can't afford
				auto goal = ai->ah->whatToDo(pb.price, sptr(Goals::BuildThis(pb.bid, t).setpriority(0.5)));
				ret.push_back(goal);
			}
		}
	}

	if (ret.empty())
		throw cannotFulfillGoalException("BUILD has been realized as much as possible.");
	else
		return ret;
}

TSubgoal Build::whatToDoToAchieve()
{
	return fh->chooseSolution(getAllPossibleSubgoals());
}

bool Goals::Build::fulfillsMe(TSubgoal goal)
{
	if (goal->goalType == Goals::BUILD || goal->goalType == Goals::BUILD_STRUCTURE)
		return (!town || town == goal->town); //building anything will do, in this town if set
	else
		return false;
}

TSubgoal Invalid::whatToDoToAchieve()
{
	return iAmElementar();
}

std::string GatherArmy::completeMessage() const
{
	return "Hero " + hero.get()->name + " gathered army of value " + boost::lexical_cast<std::string>(value);
}

TSubgoal GatherArmy::whatToDoToAchieve()
{
	//TODO: find hero if none set
	assert(hero.h);

	return fh->chooseSolution(getAllPossibleSubgoals()); //find dwelling. use current hero to prevent him from doing nothing.
}

static const BuildingID unitsSource[] = { BuildingID::DWELL_LVL_1, BuildingID::DWELL_LVL_2, BuildingID::DWELL_LVL_3,
	BuildingID::DWELL_LVL_4, BuildingID::DWELL_LVL_5, BuildingID::DWELL_LVL_6, BuildingID::DWELL_LVL_7};

TGoalVec GatherArmy::getAllPossibleSubgoals()
{
	//get all possible towns, heroes and dwellings we may use
	TGoalVec ret;

	if(!hero.validAndSet())
	{
		return ret;
	}

	//TODO: include evaluation of monsters gather in calculation
	for(auto t : cb->getTownsInfo())
	{
		auto pos = t->visitablePos();
		if(ai->isAccessibleForHero(pos, hero))
		{
			//grab army from town
			if(!t->visitingHero && howManyReinforcementsCanGet(hero, t))
			{
				if(!vstd::contains(ai->townVisitsThisWeek[hero], t))
					ret.push_back(sptr(Goals::VisitTile(pos).sethero(hero)));
			}
			//buy army in town
			if (!t->visitingHero || t->visitingHero == hero.get(true))
			{
				ui32 val = std::min<ui32>(value, howManyReinforcementsCanBuy(hero, t));
				if (val)
				{
					auto goal = sptr(Goals::BuyArmy(t, val).sethero(hero));
					if(!ai->ah->containsObjective(goal)) //avoid loops caused by reserving same objective twice
						ret.push_back(goal);
					else
						logAi->debug("Can not buy army, because of ai->ah->containsObjective");
				}
			}
			//build dwelling
			//TODO: plan building over multiple turns?
			//auto bid = ah->canBuildAnyStructure(t, std::vector<BuildingID>(unitsSource, unitsSource + ARRAY_COUNT(unitsSource)), 8 - cb->getDate(Date::DAY_OF_WEEK));
			auto bid = ai->ah->canBuildAnyStructure(t, std::vector<BuildingID>(unitsSource, unitsSource + ARRAY_COUNT(unitsSource)), 1);
			if (bid.is_initialized())
			{
				auto goal = sptr(BuildThis(bid.get(), t).setpriority(priority));
				if (!ai->ah->containsObjective(goal)) //avoid loops caused by reserving same objective twice
					ret.push_back(goal);
				else
					logAi->debug("Can not build a structure, because of ai->ah->containsObjective");
			}
		}
	}

	auto otherHeroes = cb->getHeroesInfo();
	auto heroDummy = hero;
	vstd::erase_if(otherHeroes, [heroDummy](const CGHeroInstance * h)
	{
		if(h == heroDummy.h)
			return true;
		else if(!ai->isAccessibleForHero(heroDummy->visitablePos(), h, true))
			return true;
		else if(!ai->canGetArmy(heroDummy.h, h)) //TODO: return actual aiValue
			return true;
		else if(ai->getGoal(h)->goalType == Goals::GATHER_ARMY)
			return true;
		else
		   return false;
	});
	for(auto h : otherHeroes)
	{
		// Go to the other hero if we are faster
		if (!vstd::contains(ai->visitedHeroes[hero], h)
			&& ai->isAccessibleForHero(h->visitablePos(), hero, true)) //visit only once each turn //FIXME: this is only bug workaround
			ret.push_back(sptr(Goals::VisitHero(h->id.getNum()).setisAbstract(true).sethero(hero)));
		// Let the other hero come to us
		if (!vstd::contains(ai->visitedHeroes[h], hero))
			ret.push_back(sptr(Goals::VisitHero(hero->id.getNum()).setisAbstract(true).sethero(h)));
	}

	std::vector<const CGObjectInstance *> objs;
	for(auto obj : ai->visitableObjs)
	{
		if(obj->ID == Obj::CREATURE_GENERATOR1)
		{
			auto relationToOwner = cb->getPlayerRelations(obj->getOwner(), ai->playerID);

			//Use flagged dwellings only when there are available creatures that we can afford
			if(relationToOwner == PlayerRelations::SAME_PLAYER)
			{
				auto dwelling = dynamic_cast<const CGDwelling *>(obj);

				ui32 val = std::min<ui32>(value, howManyReinforcementsCanBuy(hero, dwelling));

				if(val)
				{
					for(auto & creLevel : dwelling->creatures)
					{
						if(creLevel.first)
						{
							for(auto & creatureID : creLevel.second)
							{
								auto creature = VLC->creh->creatures[creatureID];
								if(ai->ah->freeResources().canAfford(creature->cost))
									objs.push_back(obj); //TODO: reserve resources?
							}
						}
					}
				}
			}
		}
	}
	for(auto h : cb->getHeroesInfo())
	{
		for(auto obj : objs)
		{
			//find safe dwelling
			if(ai->isGoodForVisit(obj, h))
			{
				vstd::concatenate(ret, ai->ah->howToVisitObj(h, obj));
			}
		}
	}

	if(ai->canRecruitAnyHero() && ai->ah->freeGold() > GameConstants::HERO_GOLD_COST) //this is not stupid in early phase of game
	{
		if(auto t = ai->findTownWithTavern())
		{
			for(auto h : cb->getAvailableHeroes(t)) //we assume that all towns have same set of heroes
			{
				if(h && h->getTotalStrength() > 500) //do not buy heroes with single creatures for GatherArmy
				{
					ret.push_back(sptr(Goals::RecruitHero()));
					break;
				}
			}
		}
	}

	if(ret.empty())
	{
		if(hero == ai->primaryHero())
			ret.push_back(sptr(Goals::Explore()));
		else
			throw cannotFulfillGoalException("No ways to gather army");
	}

	return ret;
}

//TSubgoal AbstractGoal::whatToDoToAchieve()
//{
//	logAi->debug("Decomposing goal of type %s",name());
//	return sptr (Goals::Explore());
//}

TSubgoal AbstractGoal::goVisitOrLookFor(const CGObjectInstance * obj)
{
	if(obj)
		return sptr(Goals::VisitObj(obj->id.getNum()));
	else
		return sptr(Goals::Explore());
}

TSubgoal AbstractGoal::lookForArtSmart(int aid)
{
	return sptr(Goals::Invalid());
}

bool AbstractGoal::invalid() const
{
	return goalType == INVALID;
}

void AbstractGoal::accept(VCAI * ai)
{
	ai->tryRealize(*this);
}


template<typename T>
void CGoal<T>::accept(VCAI * ai)
{
	ai->tryRealize(static_cast<T &>(*this)); //casting enforces template instantiation
}

float AbstractGoal::accept(FuzzyHelper * f)
{
	return f->evaluate(*this);
}

template<typename T>
float CGoal<T>::accept(FuzzyHelper * f)
{
	return f->evaluate(static_cast<T &>(*this)); //casting enforces template instantiation
}
