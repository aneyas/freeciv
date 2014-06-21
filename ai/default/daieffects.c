/********************************************************************** 
 Freeciv - Copyright (C) 2002 - The Freeciv Project
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
***********************************************************************/

#ifdef HAVE_CONFIG_H
#include <fc_config.h>
#endif

/* common */
#include "city.h"
#include "effects.h"
#include "game.h"
#include "government.h"
#include "player.h"
#include "specialist.h"
#include "traderoutes.h"
#include "victory.h"

/* server/advisors */
#include "advdata.h"
#include "advtools.h"

/* ai */
#include "aitraits.h"
#include "handicaps.h"


#include "daieffects.h"

/**************************************************************************
  Return the number of "luxury specialists".  This is the number of
  specialists who provide at least HAPPY_COST luxury, being the number of
  luxuries needed to make one citizen content or happy.

  The AI assumes that for any specialist that provides HAPPY_COST luxury, 
  if we can get that luxury from some other source it allows the specialist 
  to become a worker.  The benefits from an extra worker are weighed against
  the losses from acquiring the two extra luxury.

  This is a very bad model if the abilities of specialists are changed.
  But as long as the civ2 model of specialists is used it will continue
  to work okay.
**************************************************************************/
static int get_entertainers(const struct city *pcity)
{
  int providers = 0;

  specialist_type_iterate(i) {
    if (get_specialist_output(pcity, i, O_LUXURY) >= game.info.happy_cost) {
      providers += pcity->specialists[i];
    }
  } specialist_type_iterate_end;

  return providers;
}

/************************************************************************** 
  How desirable particular effect making people content is for a
  particular city?
**************************************************************************/
int dai_content_effect_value(const struct player *pplayer,
                             const struct city *pcity,
                             int amount,
                             int num_cities,
                             int happiness_step)
{
  int v = 0;

  if (get_city_bonus(pcity, EFT_NO_UNHAPPY) <= 0) {
    int i;
    int max_converted = pcity->feel[CITIZEN_UNHAPPY][FEELING_FINAL];

    /* See if some step of happiness calculation gets capped */
    for (i = happiness_step; i < FEELING_FINAL; i++) {
      max_converted = MIN(max_converted, pcity->feel[CITIZEN_UNHAPPY][i]);
    }

    v = MIN(amount, max_converted + get_entertainers(pcity)) * 35;
  }

  if (num_cities > 1) {
    int factor = 2;

    /* Try to build wonders to offset empire size unhappiness */
    if (city_list_size(pplayer->cities) 
        > get_player_bonus(pplayer, EFT_EMPIRE_SIZE_BASE)) {
      if (get_player_bonus(pplayer, EFT_EMPIRE_SIZE_BASE) > 0) {
        factor += city_list_size(pplayer->cities) 
          / MAX(get_player_bonus(pplayer, EFT_EMPIRE_SIZE_STEP), 1);
      }
      factor += 2;
    }
    v += factor * num_cities * amount;
  }

  return v;
}

/**************************************************************************
  Is a unit class affected by an effect?
  Note that some effects have unit_type restrictions that may cause this
  test to be inaccurate.
**************************************************************************/
static bool is_unit_class_affected_by(const struct unit_class *pclass,
                                      const struct effect *peffect)
{
  requirement_list_iterate(peffect->reqs, preq) {
    if ( (preq->source.kind == VUT_UCLASS
          && ((preq->source.value.uclass != pclass && preq->present)
              || (preq->source.value.uclass == pclass && !preq->present)))
        || (preq->source.kind == VUT_UCFLAG
            && ( (uclass_has_flag(pclass, preq->source.value.unitclassflag)
                  && !preq->present)
                || (!uclass_has_flag(pclass, preq->source.value.unitclassflag)
                    && preq->present)))) {
      return FALSE;
    }
  } requirement_list_iterate_end;
  return TRUE;
}

/**************************************************************************
  Number of AI stats units affected by effect
**************************************************************************/
static int num_affected_units(const struct effect *peffect,
                              const struct adv_data *ai)
{
  int unit_count = 0;

  unit_class_iterate(pclass) {
    if (is_unit_class_affected_by(pclass, peffect)) {
      unit_count += ai->stats.units.byclass[uclass_index(pclass)];
    }
  } unit_class_iterate_end;

  return unit_count;
}

/**************************************************************************
  How desirable is a particular effect for a particular city?
  Expressed as an adjustment of the base value (v)
  given the number of cities in range (c).
**************************************************************************/
int dai_effect_value(struct player *pplayer, struct government *gov,
                     const struct adv_data *ai, const struct city *pcity,
                     const bool capital, int turns,
                     const struct effect *peffect, const int c,
                     const int nplayers, int v)
{
  int amount = peffect->value;
  bool affects_sea_capable_units = FALSE;
  bool affects_land_capable_units = FALSE;
  int num;
  int trait;

  if (amount == 0) {
    /* We could prune such effects in ruleset loading already,
     * but we allow people tuning their rulesets to temporarily disable
     * the effect by setting value to 0 without need to completely
     * remove the effect.
     * Shortcutting these effects here is not only for performance,
     * more importantly it makes sure code below assuming amount to
     * be positive does not assign positive value. */
    return 0;
  }

  switch (peffect->type) {
  /* These effects have already been evaluated in base_want() */
  case EFT_CAPITAL_CITY:
  case EFT_GOV_CENTER:
  case EFT_UPKEEP_FREE:
  case EFT_TECH_UPKEEP_FREE:
  case EFT_POLLU_POP_PCT:
  case EFT_POLLU_PROD_PCT:
  case EFT_OUTPUT_BONUS:
  case EFT_OUTPUT_BONUS_2:
  case EFT_OUTPUT_ADD_TILE:
  case EFT_OUTPUT_INC_TILE:
  case EFT_OUTPUT_PER_TILE:
  case EFT_OUTPUT_WASTE:
  case EFT_OUTPUT_WASTE_BY_DISTANCE:
  case EFT_OUTPUT_WASTE_PCT:
  case EFT_SPECIALIST_OUTPUT:
  case EFT_ENEMY_CITIZEN_UNHAPPY_PCT:
  case EFT_IRRIGATION_PCT:
  case EFT_MINING_PCT:
  case EFT_OUTPUT_TILE_PUNISH_PCT:
    break;

  case EFT_CITY_VISION_RADIUS_SQ:
  case EFT_UNIT_VISION_RADIUS_SQ:
    /* Wild guess.  "Amount" is the number of tiles (on average) that
     * will be revealed by the effect.  Note that with an omniscient
     * AI this effect is actually not useful at all. */
    v += c * amount;
    break;

  case EFT_TURN_YEARS:
  case EFT_SLOW_DOWN_TIMELINE:
    /* AI doesn't care about these. */
    break;

    /* WAG evaluated effects */
  case EFT_INCITE_COST_PCT:
    v += c * amount / 100;
    break;
  case EFT_MAKE_HAPPY:
    v += (get_entertainers(pcity) + pcity->feel[CITIZEN_UNHAPPY][FEELING_FINAL]) * 5 * amount;
    if (city_list_size(pplayer->cities)
	> get_player_bonus(pplayer, EFT_EMPIRE_SIZE_BASE)) {
      v += c * amount; /* offset large empire size */
    }
    v += c * amount;
    break;
  case EFT_UNIT_RECOVER:
    /* TODO */
    break;
  case EFT_NO_UNHAPPY:
    v += (get_entertainers(pcity) + pcity->feel[CITIZEN_UNHAPPY][FEELING_FINAL]) * 30;
    break;
  case EFT_FORCE_CONTENT:
    v += dai_content_effect_value(pplayer, pcity, amount, c, FEELING_FINAL);
    break;
  case EFT_MAKE_CONTENT:
    v += dai_content_effect_value(pplayer, pcity, amount, c, FEELING_EFFECT);
    break;
  case EFT_MAKE_CONTENT_MIL_PER:
    if (get_city_bonus(pcity, EFT_NO_UNHAPPY) <= 0) {
      v += MIN(pcity->feel[CITIZEN_UNHAPPY][FEELING_FINAL] + get_entertainers(pcity),
	       amount) * 25;
      v += MIN(amount, 5) * c;
    }
    break;
  case EFT_MAKE_CONTENT_MIL:
    if (get_city_bonus(pcity, EFT_NO_UNHAPPY) <= 0) {
      v += pcity->feel[CITIZEN_UNHAPPY][FEELING_FINAL] * amount
        * MAX(unit_list_size(pcity->units_supported), 0) * 2;
      v += c * MAX(amount + 2, 1);
    }
    break;
  case EFT_TECH_PARASITE:
    {
      int bulbs;
      int value;
	  
      if (nplayers <= amount) {
	break;
      }

      bulbs = 0;
      players_iterate(aplayer) {
	int potential = aplayer->bulbs_last_turn
	  + city_list_size(aplayer->cities) + 1;

	if (players_on_same_team(aplayer, pplayer)) {
	  continue;
	}
	bulbs += potential;
      } players_iterate_end;
  
      /* For some number of turns we will be receiving bulbs for free
       * Bulbs should be amortized properly for each turn.
       * We use formula for the sum of geometric series:
       */
      value = bulbs * (1.0 - pow(1.0 - (1.0 / MORT), turns)) * MORT;
	  
      value = value  * (100 - game.server.freecost)	  
	* (nplayers - amount) / (nplayers * amount * 100);
	  
      /* WAG */
      value /= 3;
	
      v += value;
      break;
    }
  case EFT_GROWTH_FOOD:
    v += c * 4 + (amount / 7) * pcity->surplus[O_FOOD];
    break;
  case EFT_HEALTH_PCT:
    /* Is plague possible */
    if (game.info.illness_on) {
      v += c * 5 + (amount / 5) * pcity->server.illness;
    }
    break;
  case EFT_AIRLIFT:
    /* FIXME: We need some smart algorithm here. The below is 
     * totally braindead. */
    v += c + MIN(ai->stats.units.airliftable, 13);
    break;
  case EFT_ANY_GOVERNMENT:
    if (!can_change_to_government(pplayer, ai->goal.govt.gov)) {
      v += MIN(MIN(ai->goal.govt.val, 65),
	       num_unknown_techs_for_goal(pplayer, ai->goal.govt.req) * 10);
    }
    break;
  case EFT_ENABLE_NUKE:
    /* Treat nuke as a Cruise Missile upgrade */
    v += 20 + ai->stats.units.missiles * 5;
    break;
  case EFT_ENABLE_SPACE:
    if (victory_enabled(VC_SPACERACE)) {
      v += 5;
      if (ai->dipl.production_leader == pplayer) {
	v += 100;
      }
    }
    break;
  case EFT_VICTORY:
    v += 250;
    break;
  case EFT_GIVE_IMM_TECH:
    if (adv_wants_science(pplayer)) {
      v += amount * (game.info.sciencebox + 1);
    }
    break;
  case EFT_HAVE_EMBASSIES:
    v += 5 * nplayers;
    break;
  case EFT_REVEAL_CITIES:
  case EFT_NO_ANARCHY:
    break;  /* Useless for AI */
  case EFT_NUKE_PROOF:
    if (ai->threats.nuclear) {
      v += city_size_get(pcity) * unit_list_size(pcity->tile->units)
           * (capital + 1) * amount / 100;
    }
    break;
  case EFT_REVEAL_MAP:
    if (!ai->explore.land_done || !ai->explore.sea_done) {
      v += 10;
    }
    break;
  case EFT_SIZE_UNLIMIT:
    /* Note we look up the SIZE_UNLIMIT again right below.  This could
     * be avoided... */
    if (amount > 0) {
      if (get_city_bonus(pcity, EFT_SIZE_UNLIMIT) <= 0) {
        amount = 20; /* really big city */
      }
    } else {
      /* Effect trying to remove unlimit. */
      v -= 30 * c * ai->food_priority;
      break;
    }
    /* there not being a break here is deliberate, mind you */
  case EFT_SIZE_ADJ:
    if (get_city_bonus(pcity, EFT_SIZE_UNLIMIT) <= 0) {
      const int aqueduct_size = get_city_bonus(pcity, EFT_SIZE_ADJ);
      int extra_food = pcity->surplus[O_FOOD];

      if (city_granary_size(city_size_get(pcity)) == pcity->food_stock) {
        /* The idea being that if we have a full granary, we have an
         * automatic surplus of our granary excess in addition to anything
         * collected by city workers. */
        extra_food += pcity->food_stock - 
                      city_granary_size(city_size_get(pcity) - 1);
      }

      if (amount > 0 && !city_can_grow_to(pcity, city_size_get(pcity) + 1)) {
	v += extra_food * ai->food_priority * amount;
	if (city_size_get(pcity) == aqueduct_size) {
	  v += 30 * extra_food;
	}
      }
      v += c * amount * 4 / aqueduct_size;
    }
    break;
  case EFT_SS_STRUCTURAL:
  case EFT_SS_COMPONENT:
  case EFT_SS_MODULE:
    if (victory_enabled(VC_SPACERACE)
	/* If someone has started building spaceship already or
	 * we have chance to win a spacerace */
	&& (ai->dipl.spacerace_leader
	    || ai->dipl.production_leader == pplayer)) {
      v += 95;
    }
    break;
  case EFT_SPY_RESISTANT:
    /* Uhm, problem: City Wall has -50% here!! */
    break;
  case EFT_MOVE_BONUS:
    num = num_affected_units(peffect, ai);
    v += (8 * v * amount + num);
    break;
  case EFT_UNIT_NO_LOSE_POP:
    v += unit_list_size(pcity->tile->units) * 2;
    break;
  case EFT_HP_REGEN:
    num = num_affected_units(peffect, ai);
    v += (5 * c + num);
    break;
  case EFT_VETERAN_COMBAT:
    num = num_affected_units(peffect, ai);
    v += (2 * c + num);
    break;
  case EFT_VETERAN_BUILD:
    /* FIXME: check other reqs (e.g., unitflag) */
    num = num_affected_units(peffect, ai);
    v += amount * (3 * c + num);
    break;
  case EFT_UPGRADE_UNIT:
    if (amount == 1) {
      v += ai->stats.units.upgradeable * 2;
    } else if (amount == 2) {
      v += ai->stats.units.upgradeable * 3;
    } else {
      v += ai->stats.units.upgradeable * 4;
    }
    break;
  case EFT_UNIT_BRIBE_COST_PCT:
    num = num_affected_units(peffect, ai);
    v += ((2 * c + num) * amount) / 400;
    break;
  case EFT_DEFEND_BONUS:
    if (has_handicap(pplayer, H_DEFENSIVE)) {
      v += amount / 10; /* make AI slow */
    }
    unit_class_iterate(pclass) {
      if (is_unit_class_affected_by(pclass, peffect)) {
        if (pclass->adv.sea_move != MOVE_NONE) {
          affects_sea_capable_units = TRUE;
        }
        if (pclass->adv.land_move != MOVE_NONE) {
          affects_land_capable_units = TRUE;
        }
      }
      if (affects_sea_capable_units && affects_land_capable_units) {
        /* Don't bother searching more if we already know enough. */
        break;
      }
    } unit_class_iterate_end;

    if (affects_sea_capable_units) {
      if (is_ocean_tile(pcity->tile)) {
        v += ai->threats.ocean[-tile_continent(pcity->tile)]
          ? amount/5 : amount/20;
      } else {
        adjc_iterate(pcity->tile, tile2) {
          if (is_ocean_tile(tile2)) {
            if (ai->threats.ocean[-tile_continent(tile2)]) {
              v += amount/5;
              break;
            }
          }
        } adjc_iterate_end;
      }
    }
    v += (amount/20 + ai->threats.invasions - 1) * c; /* for wonder */
    if (capital || affects_land_capable_units) {
      if (ai->threats.continent[tile_continent(pcity->tile)]
          || capital
          || (ai->threats.invasions
              /* FIXME: This ignores riverboats on some rulesets.
                        We should analyze rulesets when game starts
                        and have relevant checks here. */
              && is_terrain_class_near_tile(pcity->tile, TC_OCEAN))) {
        if (ai->threats.continent[tile_continent(pcity->tile)]) {
          v += amount;
        } else {
          v += amount / (!ai->threats.igwall ? (15 - capital * 5) : 15);
        }
      }
    }
    break;
  case EFT_GAIN_AI_LOVE:
    players_iterate(aplayer) {
      if (aplayer->ai_controlled) {
	if (has_handicap(pplayer, H_DEFENSIVE)) {
	  v += amount / 10;
	} else {
	  v += amount / 20;
	}
      }
    } players_iterate_end;
    break;
  case EFT_UPGRADE_PRICE_PCT:
    /* This is based on average base upgrade price of 50. */
    v -= ai->stats.units.upgradeable * amount / 2;
    break;
  /* Currently not supported for building AI - wait for modpack users */
  case EFT_CITY_UNHAPPY_SIZE:
  case EFT_UNHAPPY_FACTOR:
  case EFT_UPKEEP_FACTOR:
  case EFT_UNIT_UPKEEP_FREE_PER_CITY:
  case EFT_CIVIL_WAR_CHANCE:
  case EFT_EMPIRE_SIZE_BASE:
  case EFT_EMPIRE_SIZE_STEP:
  case EFT_MAX_RATES:
  case EFT_MARTIAL_LAW_EACH:
  case EFT_MARTIAL_LAW_MAX:
  case EFT_RAPTURE_GROW:
  case EFT_REVOLUTION_UNHAPPINESS:
  case EFT_HAS_SENATE:
  case EFT_INSPIRE_PARTISANS:
  case EFT_HAPPINESS_TO_GOLD:
  case EFT_FANATICS:
  case EFT_NO_DIPLOMACY:
  case EFT_NOT_TECH_SOURCE:
  case EFT_OUTPUT_PENALTY_TILE:
  case EFT_OUTPUT_INC_TILE_CELEBRATE:
  case EFT_TRADE_REVENUE_BONUS:
  case EFT_TILE_WORKABLE:
  case EFT_IRRIG_POSSIBLE:
  case EFT_TRANSFORM_POSSIBLE:
  case EFT_MINING_POSSIBLE:
  case EFT_IRRIG_TF_POSSIBLE:
  case EFT_MINING_TF_POSSIBLE:
  case EFT_PERFORMANCE:
  case EFT_HISTORY:
    break;
    /* This has no effect for AI */
  case EFT_VISIBLE_WALLS:
  case EFT_CITY_IMAGE:
  case EFT_SHIELD2GOLD_FACTOR:
    break;
  case EFT_TECH_COST_FACTOR:
    v -= amount * 50;
    break;
  case EFT_CITY_RADIUS_SQ:
    v += amount * 10; /* AI wants bigger city radii */
    break;
  case EFT_CITY_BUILD_SLOTS:
    v += amount * 10;
    break;
  case EFT_MIGRATION_PCT:
    /* consider all foreign cities within the set distance */
    iterate_outward(city_tile(pcity), game.server.mgr_distance + 1, ptile) {
      struct city *acity = tile_city(ptile);

      if (!acity || acity == pcity || city_owner(acity) == pplayer) {
        /* no city, the city in the center or own city */
        continue;
      }

      v += amount; /* AI wants migration into its cities! */
    } iterate_outward_end;
    break;
  case EFT_MAX_TRADE_ROUTES:
    trait = ai_trait_get_value(TRAIT_TRADER, pplayer);
    v += amount
      * (pow(2.0,
             (double) get_city_bonus(pcity, EFT_TRADE_REVENUE_BONUS) / 1000.0)
         + c)
      * trait
      / TRAIT_DEFAULT_VALUE;
    if (city_num_trade_routes(pcity) >= max_trade_routes(pcity)
        && amount > 0) {
      /* Has no free trade routes before this */
      v += trait;
    }
    break;
  case EFT_TRADEROUTE_PCT:
    {
      int trade = 0;

      trait = ai_trait_get_value(TRAIT_TRADER, pplayer);

      trade_routes_iterate(pcity, tgt) {
        trade += trade_between_cities(pcity, tgt);
      } trade_routes_iterate_end;

      v += trade * amount * trait / 100 / TRAIT_DEFAULT_VALUE;

      if (city_num_trade_routes(pcity) < max_trade_routes(pcity)
          && amount > 0) {
        /* Space for future routes */
        v += trait * 5 / TRAIT_DEFAULT_VALUE;
      }
    }
    break;
  case EFT_COUNT:
    log_error("Bad effect type.");
    break;
  }

  return v;
}