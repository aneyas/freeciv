/*
1.5.1

- altered the automatic build advisor, chooses more intelligent now. 
- fixed a bug in the automatic worker assignment scheme. the
  message cityname is bugged: etc... shouldn't happend again.
- the ai will only build 1 wonder on a continent at a given time.
- caravan control added, when ai is building wonders, "idle" cities  
  will help by building caravans and send them to the aid.

 */




/********************************************************************** 
 Freeciv - Copyright (C) 1996 - A Kjeldberg, L Gregersen, P Unold
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
***********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <player.h>
#include <city.h>
#include <game.h>
#include <unit.h>
#include <unithand.h>
#include <shared.h>
#include <cityhand.h>
#include <packets.h>
#include <map.h>
#include <mapgen.h>
#include <aitools.h>
#include <aihand.h>
#include <aiunit.h>
#include <aicity.h>
#include <aitech.h>
/****************************************************************************
  A man builds a city
  With banks and cathedrals
  A man melts the sand so he can 
  See the world outside
  A man makes a car 
  And builds a road to run them on
  A man dreams of leaving
  but he always stays behind
  And these are the days when our work has come assunder
  And these are the days when we look for something other
  /U2 Lemon.
******************************************************************************/
/*
1 Basic:
- (serv) AI <player> server command toggles AI on off                DONE
- (Unit) settler capable of building city                            DONE
- (City) cities capable of building units/buildings                  DONE
- (Hand) adjustments of tax/luxuries/science                         DONE
- (City) happiness control                                           DONE
- (Hand) change government                                           DONE
- (Tech) tech management                                             DONE
2 Medium:
- better city placement
- ability to explore island 
- Barbarians
- better unit building management
- better improvement management 
- taxcollecters/scientists
- spend spare trade on buying
- upgrade units
- (Unit) wonders/caravans                                            DONE
- defense/city value
- Tax/science/unit producing cities 
3 Advanced:
- ZOC
- continent defense
- sea superiority
- air superiority
- continent attack
4 Superadvanced:
- Transporters (attack on other continents)
- diplomati (ambassader/bytte tech/bytte kort med anden AI)


Step 1 is the basics, can be used for the blank seat people usually have when
they start a game, the AI will atleast do something other than just sit and 
wait, for the kill.
Step 2 will make it do it alot better, and hopefully the barbarians will be in
action.
3 if step 1 and 2 gives decent results, the AI will actually have survived the
building phase and we have to worry about units, step 3 is meant as defense.
So first in step 4 will we introduce attacking AI. (if it ever gets that far)
 */

void ai_before_work(struct player *pplayer);
void ai_manage_taxes(struct player *pplayer); 
void ai_manage_government(struct player *pplayer);
void ai_manage_diplomacy(struct player *pplayer);
void ai_after_work(struct player *pplayer);


/**************************************************************************
 Main AI routine.
**************************************************************************/

void ai_do_first_activities(struct player *pplayer)
{
  ai_before_work(pplayer); 
  ai_manage_units(pplayer); /* STOP.  Everything else is at end of turn. */
}

void ai_do_last_activities(struct player *pplayer)
{
  ai_manage_cities(pplayer);
/* if I were upgrading units, which I'm not, I would do it here -- Syela */ 
/* printf("Managing %s's taxes.\n", pplayer->name); */
  ai_manage_taxes(pplayer); 
/* printf("Managing %s's government.\n", pplayer->name); */
  ai_manage_government(pplayer); 
  ai_manage_diplomacy(pplayer);
  ai_manage_tech(pplayer); 
  ai_after_work(pplayer);
/* printf("Done with %s.\n", pplayer->name); */
}

/**************************************************************************
 update advisors/structures
**************************************************************************/
  
void ai_before_work(struct player *pplayer)
{
  ai_update_player_island_info(pplayer);
}


/**************************************************************************
 Trade tech and stuff, this one will probably be blank for a long time.
**************************************************************************/

void ai_manage_diplomacy(struct player *pplayer)
{

}

/**************************************************************************
 well am not sure what will happend here yet, maybe some more analysis
**************************************************************************/

void ai_after_work(struct player *pplayer)
{

}


/**************************************************************************
...
**************************************************************************/

int ai_calc_city_buy(struct city *pcity)
{
  int val;
  if (pcity->is_building_unit) {   /* add wartime stuff here */
    return ((pcity->size*10)/(2*city_get_defenders(pcity)+1))
;
  } else {                         /* crude function, add some value stuff */
    if (pcity->currently_building==B_CAPITAL)
      return 0;
    val = ((pcity->size*20)/(city_get_buildings(pcity)+1));
    val = val * (30 - pcity->shield_prod); /* yes it'll become negative if > 30 */
    return val;
  }
}

/**************************************************************************
.. Spend money
**************************************************************************/
void ai_spend_gold(struct player *pplayer, int gold)
{ /* obsoleted by Syela */
  struct city *pc2=NULL;
  int maxwant, curwant;
  maxwant = 0;
  city_list_iterate(pplayer->cities, pcity) 
    if ((curwant = ai_calc_city_buy(pcity)) > maxwant) {
      maxwant = curwant;
      pc2 = pcity;
    }
  city_list_iterate_end;

  if (!pc2) return;
  if (pc2->is_building_unit) {
    if (city_buy_cost(pc2) > gold)
      return;
    pplayer->economic.gold -= city_buy_cost(pc2);
    pc2->shield_stock = unit_value(pc2->currently_building);
  } else { 
    if (city_buy_cost(pc2) < gold) {
      pplayer->economic.gold -= city_buy_cost(pc2);
      pc2->shield_stock = improvement_value(pc2->currently_building);
      return;
    }
    /* we don't have to end the build, we just pool in some $ */
    if (is_wonder(pc2->currently_building)) 
      pc2->shield_stock +=(gold/4);
    else
      pc2->shield_stock +=(gold/2);
    pplayer->economic.gold -= gold;
  }
}
 


/**************************************************************************
.. Set tax/science/luxury rates. Tax Rates > 40 indicates a crisis.
**************************************************************************/

void ai_manage_taxes(struct player *pplayer) 
{ /* total rewrite by Syela */
  int gnow=pplayer->economic.gold;
  int gthen=pplayer->ai.prev_gold;
  int sad = 0, trade = 0, m, n, i, expense = 0;
  int elvises[11] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
  int hhjj[11] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
  int food_weighting[3] = { 15, 14, 13 }; 
 
  pplayer->economic.science += pplayer->economic.luxury;
/* without the above line, auto_arrange does strange things we must avoid -- Syela */
  pplayer->economic.luxury = 0;
  city_list_iterate(pplayer->cities, pcity) 
    auto_arrange_workers(pcity); /* I hate doing this but I need to stop the flipflop */
    sad += pcity->ppl_unhappy[4];
    trade += pcity->trade_prod;
/*    printf("%s has %d trade.\n", pcity->name, pcity->trade_prod); */
    for (i = 0; i < B_LAST; i++)
      if (city_got_building(pcity, i)) expense += improvement_upkeep(pcity,i);
  city_list_iterate_end;

/*  printf("%s has %d trade.\n", pplayer->name, trade); */
  if (!trade) return; /* damn division by zero! */

  pplayer->economic.luxury = 0;
  city_list_iterate(pplayer->cities, pcity) 
    n = (pcity->ppl_unhappy[4] - pcity->ppl_happy[4]) * 20; /* need this much lux */
    for (i = 0; i <= 10; i++) {
      m = ((n - pcity->trade_prod * i + 19) / 20);
      elvises[i] += MAX(m, 0) * ai_best_tile_value(pcity);
    }
/* printf("Does %s want to be bigger? %d\n", pcity->name, wants_to_be_bigger(pcity)); */
    if (pcity->size > 4 && pplayer->government >= G_REPUBLIC &&
        !pcity->ppl_unhappy[4] && wants_to_be_bigger(pcity)) {
/*      printf("%d happy people in %s\n", pcity->ppl_happy[4], pcity->name); */
      n = ((pcity->size + 1) / 2 - pcity->ppl_happy[4]) * 20;
      for (i = 0; i <= 10; i++) {
        if (pcity->trade_prod * i >= n && pcity->food_surplus > 0)
          hhjj[i] += (pcity->size * (city_got_building(pcity,B_GRANARY) ? 3 : 2) *
              game.foodbox / 2 - pcity->food_stock) *
              food_weighting[get_race(pplayer)->attack] / pcity->size *
              (pcity->was_happy ? 4 : 3); /* maybe should be ? 4 : 2 */
      }
    } /* hhjj[i] is (we think) the desirability of partying with lux = 10 * i */
  city_list_iterate_end;

  for (i = 0; i <= 10; i++) elvises[i] += (trade * i) / 10 * 8;
  /* elvises[i] is the production + income lost to elvises with lux = i * 10 */
  n = 0; /* default to 0 lux */
  for (i = 1; i <= 10; i++) if (elvises[i] < elvises[n]) n = i;
/* two thousand zero zero party over it's out of time */
  for (i = 0; i <= 10; i++) {
    hhjj[i] -= (trade * i) / 10 * 8; /* hhjj is now our bonus */
/*    printf("hhjj[%d] = %d for %s.\n", i, hhjj[i], pplayer->name);  */
  }
  /* want to max the hhjj */
  for (i = n; i <= 10; i++) if (hhjj[i] > hhjj[n]) n = i;

/*  n = (sad * 40 + trade) / trade / 2;  WAG, now disabled */

  pplayer->economic.luxury = n * 10;



  /* do president sale here */

  if (pplayer->research.researching==A_NONE) {
    pplayer->economic.tax = 100 - pplayer->economic.luxury;
  } else { /* have to balance things logically */
/* if we need 50 gold and we have trade = 100, need 50 % tax (n = 5) */
    n = ((ai_gold_reserve(pplayer) - gnow - expense) * 20 + trade) / trade / 2;
    if (n < 1) n = 1; /* should allow 0 tax? */
    while (n > 10 - (pplayer->economic.luxury / 10)) n--;
    pplayer->economic.tax = 10 * n;
  }
  pplayer->economic.science = 100 - pplayer->economic.tax - pplayer->economic.luxury;

  city_list_iterate(pplayer->cities, pcity) 
    auto_arrange_workers(pcity);
    if (ai_fix_unhappy(pcity))
      ai_scientists_taxmen(pcity);
  city_list_iterate_end;
}

/* --------------------------GOVERNMENT--------------------------------- */

/**************************************************************************
 change the government form, if it can and there is a good reason
**************************************************************************/

void ai_manage_government(struct player *pplayer)
{
  int government = get_race(pplayer)->goals.government;
  if (pplayer->government == government)
    return;
  if (can_change_to_government(pplayer, government)) {
    ai_government_change(pplayer, government);
    return;
  }
  switch (government) {
  case G_COMMUNISM:
    if (can_change_to_government(pplayer, G_MONARCHY)) 
      ai_government_change(pplayer, G_MONARCHY);
    break;
  case G_DEMOCRACY:
    if (can_change_to_government(pplayer, G_REPUBLIC)) 
      ai_government_change(pplayer, G_REPUBLIC);
    else if (can_change_to_government(pplayer, G_MONARCHY)) 
      ai_government_change(pplayer, G_MONARCHY);
    break;
  case G_REPUBLIC:
    if (can_change_to_government(pplayer, G_MONARCHY)) 
      ai_government_change(pplayer, G_MONARCHY); /* better than despotism! -- Syela */
    break;
  }
}














