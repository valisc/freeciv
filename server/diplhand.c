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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>

#include "diptreaty.h"
#include "events.h"
#include "fcintl.h"
#include "game.h"
#include "log.h"
#include "map.h"
#include "mem.h"
#include "packets.h"
#include "player.h"
#include "unit.h"

#include "citytools.h"
#include "cityturn.h"
#include "gamelog.h"
#include "maphand.h"
#include "plrhand.h"
#include "settlers.h"
#include "unittools.h"

#include "advdiplomacy.h"

#include "diplhand.h"

#define SPECLIST_TAG treaty
#define SPECLIST_TYPE struct Treaty
#include "speclist.h"

#define SPECLIST_TAG treaty
#define SPECLIST_TYPE struct Treaty
#include "speclist_c.h"

#define treaty_list_iterate(list, p) \
    TYPED_LIST_ITERATE(struct Treaty, list, p)
#define treaty_list_iterate_end  LIST_ITERATE_END

static struct treaty_list treaties;
static bool did_init_treaties;

/**************************************************************************
...
**************************************************************************/
struct Treaty *find_treaty(struct player *plr0, struct player *plr1)
{
  if (!did_init_treaties) {
    treaty_list_init(&treaties);
    did_init_treaties = TRUE;
  }

  treaty_list_iterate(treaties, ptreaty) {
    if ((ptreaty->plr0 == plr0 && ptreaty->plr1 == plr1) ||
	(ptreaty->plr0 == plr1 && ptreaty->plr1 == plr0)) {
      return ptreaty;
    }
  } treaty_list_iterate_end;

  return NULL;
}

/**************************************************************************
pplayer clicked the accept button. If he accepted the treaty we check the
clauses. If both players have now accepted the treaty we execute the agreed
clauses.
**************************************************************************/
void handle_diplomacy_accept_treaty_req(struct player *pplayer,
					int counterpart)
{
  struct Treaty *ptreaty;
  struct player *pother;
  bool *player_accept, *other_accept;

  if (!is_valid_player_id(counterpart) || pplayer->player_no == counterpart) {
    return;
  }

  pother = get_player(counterpart);
  ptreaty = find_treaty(pplayer, pother);

  if (!ptreaty) {
    return;
  }

  if (ptreaty->plr0 == pplayer) {
    player_accept = &ptreaty->accept0;
    other_accept = &ptreaty->accept1;
  } else {
    player_accept = &ptreaty->accept1;
    other_accept = &ptreaty->accept0;
  }

  if (!*player_accept) {	/* Tries to accept. */

    /* Check that player who accepts can keep what (s)he promises. */

    clause_list_iterate(ptreaty->clauses, pclause) {
      struct city *pcity = NULL;

      if (pclause->from == pplayer) {
	switch(pclause->type) {
	case CLAUSE_ADVANCE:
          if (!tech_is_available(pother, pclause->value)) {
	    /* It is impossible to give a technology to a civilization that
	     * can never possess it (the client should enforce this). */
	    freelog(LOG_ERROR, "Treaty: The %s can't have tech %s",
                    get_nation_name_plural(pother->nation),
                    advances[pclause->value].name);
	    notify_player(pplayer,
                          _("Game: The %s can't accept %s."),
                          get_nation_name_plural(pother->nation),
                          advances[pclause->value].name);
	    return;
          }
	  if (get_invention(pplayer, pclause->value) != TECH_KNOWN) {
	    freelog(LOG_ERROR,
                    "The %s don't know tech %s, but try to give it to the %s.",
		    get_nation_name_plural(pplayer->nation),
		    advances[pclause->value].name,
		    get_nation_name_plural(pother->nation));
	    notify_player(pplayer,
			  _("Game: You don't have tech %s, you can't accept treaty."),
			  advances[pclause->value].name);
	    return;
	  }
	  break;
	case CLAUSE_CITY:
	  pcity = find_city_by_id(pclause->value);
	  if (!pcity) { /* Can't find out cityname any more. */
	    notify_player(pplayer,
			  _("City you are trying to give no longer exists, "
			    "you can't accept treaty."));
	    return;
	  }
	  if (pcity->owner != pplayer->player_no) {
	    notify_player(pplayer,
			  _("You are not owner of %s, you can't accept treaty."),
			  pcity->name);
	    return;
	  }
	  if (city_got_building(pcity, B_PALACE)) {
	    notify_player(pplayer,
			  _("Game: Your capital (%s) is requested, "
			    "you can't accept treaty."),
			  pcity->name);
	    return;
	  }
	  break;
	case CLAUSE_ALLIANCE:
          if (!pplayer_can_ally(pplayer, pother)) {
	    notify_player(pplayer,
			  _("Game: You are at war with one of %s's "
			    "allies - an alliance with %s is impossible."),
			  pother->name, pother->name);
            return;
          }
          if (!pplayer_can_ally(pother, pplayer)) {
	    notify_player(pplayer,
			  _("Game: %s is at war with one of your allies "
			    "- an alliance with %s is impossible."),
			  pother->name, pother->name);
            return;
          }
          break;
	case CLAUSE_GOLD:
	  if (pplayer->economic.gold < pclause->value) {
	    notify_player(pplayer,
			  _("Game: You don't have enough gold, "
			    "you can't accept treaty."));
	    return;
	  }
	  break;
	default:
	  ; /* nothing */
	}
      }
    } clause_list_iterate_end;
  }

  *player_accept = ! *player_accept;

  dlsend_packet_diplomacy_accept_treaty(&pplayer->connections,
					pother->player_no, *player_accept,
					*other_accept);
  dlsend_packet_diplomacy_accept_treaty(&pother->connections,
					pplayer->player_no, *other_accept,
					*player_accept);

  if (ptreaty->accept0 && ptreaty->accept1) {
    int nclauses = clause_list_size(&ptreaty->clauses);

    dlsend_packet_diplomacy_cancel_meeting(&pplayer->connections,
					   pother->player_no,
					   pplayer->player_no);
    dlsend_packet_diplomacy_cancel_meeting(&pother->connections,
					   pplayer->player_no,
 					   pplayer->player_no);

    notify_player(pplayer,
		  PL_("Game: A treaty containing %d clause was agreed upon.",
		      "Game: A treaty containing %d clauses was agreed upon.",
		      nclauses),
		  nclauses);
    notify_player(pother,
		  PL_("Game: A treaty containing %d clause was agreed upon.",
		      "Game: A treaty containing %d clauses was agreed upon.",
		      nclauses),
		  nclauses);
    gamelog(GAMELOG_TREATY, _("%s and %s agree to a treaty"),
	    get_nation_name_plural(pplayer->nation),
	    get_nation_name_plural(pother->nation));

    /* Check that one who accepted treaty earlier still have everything
       (s)he promised to give. */

    clause_list_iterate(ptreaty->clauses, pclause) {
      struct city *pcity;
      if (pclause->from == pother) {
	switch (pclause->type) {
	case CLAUSE_CITY:
	  pcity = find_city_by_id(pclause->value);
	  if (!pcity) { /* Can't find out cityname any more. */
	    notify_player(pplayer,
			  _("Game: One of the cities %s is giving away is destroyed! "
			    "Treaty canceled!"),
			  get_nation_name_plural(pother->nation));
	    notify_player(pother,
			  _("Game: One of the cities %s is giving away is destroyed! "
			    "Treaty canceled!"),
			  get_nation_name_plural(pother->nation));
	    goto cleanup;
	  }
	  if (pcity->owner != pother->player_no) {
	    notify_player(pplayer,
			  _("Game: The %s no longer control %s! "
			    "Treaty canceled!"),
			  get_nation_name_plural(pother->nation),
			  pcity->name);
	    notify_player(pother,
			  _("Game: The %s no longer control %s! "
			    "Treaty canceled!"),
			  get_nation_name_plural(pother->nation),
			  pcity->name);
	    goto cleanup;
	  }
	  if (city_got_building(pcity, B_PALACE)) {
	    notify_player(pother,
			  _("Game: Your capital (%s) is requested, "
			    "you can't accept treaty."), pcity->name);
	    goto cleanup;
	  }

	  break;
	case CLAUSE_TEAM:
          /* Limitation: Only for teams */
          if (pother->team == TEAM_NONE || pother->team != pplayer->team) {
            freelog(LOG_ERROR, "Attempted to make team in-game!");
            goto cleanup;
          }
          break;
	case CLAUSE_ALLIANCE:
          /* We need to recheck this way since things might have
           * changed. */
          if (!pplayer_can_ally(pother, pplayer)) {
	    notify_player(pplayer,
			  _("Game: %s is at war with one of your "
			    "allies - an alliance with %s is impossible."),
			  pother->name, pother->name);
	    notify_player(pother,
			  _("Game: You are at war with one of %s's "
			    "allies - an alliance with %s is impossible."),
			  pplayer->name, pplayer->name);
	    goto cleanup;
          }
          break;
	case CLAUSE_GOLD:
	  if (pother->economic.gold < pclause->value) {
	    notify_player(pplayer,
			  _("Game: The %s don't have the promised amount "
			    "of gold! Treaty canceled!"),
			  get_nation_name_plural(pother->nation));
	    notify_player(pother,
			  _("Game: The %s don't have the promised amount "
			    "of gold! Treaty canceled!"),
			  get_nation_name_plural(pother->nation));
	    goto cleanup;
	  }
	  break;
	default:
	  ; /* nothing */
	}
      }
    } clause_list_iterate_end;

    if (pplayer->ai.control) {
      ai_treaty_accepted(pplayer, pother, ptreaty);
    }
    if (pother->ai.control) {
      ai_treaty_accepted(pother, pplayer, ptreaty);
    }

    clause_list_iterate(ptreaty->clauses, pclause) {
      struct player *pgiver = pclause->from;
      struct player *pdest = (pplayer == pgiver) ? pother : pplayer;

      switch (pclause->type) {
      case CLAUSE_ADVANCE:
        /* It is possible that two players open the diplomacy dialog
         * and try to give us the same tech at the same time. This
         * should be handled discreetly instead of giving a core dump. */
        if (get_invention(pdest, pclause->value) == TECH_KNOWN) {
	  freelog(LOG_VERBOSE,
                  "The %s already know tech %s, that %s want to give them.",
		  get_nation_name_plural(pdest->nation),
		  advances[pclause->value].name,
		  get_nation_name_plural(pgiver->nation));
          break;
        }
	notify_player_ex(pdest, -1, -1, E_TECH_GAIN,
			 _("Game: You are taught the knowledge of %s."),
			 get_tech_name(pdest, pclause->value));

	notify_embassies(pdest, pgiver,
			 _("Game: The %s have acquired %s from the %s."),
			 get_nation_name_plural(pdest->nation),
			 get_tech_name(pdest, pclause->value),
			 get_nation_name_plural(pgiver->nation));

	gamelog(GAMELOG_TECH, _("%s acquire %s (Treaty) from %s"),
		get_nation_name_plural(pdest->nation),
		get_tech_name(pdest, pclause->value),
		get_nation_name_plural(pgiver->nation));

	do_dipl_cost(pdest);

	found_new_tech(pdest, pclause->value, FALSE, TRUE, A_NONE);
	break;
      case CLAUSE_GOLD:
	notify_player(pdest, _("Game: You get %d gold."), pclause->value);
	pgiver->economic.gold -= pclause->value;
	pdest->economic.gold += pclause->value;
	gamelog(GAMELOG_TECH, _("%s acquire %d gold from %s"),
		get_nation_name_plural(pdest->nation), pclause->value, 
		get_nation_name_plural(pgiver->nation));
	break;
      case CLAUSE_MAP:
	give_map_from_player_to_player(pgiver, pdest);
	notify_player(pdest, _("Game: You receive %s's worldmap."),
		      pgiver->name);
	break;
      case CLAUSE_SEAMAP:
	give_seamap_from_player_to_player(pgiver, pdest);
	notify_player(pdest, _("Game: You receive %s's seamap."),
		      pgiver->name);
	break;
      case CLAUSE_CITY:
	{
	  struct city *pcity = find_city_by_id(pclause->value);

	  if (!pcity) {
	    freelog(LOG_NORMAL,
		    "Treaty city id %d not found - skipping clause.",
		    pclause->value);
	    break;
	  }

	  notify_player_ex(pdest, pcity->x, pcity->y, E_CITY_TRANSFER,
			   _("Game: You receive city of %s from %s."),
			   pcity->name, pgiver->name);

	  notify_player_ex(pgiver, pcity->x, pcity->y, E_CITY_LOST,
			   _("Game: You give city of %s to %s."),
			   pcity->name, pdest->name);

          gamelog(GAMELOG_TECH, _("%s acquire the city %s from %s"),
		  get_nation_name_plural(pdest->nation), pcity->name, 
		  get_nation_name_plural(pgiver->nation));
	  transfer_city(pdest, pcity, -1, TRUE, TRUE, FALSE);
	  break;
	}
      case CLAUSE_CEASEFIRE:
	pgiver->diplstates[pdest->player_no].type=DS_CEASEFIRE;
	pgiver->diplstates[pdest->player_no].turns_left=16;
	pdest->diplstates[pgiver->player_no].type=DS_CEASEFIRE;
	pdest->diplstates[pgiver->player_no].turns_left=16;
	notify_player_ex(pgiver, -1, -1, E_TREATY_CEASEFIRE,
			 _("Game: You agree on a cease-fire with %s."),
			 pdest->name);
	notify_player_ex(pdest, -1, -1, E_TREATY_CEASEFIRE,
			 _("Game: You agree on a cease-fire with %s."),
			 pgiver->name);
	check_city_workers(pplayer);
	check_city_workers(pother);
	break;
      case CLAUSE_PEACE:
	pgiver->diplstates[pdest->player_no].type=DS_PEACE;
	pdest->diplstates[pgiver->player_no].type=DS_PEACE;
	notify_player_ex(pgiver, -1, -1, E_TREATY_PEACE,
			 _("Game: You agree on a peace treaty with %s."),
			 pdest->name);
	notify_player_ex(pdest, -1, -1, E_TREATY_PEACE,
			 _("Game: You agree on a peace treaty with %s."),
			 pgiver->name);
	check_city_workers(pplayer);
	check_city_workers(pother);
	break;
      case CLAUSE_ALLIANCE:
	pgiver->diplstates[pdest->player_no].type=DS_ALLIANCE;
	pdest->diplstates[pgiver->player_no].type=DS_ALLIANCE;
	notify_player_ex(pgiver, -1, -1, E_TREATY_ALLIANCE,
			 _("Game: You agree on an alliance with %s."),
			 pdest->name);
	notify_player_ex(pdest, -1, -1, E_TREATY_ALLIANCE,
			 _("Game: You agree on an alliance with %s."),
			 pgiver->name);
	gamelog(GAMELOG_TECH, _("%s agree on an alliance with %s"),
		get_nation_name_plural(pdest->nation), 
		get_nation_name_plural(pgiver->nation));
	check_city_workers(pplayer);
	check_city_workers(pother);
	break;
      case CLAUSE_TEAM:
        notify_player_ex(pgiver, -1, -1, E_TREATY_ALLIANCE,
                         _("Game: You start a research pool with %s."),
                         pdest->name);
        notify_player_ex(pdest, -1, -1, E_TREATY_ALLIANCE,
                         _("Game: You start a research pool with %s."),
                         pgiver->name);
        /* We must share and average research */
        {
          int average = (pplayer->research.bulbs_researched
                         + pother->research.bulbs_researched) / 2;
          int average2 = (pplayer->research.bulbs_researched_before
                          + pother->research.bulbs_researched_before) / 2;
          int tech;
 
          for (tech = 0; tech < A_LAST; tech++) {
            if (!tech_exists(tech)) {
              continue;
            }
            if (get_invention(pplayer, tech) != TECH_KNOWN
                && get_invention(pother, tech) == TECH_KNOWN) {
              found_new_tech(pplayer, tech, FALSE, TRUE, A_NONE);
            }
            if (get_invention(pother, tech) != TECH_KNOWN
                && get_invention(pplayer, tech) == TECH_KNOWN) {
              found_new_tech(pother, tech, FALSE, TRUE, A_NONE);
            }
          }
          pplayer->research.bulbs_researched = average;
          pother->research.bulbs_researched = average;
          pother->research.researching = pplayer->research.researching;
          pother->research.changed_from = pplayer->research.changed_from;
          pplayer->research.bulbs_researched_before = average2;
          pother->research.bulbs_researched_before = average2;
          pother->ai.tech_goal = pplayer->ai.tech_goal;
        }
        pgiver->diplstates[pdest->player_no].type = DS_TEAM;
        pdest->diplstates[pgiver->player_no].type = DS_TEAM;
        check_city_workers(pplayer);
        check_city_workers(pother);
        break;
      case CLAUSE_VISION:
	give_shared_vision(pgiver, pdest);
	notify_player_ex(pgiver, -1, -1, E_TREATY_SHARED_VISION,
			 _("Game: You give shared vision to %s."),
			 pdest->name);
	notify_player_ex(pdest, -1, -1, E_TREATY_SHARED_VISION,
			 _("Game: %s gives you shared vision."),
			 pgiver->name);
	gamelog(GAMELOG_TECH, _("%s share vision with %s"),
		get_nation_name_plural(pdest->nation), 
		get_nation_name_plural(pgiver->nation));
	break;
      case CLAUSE_LAST:
        freelog(LOG_ERROR, "Received bad clause type");
        break;
      }

    } clause_list_iterate_end;
  cleanup:
    treaty_list_unlink(&treaties, ptreaty);
    free(ptreaty);
    send_player_info(pplayer, NULL);
    send_player_info(pother, NULL);
  }
}

/****************************************************************************
  Create an embassy. pplayer gets an embassy with aplayer.
****************************************************************************/
void establish_embassy(struct player *pplayer, struct player *aplayer)
{
  /* Establish the embassy. */
  pplayer->embassy |= (1 << aplayer->player_no);
  send_player_info(pplayer, pplayer);
  send_player_info(pplayer, aplayer);  /* update player dialog with embassy */
  send_player_info(aplayer, pplayer);  /* INFO_EMBASSY level info */
}

/**************************************************************************
...
**************************************************************************/
void handle_diplomacy_remove_clause_req(struct player *pplayer,
					int counterpart, int giver,
					enum clause_type type, int value)
{
  struct Treaty *ptreaty;
  struct player *pgiver, *pother;

  if (!is_valid_player_id(counterpart) || pplayer->player_no == counterpart
      || !is_valid_player_id(giver)) {
    return;
  }

  pother = get_player(counterpart);
  pgiver = get_player(giver);

  if (pgiver != pplayer && pgiver != pother) {
    return;
  }
  
  ptreaty = find_treaty(pplayer, pother);

  if (ptreaty && remove_clause(ptreaty, pgiver, type, value)) {
    dlsend_packet_diplomacy_remove_clause(&pplayer->connections,
					  pother->player_no, giver, type,
					  value);
    dlsend_packet_diplomacy_remove_clause(&pother->connections,
					  pplayer->player_no, giver, type,
					  value);
    if (pplayer->ai.control) {
      ai_treaty_evaluate(pplayer, pother, ptreaty);
    }
    if (pother->ai.control) {
      ai_treaty_evaluate(pother, pplayer, ptreaty);
    }
  }
}

/**************************************************************************
...
**************************************************************************/
void handle_diplomacy_create_clause_req(struct player *pplayer,
					int counterpart, int giver,
					enum clause_type type, int value)
{
  struct Treaty *ptreaty;
  struct player *pgiver, *pother;

  if (!is_valid_player_id(counterpart) || pplayer->player_no == counterpart
      || !is_valid_player_id(giver)) {
    return;
  }

  pother = get_player(counterpart);
  pgiver = get_player(giver);

  if (pgiver != pplayer && pgiver != pother) {
    return;
  }

  ptreaty = find_treaty(pplayer, pother);

  if (ptreaty && add_clause(ptreaty, pgiver, type, value)) {
    /* 
     * If we are trading cities, then it is possible that the
     * dest is unaware of it's existence.  We have 2 choices,
     * forbid it, or lighten that area.  If we assume that
     * the giver knows what they are doing, then 2. is the
     * most powerful option - I'll choose that for now.
     *                           - Kris Bubendorfer
     */
    if (type == CLAUSE_CITY) {
      struct city *pcity = find_city_by_id(value);

      if (pcity && !map_is_known_and_seen(pcity->x, pcity->y, pother))
	give_citymap_from_player_to_player(pcity, pplayer, pother);
    }

    dlsend_packet_diplomacy_create_clause(&pplayer->connections,
					  pother->player_no, giver, type,
					  value);
    dlsend_packet_diplomacy_create_clause(&pother->connections,
					  pplayer->player_no, giver, type,
					  value);
    if (pplayer->ai.control) {
      ai_treaty_evaluate(pplayer, pother, ptreaty);
    }
    if (pother->ai.control) {
      ai_treaty_evaluate(pother, pplayer, ptreaty);
    }
  }
}

/**************************************************************************
...
**************************************************************************/
static void really_diplomacy_cancel_meeting(struct player *pplayer,
					    struct player *pother)
{
  struct Treaty *ptreaty = find_treaty(pplayer, pother);

  if (ptreaty) {
    dlsend_packet_diplomacy_cancel_meeting(&pother->connections,
					   pplayer->player_no,
					   pplayer->player_no);
    notify_player(pother, _("Game: %s canceled the meeting!"), 
		  pplayer->name);
    /* Need to send to pplayer too, for multi-connects: */
    dlsend_packet_diplomacy_cancel_meeting(&pplayer->connections,
					   pother->player_no,
					   pplayer->player_no);
    notify_player(pplayer, _("Game: Meeting with %s canceled."), 
		  pother->name);
    treaty_list_unlink(&treaties, ptreaty);
    free(ptreaty);
  }
}

/**************************************************************************
...
**************************************************************************/
void handle_diplomacy_cancel_meeting_req(struct player *pplayer,
					 int counterpart)
{
  if (!is_valid_player_id(counterpart) || pplayer->player_no == counterpart) {
    return;
  }

  really_diplomacy_cancel_meeting(pplayer, get_player(counterpart));
}

/**************************************************************************
...
**************************************************************************/
void handle_diplomacy_init_meeting_req(struct player *pplayer,
				       int counterpart)
{
  struct player *pother;

  if (!is_valid_player_id(counterpart) || pplayer->player_no == counterpart) {
    return;
  }

  pother = get_player(counterpart);

  if (find_treaty(pplayer, pother)) {
    return;
  }

  if (is_barbarian(pplayer) || is_barbarian(pother)) {
    notify_player(pplayer, _("Your diplomatic envoy was decapitated!"));
    return;
  }

  if (could_meet_with_player(pplayer, pother)) {
    struct Treaty *ptreaty;

    ptreaty = fc_malloc(sizeof(struct Treaty));
    init_treaty(ptreaty, pplayer, pother);
    treaty_list_insert(&treaties, ptreaty);

    dlsend_packet_diplomacy_init_meeting(&pplayer->connections,
					 pother->player_no,
					 pplayer->player_no);
    dlsend_packet_diplomacy_init_meeting(&pother->connections,
					 pplayer->player_no,
					 pplayer->player_no);
  }
}

/**************************************************************************
  Send information on any on-going diplomatic meetings for connection's
  player.  (For re-connection in multi-connect case.)
**************************************************************************/
void send_diplomatic_meetings(struct connection *dest)
{
  struct player *pplayer = dest->player;

  if (!pplayer) {
    return;
  }
  players_iterate(other_player) {
    struct Treaty *ptreaty = find_treaty(pplayer, other_player);

    if (ptreaty) {
      dsend_packet_diplomacy_init_meeting(dest, pplayer->player_no,
					  other_player->player_no);
      clause_list_iterate(ptreaty->clauses, pclause) {
	dsend_packet_diplomacy_create_clause(dest, pplayer->player_no,
					     other_player->player_no,
					     pclause->type,
					     pclause->from->player_no);
      } clause_list_iterate_end;
    }
  } players_iterate_end;
}

/**************************************************************************
...
**************************************************************************/
void cancel_all_meetings(struct player *pplayer)
{
  players_iterate(pplayer2) {
    if (find_treaty(pplayer, pplayer2)) {
      really_diplomacy_cancel_meeting(pplayer, pplayer2);
    }
  } players_iterate_end;
}
