/****************************************************************************
 Freeciv - Copyright (C) 2004 - The Freeciv Team
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
****************************************************************************/

#ifdef HAVE_CONFIG_H
#include <fc_config.h>
#endif

/* common */
#include "base.h"
#include "game.h"
#include "road.h"

#include "extras.h"

#define MAX_EXTRA_TYPES (S_LAST + MAX_BASE_TYPES + MAX_ROAD_TYPES)

static struct extra_type extras[MAX_EXTRA_TYPES];

/****************************************************************************
  Initialize extras structures.
****************************************************************************/
void extras_init(void)
{
  int i;

  for (i = 0; i < S_LAST; i++) {
    extras[i].id = i;
    extras[i].type = EXTRA_SPECIAL;
    extras[i].data.special = i;
  }

  for (; i < MAX_EXTRA_TYPES; i++) {
    extras[i].id = i;
  }
}

/****************************************************************************
  Free the memory associated with extras
****************************************************************************/
void extras_free(void)
{
  base_types_free();
  road_types_free();
}

/**************************************************************************
  Return the number of extra_types.
**************************************************************************/
int extra_count(void)
{
  return S_LAST + game.control.num_base_types + game.control.num_road_types;
}

/**************************************************************************
  Return the extra id.
**************************************************************************/
int extra_number(const struct extra_type *pextra)
{
  fc_assert_ret_val(NULL != pextra, -1);

  return pextra->id;
}

/**************************************************************************
  Return the extra index.
**************************************************************************/
int extra_index(const struct extra_type *pextra)
{
  fc_assert_ret_val(NULL != pextra, -1);

  return pextra - extras;
}

/****************************************************************************
  Return extras type of given id.
****************************************************************************/
struct extra_type *extra_by_number(int id)
{
  fc_assert_ret_val(id >= 0 && id < MAX_EXTRA_TYPES, NULL);

  return &extras[id];
}

/****************************************************************************
  Get extra of the given type and given subid
****************************************************************************/
struct extra_type *extra_type_get(enum extra_type_id type, int subid)
{
  switch (type) {
  case EXTRA_SPECIAL:
    return extra_by_number(subid);
  case EXTRA_BASE:
    return extra_by_number(S_LAST + subid);
  case EXTRA_ROAD:
    return extra_by_number(S_LAST + game.control.num_base_types + subid);
  }

  return NULL;
}

/**************************************************************************
  Return the (translated) name of the extra type.
  You don't have to free the return pointer.
**************************************************************************/
const char *extra_name_translation(const struct extra_type *pextra)
{
  return name_translation(&pextra->name);
}

/**************************************************************************
  Return the (untranslated) rule name of the extra type.
  You don't have to free the return pointer.
**************************************************************************/
const char *extra_rule_name(const struct extra_type *pextra)
{
  return rule_name(&pextra->name);
}

/**************************************************************************
  Returns extra type matching rule name or NULL if there is no extra type
  with such name.
**************************************************************************/
struct extra_type *extra_type_by_rule_name(const char *name)
{
  const char *qs = Qn_(name);

  extra_type_iterate(pextra) {
    if (!fc_strcasecmp(extra_rule_name(pextra), qs)) {
      return pextra;
    }
  } extra_type_iterate_end;

  return NULL;
}

/**************************************************************************
  Returns extra type matching the translated name, or NULL if there is no
  extra type with that name.
**************************************************************************/
struct extra_type *extra_type_by_translated_name(const char *name)
{
  extra_type_iterate(pextra) {
    if (0 == strcmp(extra_name_translation(pextra), name)) {
      return pextra;
    }
  } extra_type_iterate_end;

  return NULL;
}

/**************************************************************************
  Returns extra type for given cause.
**************************************************************************/
struct extra_type *extra_type_by_cause(enum extra_cause cause)
{
  enum tile_special_type spe;

  switch(cause) {
  case EC_IRRIGATION:
    spe = S_IRRIGATION;
    break;
  case EC_MINE:
    spe = S_MINE;
    break;
  case EC_POLLUTION:
    spe = S_POLLUTION;
    break;
  case EC_HUT:
    spe = S_HUT;
    break;
  case EC_FARMLAND:
    spe = S_FARMLAND;
    break;
  case EC_FALLOUT:
    spe = S_FALLOUT;
    break;
  default:
    spe = S_LAST;
    break;
  }

  return extra_type_get(EXTRA_SPECIAL, spe);
}

/****************************************************************************
  Is there extra of the given type cardinally near tile?
****************************************************************************/
bool is_extra_card_near(const struct tile *ptile, const struct extra_type *pextra)
{
  cardinal_adjc_iterate(ptile, adjc_tile) {
    if (tile_has_extra(adjc_tile, pextra)) {
      return TRUE;
    }
  } cardinal_adjc_iterate_end;

  return FALSE;
}

/****************************************************************************
  Is there extra of the given type near tile?
****************************************************************************/
bool is_extra_near_tile(const struct tile *ptile, const struct extra_type *pextra)
{
  adjc_iterate(ptile, adjc_tile) {
    if (tile_has_extra(adjc_tile, pextra)) {
      return TRUE;
    }
  } adjc_iterate_end;

  return FALSE;
}
