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
#ifndef FC__PLAYER_H
#define FC__PLAYER_H

#include "city.h"
#include "connection.h"		/* struct conn_list */
#include "nation.h"
#include "shared.h"
#include "spaceship.h"
#include "tech.h"
#include "unit.h"

struct tile;

#define PLAYER_DEFAULT_TAX_RATE 0
#define PLAYER_DEFAULT_SCIENCE_RATE 100
#define PLAYER_DEFAULT_LUXURY_RATE 0


enum handicap_type {
  H_NONE=0, /* no handicaps */
  H_RIGIDPROD=1, /* can't switch to/from building_unit without penalty */
  H_MAP=2, /* only knows map_get_known tiles */
  H_TECH=4, /* doesn't know what enemies have researched */
  H_CITYBUILDINGS=8, /* doesn't know what buildings are in enemy cities */
  H_CITYUNITS=16, /* doesn't know what units are in enemy cities */
  H_STACKS=32, /* doesn't know what units are in stacks */
  H_VETERAN=64, /* doesn't know veteran status of enemy units */
  H_SUB=128, /* doesn't know where subs may be lurking */
/* below this point are milder handicaps that I can actually implement -- Syela */
  H_RATES=256, /* can't set its rates beyond government limits */
  H_TARGETS=512, /* can't target anything it doesn't know exists */
  H_HUTS=1024 /* doesn't know which unseen tiles have huts on them */
/* anything else I have forgotten?  Let me know. -- Syela */
};


struct player_economic {
  int gold;
  int tax;
  int science;
  int luxury;
};

struct player_research {
  int researched;     /* # bulbs reseached */
  int researchpoints; /* # bulbs to complete */
  int researching;    /* invention being researched in */
  int changed_from;   /* if the player changed techs, which one changed from */
  int before_researched;	/* if the player changed techs, how many */
				/* points they had before the change */
  unsigned char inventions[A_LAST];
};

struct player_score {
  int happy;
  int content;
  int unhappy;
  int taxmen;
  int scientists;
  int elvis;
  int wonders;
  int techs;
  int techout;
  int landarea;
  int settledarea;
  int population;
  int cities;
  int units;
  int pollution;
  int literacy;
  int bnp;
  int mfg;
  int spaceship;
};

struct player_ai {
  int control;
  int tech_goal;
  int prev_gold;
  int maxbuycost;
  int est_upkeep; /* estimated upkeep of buildings in cities */
  int tech_want[A_LAST+1];
  int tech_turns[A_LAST+1]; /* saves zillions of calculations! */
  int handicap;			/* sum of enum handicap_type */
  int skill_level;		/* 0-10 value for save/load/display */
  int fuzzy;			/* chance in 1000 to mis-decide */
  int expand;			/* percentage factor to value new cities */
  int warmth; /* threat of global warming */
  int is_barbarian;
};

/* Diplomatic states (how one player views another).
 * (Some diplomatic states are "pacts" (mutual agreements), others aren't.)
 */
enum diplstate_type {
  DS_NEUTRAL = 0,
  DS_WAR,
  DS_CEASEFIRE,
  DS_PEACE,
  DS_ALLIANCE,
  DS_NO_CONTACT,
  DS_LAST	/* leave this last */
};

struct player_diplstate {
  enum diplstate_type type;	/* this player's disposition towards other */
  /* the following are for "pacts" */
  int turns_left;		/* until pact (e.g., cease-fire) ends */
  int has_reason_to_cancel;	/* 0: no, 1: this turn, 2: this or next turn */
};

/***************************************************************************
  On the distinction between nations(formerly races), players, and users,
  see freeciv_hackers_guide.txt
***************************************************************************/

struct player {
  int player_no;
  char name[MAX_LEN_NAME];
  char username[MAX_LEN_NAME];
  int is_male;
  int government;
  Nation_Type_id nation;
  int turn_done;
  int nturns_idle;
  int is_alive;
  int got_tech;
  int revolution;
  int capital; /* bool used to give player capital in first city. */
  int embassy;
  int reputation;
  struct player_diplstate diplstates[MAX_NUM_PLAYERS + MAX_NUM_BARBARIANS];
  int city_style;
  struct unit_list units;
  struct city_list cities;
  struct player_score score;
  struct player_economic economic;
  struct player_research research;
  struct player_spaceship spaceship;
  int future_tech;
  struct player_ai ai;
  int is_connected;		       /* observers don't count */
  struct connection *current_conn;     /* non-null while handling packet */
  struct conn_list connections;	       /* will replace conn */
  struct worklist worklists[MAX_NUM_WORKLISTS];
};

void player_init(struct player *plr);
struct player *find_player_by_name(char *name);
struct player *find_player_by_name_prefix(const char *name,
					  enum m_pre_result *result);
struct player *find_player_by_user(char *name);
void player_set_unit_focus_status(struct player *pplayer);
int player_has_embassy(struct player *pplayer, struct player *pplayer2);

int player_can_see_unit(struct player *pplayer, struct unit *punit);
int player_owns_city(struct player *pplayer, struct city *pcity);

struct city *player_find_city_by_id(struct player *pplayer, int city_id);
struct unit *player_find_unit_by_id(struct player *pplayer, int unit_id);

int player_in_city_radius(struct player *pplayer, int x, int y);
int player_owns_active_wonder(struct player *pplayer,
			      Impr_Type_id id);
int player_owns_active_govchange_wonder(struct player *pplayer);
int player_knows_improvement_tech(struct player *pplayer,
				   Impr_Type_id id);
int player_knows_techs_with_flag(struct player *pplayer, int flag);

void player_limit_to_government_rates(struct player *pplayer);

const char *player_addr_hack(struct player *pplayer);

struct city *find_palace(struct player *pplayer);

int ai_handicap(struct player *pplayer, enum handicap_type htype);
int ai_fuzzy(struct player *pplayer, int normal_decision);

const char *reputation_text(const int rep);
const char *diplstate_text(const enum diplstate_type type);

/* we have an int in some contexts, a pointer in others.  Yuk! */
const struct player_diplstate *pplayer_get_diplstate(const struct player *pplayer,
						     const struct player *pplayer2);
const struct player_diplstate *player_get_diplstate(const int player,
						    const int player2);
int pplayers_at_war(const struct player *pplayer, 
		    const struct player *pplayer2);
int players_at_war(const int player, const int player2);
int pplayers_allied(const struct player *pplayer,
		    const struct player *pplayer2);
int players_allied(const int player, const int player2);
int pplayers_non_attack(const struct player *pplayer,
			const struct player *pplayer2);
int players_non_attack(const int player, const int player2);

int is_barbarian(const struct player *pplayer);

#define players_iterate(PI_player)                                            \
{                                                                             \
  struct player *PI_player;                                                   \
  int PI_p_itr;                                                               \
  for (PI_p_itr = 0; PI_p_itr < game.nplayers; PI_p_itr++) {                  \
    PI_player = get_player(PI_p_itr);

#define players_iterate_end                                                   \
  }                                                                           \
}

#endif  /* FC__PLAYER_H */
