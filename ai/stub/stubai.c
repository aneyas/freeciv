/***********************************************************************
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

#ifdef HAVE_CONFIG_H
#include <fc_config.h>
#endif

/* common */
#include "ai.h"
#include "player.h"

/* server/advisors */
#include "advdata.h"
#include "autosettlers.h"

/* ai/default */
#include "advdiplomacy.h"
#include "advdomestic.h"
#include "advmilitary.h"
#include "aicity.h"
#include "aidata.h"
#include "aiferry.h"
#include "aihand.h"
#include "ailog.h"
#include "aiplayer.h"
#include "aisettler.h"
#include "aitools.h"
#include "aiunit.h"

#include "stubai.h"

const char *fc_ai_stub_capstr(void);

static struct ai_type *self = NULL;

/**************************************************************************
  Set pointer to ai type of the classic ai.
**************************************************************************/
static void classic_ai_set_self(struct ai_type *ai)
{
  self = ai;
}

/**************************************************************************
  Get pointer to ai type of the classic ai.
**************************************************************************/
static struct ai_type *classic_ai_get_self(void)
{
  return self;
}


/**************************************************************************
  Call default ai with classic ai type as parameter.
**************************************************************************/
static void cai_player_alloc(struct player *pplayer)
{
  struct ai_type *deftype = classic_ai_get_self();

  dai_player_alloc(deftype, pplayer);
}

/**************************************************************************
  Call default ai with classic ai type as parameter.
**************************************************************************/
static void cai_player_free(struct player *pplayer)
{
  struct ai_type *deftype = classic_ai_get_self();

  dai_player_free(deftype, pplayer);
}

/**************************************************************************
  Call default ai with classic ai type as parameter.
**************************************************************************/
static void cai_player_save(struct player *pplayer, struct section_file *file,
                     int plrno)
{
  struct ai_type *deftype = classic_ai_get_self();

  dai_player_save(deftype, "ai", pplayer, file, plrno);
}

/**************************************************************************
  Call default ai with classic ai type as parameter.
**************************************************************************/
static void cai_player_load(struct player *pplayer,
                            const struct section_file *file, int plrno)
{
  struct ai_type *deftype = classic_ai_get_self();

  dai_player_load(deftype, "ai", pplayer, file, plrno);
}


/**************************************************************************
  Call default ai with classic ai type as parameter.
**************************************************************************/
static void cai_auto_settler_reset(struct player *pplayer)
{
  struct ai_type *deftype = classic_ai_get_self();

  dai_auto_settler_reset(deftype, pplayer);
}

/**************************************************************************
  Call default ai with classic ai type as parameter.
**************************************************************************/
static void cai_auto_settler_run(struct player *pplayer, struct unit *punit,
                                 struct settlermap *state)
{
  struct ai_type *deftype = classic_ai_get_self();

  dai_auto_settler_run(deftype, pplayer, punit, state);
}

/**************************************************************************
  Call default ai with classic ai type as parameter.
**************************************************************************/
static void cai_auto_settler_cont(struct player *pplayer, struct unit *punit,
                                  struct settlermap *state)
{
  struct ai_type *deftype = classic_ai_get_self();

  dai_auto_settler_cont(deftype, pplayer, punit, state);
}



/**************************************************************************
  Return module capability string
**************************************************************************/
const char *fc_ai_stub_capstr(void)
{
  return FC_AI_MOD_CAPSTR;
}

/**************************************************************************
  Setup player ai_funcs function pointers.
**************************************************************************/
bool fc_ai_stub_setup(struct ai_type *ai)
{
  classic_ai_set_self(ai);
  strncpy(ai->name, "stub_classic", sizeof(ai->name));

  ai->funcs.player_alloc = cai_player_alloc;
  ai->funcs.player_free = cai_player_free;
  ai->funcs.player_save = cai_player_save;
  ai->funcs.player_load = cai_player_load;


  ai->funcs.settler_reset = cai_auto_settler_reset;
  ai->funcs.settler_run = cai_auto_settler_run;
  ai->funcs.settler_cont = cai_auto_settler_cont;

  ai->funcs.unit_move = NULL;
  ai->funcs.unit_task = NULL;


  return TRUE;
}
