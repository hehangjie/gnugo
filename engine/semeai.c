/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
 * This is GNU GO, a Go program. Contact gnugo@gnu.org, or see   *
 * http://www.gnu.org/software/gnugo/ for more information.      *
 *                                                               *
 * Copyright 1999, 2000, 2001 by the Free Software Foundation.   *
 *                                                               *
 * This program is free software; you can redistribute it and/or *
 * modify it under the terms of the GNU General Public License   *
 * as published by the Free Software Foundation - version 2.     *
 *                                                               *
 * This program is distributed in the hope that it will be       *
 * useful, but WITHOUT ANY WARRANTY; without even the implied    *
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR       *
 * PURPOSE.  See the GNU General Public License in file COPYING  *
 * for more details.                                             *
 *                                                               *
 * You should have received a copy of the GNU General Public     *
 * License along with this program; if not, write to the Free    *
 * Software Foundation, Inc., 59 Temple Place - Suite 330,       *
 * Boston, MA 02111, USA.                                        *
\* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */


#include <stdio.h>
#include <stdlib.h>

#include "liberty.h"

#define INFINITY 1000

static void analyze_semeai(int my_dragon, int your_dragon);
static void add_appropriate_semeai_moves(int move, 
					 int my_dragon, int your_dragon, 
					 int my_status, int your_status,
					 int margin_of_safety);
static void small_semeai_analyzer(int str1, int str2);

/* semeai() searches for pairs of dragons of opposite color which
 * have safety DEAD. If such a pair is found, analyze_semeai is
 * called to determine which dragon will prevail in a semeai, and
 * whether a move now will make a difference in the outcome. The
 * dragon statuses are revised, and if a move now will make a
 * difference in the outcome, an owl reason is generated.
 */

void
semeai(int color)
{
  int d1, d2;
  int k;
  int apos = NO_MOVE;
  int bpos = NO_MOVE;
  int other = OTHER_COLOR(color);

  TRACE("Semeai Player is THINKING for %s!\n", 
	color_to_string(color));

  for (d1 = 0; d1 < number_of_dragons; d1++) {
    if (DRAGON(d1).color != color
	|| (DRAGON(d1).matcher_status != DEAD
	    && DRAGON(d1).matcher_status != CRITICAL))
      continue;

    for (k = 0; k < dragon2[d1].neighbors; k++) {
      d2 = dragon2[d1].adjacent[k];
      if (DRAGON(d2).color != other
	  || (DRAGON(d2).matcher_status != DEAD
	      && DRAGON(d2).matcher_status != CRITICAL))
	continue;

      /* Dragons d1 (our) and d2 (opponent) are adjacent and both DEAD
       * or CRITICAL.
       */
      apos = DRAGON(d1).origin;
      bpos = DRAGON(d2).origin;

      /* Ignore inessential worms or dragons */
      if (worm[apos].inessential 
	  || DRAGON2(apos).safety == INESSENTIAL
	  || worm[bpos].inessential 
	  || DRAGON2(bpos).safety == INESSENTIAL)
	continue;

      analyze_semeai(apos, bpos);      
    }
  }
}

/* liberty_of_dragon(pos, origin) returns true if the vertex at (pos) is a
 * liberty of the dragon with origin at (origin).
 */

static int 
liberty_of_dragon(int pos, int origin)
{
  if (pos == NO_MOVE)
    return 0;

  if (board[pos] != EMPTY)
    return 0;

  if ((ON_BOARD(SOUTH(pos))    && dragon[SOUTH(pos)].origin == origin)
      || (ON_BOARD(WEST(pos))  && dragon[WEST(pos)].origin == origin)
      || (ON_BOARD(NORTH(pos)) && dragon[NORTH(pos)].origin == origin)
      || (ON_BOARD(EAST(pos))  && dragon[EAST(pos)].origin == origin))
    return 1;

  return 0;
}

static void
update_status(int dr, int new_status, int new_safety)
{
  int pos;
  
  DEBUG(DEBUG_SEMEAI, "Changing matcher_status of %1m from %s to %s.\n", dr,
	status_to_string(dragon[dr].matcher_status),
	status_to_string(new_status));
  DEBUG(DEBUG_SEMEAI, "Changing safety of %1m from %s to %s.\n", dr,
	safety_to_string(DRAGON2(dr).safety), safety_to_string(new_safety));
  
  for (pos = BOARDMIN; pos < BOARDMAX; pos++)
    if (IS_STONE(board[pos]) && is_same_dragon(dr, pos))
      dragon[pos].matcher_status = new_status;

  DRAGON2(dr).safety = ALIVE_IN_SEKI;
}

/* analyzes a pair of adjacent dragons which are 
 * DEAD or CRITICAL.
 */
static void
analyze_semeai(int my_dragon, int your_dragon)
{
  /* We start liberty counts at 1 since we will be subtracting
   * the number of worms. */
  int mylibs = 1, yourlibs = 1, commonlibs = 0; 
  int yourlibi = -1, yourlibj = -1;
  int commonlibi = -1, commonlibj = -1;
  int color = board[my_dragon];
  int i, j;
  int m, n;
  int my_status = UNKNOWN;
  int your_status = UNKNOWN;
  int margin_of_safety = 0;
  int owl_code_sufficient = 0;
  
  DEBUG(DEBUG_SEMEAI, "semeai_analyzer: %1m (me) vs %1m (them)\n",
	my_dragon, your_dragon);

  /* If both dragons are owl-critical, or my dragon is owl-critical
   * and your dragon is owl-dead, and the attack point for my dragon
   * owl_does_defend your dragon, add another owl defend move reason
   * and possibly change the owl status of your dragon to critical.
   *
   * Correction: We can't add an owl defense move reason here because
   * this would be a defense of an opponent dragon.
   */
  if (dragon[my_dragon].owl_status == CRITICAL
      && (dragon[your_dragon].owl_status == CRITICAL
	  || dragon[your_dragon].owl_status == DEAD)) {
    if (dragon[your_dragon].owl_defense_point
	== dragon[my_dragon].owl_attack_point)
      return;
    if (dragon[my_dragon].owl_attack_point != NO_MOVE
	&& owl_does_defend(dragon[my_dragon].owl_attack_point, your_dragon)) {
#if 0
      add_owl_defense_move(dragon[my_dragon].owl_attack_point, your_dragon,
			   WIN);
      DEBUG(DEBUG_SEMEAI, "added owl defense of %1m at %1m\n",
	    your_dragon, dragon[my_dragon].owl_defense_point);
#endif
      if (dragon[your_dragon].owl_status == DEAD) {
	for (m = 0; m < board_size; m++)
	  for (n = 0; n < board_size; n++) {
	    int pos = POS(m, n);
	    if (board[pos] == board[your_dragon]
		&& is_same_dragon(pos, your_dragon)) {
	      dragon[pos].owl_status = CRITICAL;
	      dragon[pos].matcher_status = CRITICAL;
	    }
	  }
	DEBUG(DEBUG_SEMEAI,
	      "changed owl_status and matcher_status of %1m to CRITICAL\n",
	      your_dragon);
      }
      owl_code_sufficient = 1;
    }
  }

  /* If both dragons are owl-critical, and the defense point for my
   * dragon owl_does_attack your dragon, add another owl attack move
   * reason.
   */
  if (dragon[my_dragon].owl_status == CRITICAL
      && dragon[your_dragon].owl_status == CRITICAL) {
    if (dragon[your_dragon].owl_attack_point
	== dragon[my_dragon].owl_defense_point)
      return;
    if (dragon[my_dragon].owl_defense_point != NO_MOVE
	&& owl_does_attack(dragon[my_dragon].owl_defense_point, your_dragon)) {
      add_owl_attack_move(dragon[my_dragon].owl_defense_point, your_dragon,
			  WIN);
      DEBUG(DEBUG_SEMEAI, "added owl attack of %1m at %1m\n",
	    your_dragon, dragon[my_dragon].owl_defense_point);
      owl_code_sufficient = 1;
    }
  }

  /* If both dragons are owl-critical, or your dragon is owl-critical
   * and my dragon is owl-dead, and the attack point for your dragon
   * owl_does_defend my dragon, add another owl defense move reason
   * and possibly change the owl status of my dragon to critical.
   */
  if ((dragon[my_dragon].owl_status == CRITICAL
       || dragon[my_dragon].owl_status == DEAD)
      && dragon[your_dragon].owl_status == CRITICAL) {
    if (dragon[your_dragon].owl_attack_point
	== dragon[my_dragon].owl_defense_point)
      return;
    if (dragon[your_dragon].owl_attack_point != NO_MOVE
	&& owl_does_defend(dragon[your_dragon].owl_attack_point, my_dragon)) {
      add_owl_defense_move(dragon[your_dragon].owl_attack_point, my_dragon,
			   WIN);
      DEBUG(DEBUG_SEMEAI, "added owl defense of %1m at %1m\n",
	    my_dragon, dragon[your_dragon].owl_attack_point);
      if (dragon[my_dragon].owl_status == DEAD) {
	for (m = 0; m < board_size; m++)
	  for (n = 0; n < board_size; n++) {
	    int pos = POS(m, n);
	    if (board[pos] == board[my_dragon]
		&& is_same_dragon(pos, my_dragon)) {
	      dragon[pos].owl_status = CRITICAL;
	      dragon[pos].matcher_status = CRITICAL;
	    }
	  }
	DEBUG(DEBUG_SEMEAI,
	      "changed owl_status and matcher_status of %1m to CRITICAL\n",
	      my_dragon);
      }
      owl_code_sufficient = 1;
    }
  }

  /* If both dragons are owl-critical, and the defense point for your
   * dragon owl_does_attack my dragon, add another owl attack move
   * reason.
   *
   * Correction: We can't add an owl attack move reason here because
   * this would be an attack on our own dragon.
   */
  if (dragon[my_dragon].owl_status == CRITICAL
      && dragon[your_dragon].owl_status == CRITICAL) {
    if (dragon[your_dragon].owl_defense_point
	== dragon[my_dragon].owl_attack_point)
      return;
    if (dragon[your_dragon].owl_defense_point != NO_MOVE
	&& owl_does_attack(dragon[your_dragon].owl_defense_point, my_dragon)) {
#if 0
      add_owl_attack_move(dragon[your_dragon].owl_defense_point, my_dragon,
			  WIN);
      DEBUG(DEBUG_SEMEAI, "added owl attack of %1m at %1m\n",
	    my_dragon, dragon[your_dragon].owl_attack_point);
#endif
      owl_code_sufficient = 1;
    }
  }

  /* If the owl code was able to resolve the semeai, exit. */
  if (owl_code_sufficient) {
    DEBUG(DEBUG_SEMEAI, "...owl code sufficient to resolve semeai, exiting\n");
    return;
  }


  /* The semeai module is prone to errors since semeai cannot
   * really be handled by static analysis. It is really only needed
   * when the dragons have many liberties since tight situations
   * can be handled by the tactical reading code. Thus we exclude
   * dragon pairs where either has a tactically DEAD or CRITICAL
   * string which is adjacent to the other dragon which is owl
   * substantial.
   */
  for (m = 0; m < board_size; m++)
    for (n = 0; n < board_size; n++) {
      int pos = POS(m, n);
      if (worm[pos].origin == pos
	  && worm[pos].attack_codes[0] == WIN)
	if (dragon[pos].origin == my_dragon
	    || dragon[pos].origin == your_dragon) {
	  int adj;
	  int adjs[MAXCHAIN];
	  int r;
	  
	  adj = chainlinks(pos, adjs);
	  for (r = 0; r < adj; r++) {
	    int cpos = adjs[r];
	    if (dragon[cpos].origin == my_dragon
		|| dragon[cpos].origin == your_dragon)
	      if (owl_substantial(pos)) {
		DEBUG(DEBUG_SEMEAI, "...tactical situation detected, exiting\n");
		return;
	      }
	  }
	}
    }
  
  
  /* Mark the dragons as involved in semeai */
  for (i = 0; i < board_size; i++)
    for (j = 0; j < board_size; j++) {
      int pos = POS(i, j);
      if (is_same_dragon(pos, my_dragon)
	  || is_same_dragon(pos, your_dragon))
	DRAGON2(pos).semeai = 1;
    }
  
  /* First we try to determine the number of liberties of each
   * dragon, and the number of common liberties. We subtract
   * 1 minus the number of worms of the dragon from the liberty
   * count, since if a dragon has several worms, a move may
   * have to be invested in connecting them. At the same time
   * we try to find a liberty of the opponent's dragon, and a
   * common liberty, for future reference.
   */
  for (i = 0; i < board_size; i++)
    for (j = 0; j < board_size; j++) {
      int pos = POS(i, j);
      if (board[pos]
	  && worm[pos].origin == pos) {
	if (is_same_dragon(pos, my_dragon))
	  mylibs--;
	if (is_same_dragon(pos, your_dragon))
	  yourlibs--;
      }
      else if (board[pos] == EMPTY) {
	if (liberty_of_dragon(pos, your_dragon)) {
	  yourlibs ++;
	  if (liberty_of_dragon(pos, my_dragon)) {
	    commonlibs++;
	    mylibs++;
	    commonlibi = i;
	    commonlibj = j;
	  }
	  else {
	    yourlibi = i;
	    yourlibj = j;
	  }
	}
	else if (liberty_of_dragon(pos, my_dragon))
	  mylibs++;
      }
    }
  /* We add 1 to the
   * number of liberties of an owl critical dragon if the point
   * of attack is not a liberty of the dragon, since a move
   * may have to be invested in attacking it.
   */

  if (dragon[my_dragon].owl_status == CRITICAL
      && !liberty_of_string(dragon[my_dragon].owl_attack_point, my_dragon))
    mylibs++;
  
  if (dragon[your_dragon].owl_status == CRITICAL
      && !liberty_of_string(dragon[your_dragon].owl_attack_point, your_dragon))
    yourlibs++;
  
  /* Now we compute the statuses which result from a
   * naive comparison of the number of liberties. There
   * is some uncertainty in these calculations, so we
   * must exercise caution in applying the results.
   *
   * RULES FOR PLAYING SEMEAI. Let M be the number of liberties
   * of my group, excluding common liberties; let Y be the
   * number of liberties of your group, excluding common
   * liberties; and let C be the number of common liberties.
   * 
   *             If both groups have zero eyes:
   * 
   * (1)  If C=0 and M=Y, whoever moves first wins. CRITICAL.
   * (2)  If C=0 and M>Y, I win.
   * (3)  If C=0 and M<Y, you win.
   * (4)  If C>0 and M >= Y+C then your group is dead and mine is alive.
   * (5)  If C>0 and M = Y+C-1 then the situation is CRITICAL. 
   * (5a) If M=0, then Y=0 and C=1. Whoever moves first kills.
   * (5b) If M>0, then I can kill or you can make seki.
   * (6)  If M < Y+C-1 and Y < M+C-1 then the situation is seki.
   * (7)  If C>0 and Y=M+C-1 the situation is CRITICAL. 
   * (7a) If Y=0, then M=0 and C=1 as in (5). 
   * (7b) If Y>0, you can kill or I can make seki.
   * (8)  If C>0 and Y > M+C then your group is alive and mine is dead.
   *
   *              If both groups have one eye:
   *
   * In this case M > 0 and Y > 0.
   * 
   * (1) If M>C+Y then I win.
   * (2) If Y>C+M then you win.
   * (3) If C=0 and M=Y then whoever moves first kills. CRITICAL.
   * (4) If C>0 and M=C+Y then I can kill, you can make seki. CRITICAL.
   * (5) If C>0 and M<C+Y, Y<C+M, then the situation is seki. 
   * (6) If C>0 and Y=C+M, then you can kill, I can make seki. CRITICAL.
   *
   *            If I have an eye and you dont:
   * 
   * In this case, M > 0. This situation (me ari me nashi) can
   * never be seki. The common liberties must be filled by you,
   * making it difficult to win.
   * 
   * (1) If M+C>Y then I win.
   * (2) If M+C=Y then whoever moves first wins. CRITICAL.
   * (3) If M+C<Y then you win.
   *
   *            If you have an eye and I don't
   * 
   * In this case, Y > 0. 
   * 
   * (1) If Y+C>M you win.
   * (2) If Y+C=M whoever moves first wins. CRITICAL.
   * (3) If Y+C<M I win.  */

  if (DRAGON2(my_dragon).genus == 0
      && DRAGON2(your_dragon).genus == 0) {
    if (commonlibs == 0) {
      if (mylibs > yourlibs) {
	my_status = ALIVE;
	your_status = DEAD;
	margin_of_safety = mylibs - yourlibs;
      }
      else if (mylibs < yourlibs) {
	my_status = DEAD;
	your_status = ALIVE;
	margin_of_safety = yourlibs - mylibs;
      }
      else {
	my_status = CRITICAL;
	your_status = CRITICAL;
	margin_of_safety = 0;
      }
    }
    else if (mylibs == yourlibs + commonlibs - 1) {
      if (mylibs == 0) {
	my_status = CRITICAL;
	your_status = CRITICAL;
	margin_of_safety = 0;
      }
      else {
	/* I can kill, you can make seki */
	my_status = ALIVE;
	your_status = CRITICAL;
	margin_of_safety = 0;
      }
    }
    else if (mylibs < yourlibs + commonlibs - 1
	     && yourlibs < mylibs+commonlibs - 1) {
      /* Seki */
      my_status = ALIVE;
      your_status = ALIVE;
      margin_of_safety = INFINITY; 
    }
    else if (commonlibs > 0
	     && yourlibs == mylibs + commonlibs - 1) {
      if (yourlibs == 0) {
	my_status = CRITICAL;
	your_status = CRITICAL;
	margin_of_safety = 0;
      }
      else {
	/* you can kill, I can make seki */
	my_status = CRITICAL;
	your_status = ALIVE;
	margin_of_safety = 0;
      }
    }
    else if (commonlibs > 0
	     && yourlibs > mylibs + commonlibs) {
      my_status = DEAD;
      your_status = ALIVE;
      margin_of_safety = yourlibs - mylibs - commonlibs;
    }
  }
  if (DRAGON2(my_dragon).genus > 0
      && DRAGON2(your_dragon).genus > 0) {
    if (mylibs > yourlibs + commonlibs) {
      my_status = ALIVE;
      your_status = DEAD;
      margin_of_safety = mylibs - yourlibs - commonlibs;
    }
    else if (yourlibs > mylibs + commonlibs) {
      my_status = DEAD;
      your_status = ALIVE;
      margin_of_safety = yourlibs - mylibs - commonlibs;
    }
    else if (commonlibs == 0
	     && mylibs == yourlibs) {
      my_status = CRITICAL;
      your_status = CRITICAL;
      margin_of_safety = 0;
    }
    else if (commonlibs > 0
	     && mylibs == commonlibs + yourlibs) {
      my_status = ALIVE;
      your_status = CRITICAL;
      margin_of_safety = 0;
    }
    else if (commonlibs > 0
	     && mylibs < commonlibs + yourlibs
	     && yourlibs < commonlibs + mylibs) {
      /* seki */
      my_status = ALIVE;
      your_status = ALIVE;
      margin_of_safety = INFINITY;
    }
    else if (commonlibs > 0
	     && yourlibs == commonlibs + mylibs) {
      my_status = CRITICAL;
      your_status = ALIVE;
      margin_of_safety = 0;
    }
  }
  if (DRAGON2(my_dragon).genus > 0
      && DRAGON2(your_dragon).genus == 0) {
    if (mylibs > commonlibs + yourlibs) {
      my_status = ALIVE;
      your_status = DEAD;
      margin_of_safety = mylibs - commonlibs - yourlibs;
    }
    else if (mylibs + commonlibs == yourlibs) {
      my_status = CRITICAL;
      your_status = CRITICAL;
    }
    else if (mylibs + commonlibs < yourlibs) {
      my_status = DEAD;
      your_status = ALIVE;
      margin_of_safety = mylibs + commonlibs - yourlibs;
    }
  }
  if (DRAGON2(my_dragon).genus == 0
      && DRAGON2(your_dragon).genus > 0) {
    if (yourlibs + commonlibs > mylibs) {
      my_status = DEAD;
      your_status = ALIVE;
      margin_of_safety = yourlibs + commonlibs - mylibs;
    }
    else if (yourlibs + commonlibs == mylibs) {
      my_status = CRITICAL;
      your_status = CRITICAL;
      margin_of_safety = 0;
    }
    else if (yourlibs + commonlibs > mylibs) {
      my_status = DEAD;
      your_status = ALIVE;
      margin_of_safety = yourlibs - mylibs - commonlibs;
    }
  }
  
  /* Update matcher statuses */

  /* We do not want to change the matcher status of the friendly
   * dragon if the owl status is critical. If my_status==DEAD by
   * the preceeding heuristics but the owl code finds a way to
   * live, then we should by all means take it. On the other hand
   * if my_status==ALIVE we are alive by semeai, but as a matter
   * of "safety first" if the owl code finds a way to live we may
   * want to take it. So the matcher status is not changed.
   */
  
  if (dragon[my_dragon].owl_status != CRITICAL) {
    if (my_status == ALIVE)
      update_status(my_dragon, ALIVE, ALIVE_IN_SEKI);
    else if (my_status == CRITICAL)
      update_status(my_dragon, CRITICAL, CRITICAL);
    else if (my_status == DEAD)
      update_status(my_dragon, DEAD, DEAD);
  }

  if (your_status == ALIVE)
    update_status(your_dragon, ALIVE, ALIVE_IN_SEKI);
  else if (your_status == CRITICAL)
    update_status(your_dragon, CRITICAL, CRITICAL);
  else if (your_status == DEAD)
    update_status(your_dragon, DEAD, DEAD);
  
  /* Find the recommended semeai moves. In order of priority,
   * (1) We defend our dragon;
   * (2) We attack your dragon;
   * (3) If common liberties > 1, make an eye;
   * (4) If common liberties > 1, kill an eye;
   * (5) Fill a liberty of yours;
   * (6) Fill a common liberty.  */

  if (my_status == CRITICAL || your_status == CRITICAL) {
    int found_one = 0;
    if (dragon[my_dragon].owl_status == CRITICAL
	&& dragon[my_dragon].owl_defense_point != NO_MOVE)
      add_appropriate_semeai_moves(dragon[my_dragon].owl_defense_point,
				   my_dragon, your_dragon,
				   my_status, your_status, margin_of_safety);
    else if (dragon[your_dragon].owl_status == CRITICAL
	     && dragon[your_dragon].owl_attack_point != NO_MOVE)
      add_appropriate_semeai_moves(dragon[your_dragon].owl_attack_point,
				   my_dragon, your_dragon,
				   my_status, your_status, margin_of_safety);
    else if (commonlibs > 1) {
      if (DRAGON2(my_dragon).heyes > 0)
	add_appropriate_semeai_moves(DRAGON2(my_dragon).heye,
				     my_dragon, your_dragon,
				     my_status, your_status, margin_of_safety);
      if (DRAGON2(your_dragon).heyes > 0)
	add_appropriate_semeai_moves(DRAGON2(your_dragon).heye,
				     my_dragon, your_dragon,
				     my_status, your_status, margin_of_safety);
    }
    else {
      for (i = 0; i < board_size-1; i++)
	for (j = 0; j < board_size-1; j++) {
	  int pos = POS(i, j);
	  if (liberty_of_dragon(pos, your_dragon) 
	      && !liberty_of_dragon(pos, my_dragon)
	      && safe_move(pos, color)) {
	    /* add move reasons for EVERY outside liberty where we can
             * play safely. A move to win a semeai might not be a
             * safe move if it is inside the opponent's eyespace. 
             * However we hope that the reading code can analyze the
             * semeai in cases where every safe liberty has been filled.
	     */
	    add_appropriate_semeai_moves(pos, my_dragon, your_dragon,
					 my_status, your_status,
					 margin_of_safety);
	    found_one = 1;
	  }
	}
      if (!found_one) {
	/* No outside liberties --- look for common liberties.
	 * Filling a common liberty is usually bad but if our 
	 * heuristics are accurate, we should only reach this point 
	 * if we definitely have enough liberties to win. As a
	 * sanity check, we require filling a common liberty to
	 * be a safe move.
	 */
	for (i = 0; i < board_size-1; i++)
	  for (j = 0; j < board_size-1; j++) {
	    int pos = POS(i, j);
	    if (liberty_of_dragon(pos, your_dragon)
		&& safe_move(pos, color))
	      add_appropriate_semeai_moves(pos, my_dragon, your_dragon,
					   my_status, your_status,
					   margin_of_safety);
	  }
      }
    }
  }
}

/* Add those move reasons which are appropriate. */

static void
add_appropriate_semeai_moves(int move, int my_dragon, int your_dragon, 
			     int my_status, int your_status, 
			     int margin_of_safety)
{
  if (my_status == CRITICAL)
    add_semeai_move(move, my_dragon);
  else if (margin_of_safety==1)
    add_semeai_threat(move, my_dragon);
  if (your_status == CRITICAL)
      add_semeai_move(move, your_dragon);
  else if (margin_of_safety==1)
    add_semeai_threat(move, your_dragon);
}


/* revise_semeai(color) changes the status of any DEAD dragon of
 * OPPOSITE_COLOR(color) which occurs in a semeai to UNKNOWN.
 * It returns true if such a dragon is found.
 */

int
revise_semeai(int color)
{
  int pos;
  int found_one = 0;
  int other = OTHER_COLOR(color);

  gg_assert(dragon2 != NULL);

  for (pos = BOARDMIN; pos < BOARDMAX; pos++) {
    if (ON_BOARD(pos)
	&& DRAGON2(pos).semeai
	&& dragon[pos].matcher_status == DEAD
	&& dragon[pos].color == other) {
      found_one = 1;
      dragon[pos].matcher_status = UNKNOWN;
      if (dragon[pos].origin == pos)
	TRACE("revise_semeai: changed status of dragon %1m from DEAD to UNKNOWN\n",
	      pos);
    }
  }
  
  return found_one;
}


/* small_semeai() addresses a deficiency in the reading code:
 * for reasons of speed, savestone3 and savestone4 do not
 * sufficiently check whether there may be an adjoining string
 * which can be attacked. So they may overlook a defensive
 * move which consists of attacking an adjoining string.
 *
 * small_semeai(), called by make_worms() searches for a 
 * string A with 3 or 4 liberties such that worm[A], attack_code != 0.
 * If there is a string B next to A (of the opposite color)
 * such that worm[B].attack_code != 0, the following action is
 * taken: if worm[A].liberties == worm[B].liberties, then
 * worm[A].defend is set to worm[B].defend and vice versa;
 * and if worm[A].liberties > worm[B].liberties, then worm[A].defendi
 * is set to -1.
 */

void
small_semeai()
{
  int pos;
  for (pos = BOARDMIN; pos < BOARDMAX; pos++)
    if (IS_STONE(board[pos])
	&& (worm[pos].liberties == 3 || worm[pos].liberties == 4)
	&& worm[pos].attack_codes[0] != 0) {
      int other = OTHER_COLOR(board[pos]);
      
      if (board[SOUTH(pos)] == other)
	small_semeai_analyzer(pos, SOUTH(pos));
      if (board[WEST(pos)] == other)
	small_semeai_analyzer(pos, WEST(pos));
      if (board[NORTH(pos)] == other)
	small_semeai_analyzer(pos, NORTH(pos));
      if (board[EAST(pos)] == other)
	small_semeai_analyzer(pos, EAST(pos));
    }
}

/* Helper function for small_semeai. Tries to resolve the
 * semeai between (str1) and (str2), possibly revising points
 * of attack and defense.
 */

static void
small_semeai_analyzer(int str1, int str2)
{
  int apos;
  int color = board[str1];
  int other = board[str2];

  if (worm[str2].attack_codes[0] == 0 || worm[str2].liberties < 3)
    return;
  if (worm[str1].attack_codes[0] == 0 || worm[str1].liberties < 3)
    return;


  /* FIXME: There are many more possibilities to consider */
  if (trymove(worm[str1].attack_points[0], other,
	      "small_semeai_analyzer", str1, EMPTY, 0)) {
    int acode = attack(str2, &apos);
    if (acode == 0) {
      popgo();
      change_defense(str2, worm[str1].attack_points[0], 1);
    }
    else if (trymove(apos, color, "small_semeai_analyzer", str1,
		     EMPTY, NO_MOVE)) {
      if (attack(str1, NULL) == 0) {
	popgo();
	popgo();
	change_attack(str1, 0, 0);
      }
      else {
	popgo();
	popgo();
      }
    }
    else
      popgo();
  }
  gg_assert(stackp == 0);
  
  if (trymove(worm[str2].attack_points[0], color, 
	      "small_semeai_analyzer", str2, EMPTY, 0)) {
    int acode = attack(str1, &apos);
    if (acode == 0) {
      popgo();
      change_defense(str1, worm[str2].attack_points[0], 1);
    }
    else if (trymove(apos, other, "small_semeai_analyzer", str2,
		     EMPTY, NO_MOVE)) {
      if (attack(str2, NULL) == 0) {
	popgo();
	popgo();
	change_attack(str2, 0, 0);
      }
      else {
	popgo();
	popgo();
      }
    }
    else
      popgo();
  }
  gg_assert(stackp == 0);
}


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
