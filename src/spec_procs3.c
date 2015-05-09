/*
  SillyMUD Distribution V1.1b             (c) 1993 SillyMUD Developement
 
  See license.doc for distribution terms.   SillyMUD is based on DIKUMUD
*/

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "protos.h"

/*   external vars  */

extern struct room_data *world;
extern struct char_data *character_list;
extern struct descriptor_data *descriptor_list;
extern struct index_data *obj_index;
extern struct time_info_data time_info;
extern struct index_data *mob_index;
extern struct weather_data weather_info;
	extern int top_of_world;
	extern struct int_app_type int_app[26];

extern struct title_type titles[4][ABS_MAX_LVL];
extern char *dirs[]; 

extern int gSeason;  /* what season is it ? */

/* extern procedures */
void do_group(struct char_data *ch, char *arg, int cmd);
int choose_exit_global(int a, int b, int c);
int go_direction(struct char_data *ch, int dir);
void hit(struct char_data *ch, struct char_data *victim, int type);
void gain_exp(struct char_data *ch, int gain);
char *strdup(char *source);
struct char_data *FindVictim( struct char_data *ch);
struct char_data *char_holding( struct obj_data *obj);
void send_to_all(char *messg);
void do_shout(struct char_data *ch, char *argument, int cmd);
int IsUndead( struct char_data *ch);
struct time_info_data age(struct char_data *ch);
int CountLims(struct obj_data *obj);
struct char_data *FindAnAttacker( struct char_data *ch);
void NailThisSucker( struct char_data *ch);
int NumCharmedFollowersInRoom(struct char_data *ch);
struct char_data *FindMobDiffZoneSameRace(struct char_data *ch);
struct char_data *FindMobInRoomWithFunction(int room, int (*func)());
void do_stand(struct char_data *ch, char *arg, int cmd);
void do_sit(struct char_data *ch, char *arg, int cmd);
void do_shout(struct char_data *ch, char *arg, int cmd);
void do_emote(struct char_data *ch, char *arg, int cmd);
void do_say(struct char_data *ch, char *arg, int cmd);
void add_follower(struct char_data *ch, struct char_data *leader);
void stop_follower(struct char_data *ch);

/* chess_game() stuff starts here */
/* Inspiration and original idea by Feith */
/* Implementation by Gecko */

#define WHITE 0
#define BLACK 1

int side = WHITE;  /* to avoid having to pass side with each function call */

#define IS_BLACK(piece) (((piece) >= 1400) && ((piece) <= 1415))
#define IS_WHITE(piece) (((piece) >= 1448) && ((piece) <= 1463))
#define IS_PIECE(piece) ((IS_WHITE(piece)) || (IS_BLACK(piece)))
#define IS_ENEMY(piece) (side?IS_WHITE(piece):IS_BLACK(piece))
#define IS_FRIEND(piece) (side?IS_BLACK(piece):IS_WHITE(piece))
#define ON_BOARD(room) (((room) >= 1400) && ((room) <= 1463))
#define FORWARD (side?2:0)
#define BACK    (side?0:2)
#define LEFT    (side?1:3)
#define RIGHT   (side?3:1)

#define EXIT_ROOM(roomp,dir) ((roomp)?((roomp)->dir_option[dir]):NULL)
#define CAN_GO_ROOM(roomp,dir) (EXIT_ROOM(roomp,dir) && \
                               real_roomp(EXIT_ROOM(roomp,dir)->to_room))
                                
/* get pointer to room in the given direction */                               
#define ROOMP(roomp,dir) ((CAN_GO_ROOM(roomp,dir)) ? \
                          real_roomp(EXIT_ROOM(roomp,dir)->to_room) : NULL)
                       
struct room_data *forward_square(struct room_data *room)
{
  return ROOMP(room, FORWARD);
}

struct room_data *back_square(struct room_data *room)
{
  return ROOMP(room, BACK);
}

struct room_data *left_square(struct room_data *room)
{
  return ROOMP(room, LEFT);
}

struct room_data *right_square(struct room_data *room)
{
  return ROOMP(room, RIGHT);
}

struct room_data *forward_left_square(struct room_data *room)
{
  return ROOMP(ROOMP(room, FORWARD), LEFT);
}

struct room_data *forward_right_square(struct room_data *room)
{
  return ROOMP(ROOMP(room, FORWARD), RIGHT);
}

struct room_data *back_right_square(struct room_data *room)
{
  return ROOMP(ROOMP(room, BACK), RIGHT);
}

struct room_data *back_left_square(struct room_data *room)
{
  return ROOMP(ROOMP(room, BACK), LEFT);
}

struct char_data *square_contains_enemy(struct room_data *square)
{
  struct char_data *i;
  
  for (i = square->people; i; i = i->next_in_room)
    if (IS_ENEMY(mob_index[i->nr].virtual))
      return i;

  return NULL;
}

int square_contains_friend(struct room_data *square)
{
  struct char_data *i;

  for (i = square->people; i; i = i->next_in_room)
    if (IS_FRIEND(mob_index[i->nr].virtual))
      return TRUE;

  return FALSE;
}

int square_empty(struct room_data *square)
{
  struct char_data *i;
  
  for (i = square->people; i; i = i->next_in_room)
    if (IS_PIECE(mob_index[i->nr].virtual))
      return FALSE;

  return TRUE;
}
  
int chess_game(struct char_data *ch, int cmd, char *arg, struct char_data *mob, int type)
{
  struct room_data *rp = NULL, *crp = real_roomp(ch->in_room);
  struct char_data *ep = NULL;
  int move_dir = 0, move_amount = 0, move_found = FALSE;
  int c = 0;

  if (cmd || !AWAKE(ch))
    return FALSE;

  /* keep original fighter() spec_proc for kings and knights */    
  if (ch->specials.fighting)
    switch (mob_index[ch->nr].virtual) {
      case 1401: case 1404: case 1406: case 1457: case 1460: case 1462:
        return fighter(ch, cmd, arg, mob, type);
      default:
        return FALSE;
    }

  if (!crp || !ON_BOARD(crp->number))
    return FALSE;

  if (side == WHITE && IS_BLACK(mob_index[ch->nr].virtual))
    return FALSE;

  if (side == BLACK && IS_WHITE(mob_index[ch->nr].virtual))
    return FALSE;

  if (number(0,15))
    return FALSE;

  switch (mob_index[ch->nr].virtual) {
    case 1408: case 1409: case 1410: case 1411:  /* black pawns */
    case 1412: case 1413: case 1414: case 1415:
    case 1448: case 1449: case 1450: case 1451:  /* white pawns */
    case 1452: case 1453: case 1454: case 1455:
      move_dir = number(0,3);
      switch (move_dir) {
        case 0: rp = forward_left_square(crp);  break;
        case 1: rp = forward_right_square(crp); break;
        case 2: rp = forward_square(crp);       break;
        case 3: 
          if (real_roomp(ch->in_room) &&
              (real_roomp(ch->in_room)->number == mob_index[ch->nr].virtual)) {
            rp = forward_square(crp); 
            if (rp && square_empty(rp) && ON_BOARD(rp->number)) {
              crp = rp;
              rp = forward_square(crp);
            }
          }
      }
      if (rp && (!square_contains_friend(rp)) && ON_BOARD(rp->number)) {
        ep = square_contains_enemy(rp);
        if (((move_dir <= 1) && ep) || ((move_dir > 1) && !ep))
          move_found = TRUE;
      }
      break;

    case 1400:  /* black rooks */
    case 1407:
    case 1456:  /* white rooks */
    case 1463:
      move_dir = number(0,3);
      move_amount = number(1,7);
      for (c = 0; c < move_amount; c++) {
        switch(move_dir) {
          case 0: rp = forward_square(crp);  break;
          case 1: rp = back_square(crp);     break;
          case 2: rp = right_square(crp);    break;
          case 3: rp = left_square(crp);
        }
        if (rp && !square_contains_friend(rp) && ON_BOARD(rp->number)) {
          move_found = TRUE;
          if ((ep = square_contains_enemy(rp)))
            c = move_amount;
          else
            crp = rp;
        }
        else {
          c = move_amount;
          rp = crp;
        }
      }
      break;
      
    case 1401:  /* black knights */
    case 1406:
    case 1457:  /* white knights */
    case 1462:
      move_dir = number(0,7);
      switch(move_dir) {
        case 0: rp = forward_left_square(forward_square(crp));  break;
        case 1: rp = forward_right_square(forward_square(crp)); break;
        case 2: rp = forward_right_square(right_square(crp));   break;
        case 3: rp = back_right_square(right_square(crp));  break;
        case 4: rp = back_right_square(back_square(crp));   break;
        case 5: rp = back_left_square(back_square(crp));    break;
        case 6: rp = back_left_square(left_square(crp));    break;
        case 7: rp = forward_left_square(left_square(crp));
      }
      if (rp && !square_contains_friend(rp) && ON_BOARD(rp->number)) {
        move_found = TRUE;
        ep = square_contains_enemy(rp);
      }
      break;
      
    case 1402:  /* black bishops */
    case 1405:
    case 1458:  /* white bishops */
    case 1461:
      move_dir = number(0,3);
      move_amount = number(1,7);
      for (c = 0; c < move_amount; c++) {
        switch(move_dir) {
          case 0: rp = forward_left_square(crp);  break;
          case 1: rp = forward_right_square(crp); break;
          case 2: rp = back_right_square(crp);    break;
          case 3: rp = back_left_square(crp);
        }
        if (rp && !square_contains_friend(rp) && ON_BOARD(rp->number)) {
          move_found = TRUE;
          if ((ep = square_contains_enemy(rp)))
            c = move_amount;
          else
            crp = rp;
        }
        else {
          c = move_amount;
          rp = crp;
        }
      }
      break;
      
    case 1403:  /* black queen */
    case 1459:  /* white queen */
      move_dir = number(0,7);
      move_amount = number(1,7);
      for (c = 0; c < move_amount; c++) {
        switch(move_dir) {
          case 0: rp = forward_left_square(crp);  break;
          case 1: rp = forward_square(crp);       break;
          case 2: rp = forward_right_square(crp); break;
          case 3: rp = right_square(crp);         break;
          case 4: rp = back_right_square(crp);    break;
          case 5: rp = back_square(crp);          break;
          case 6: rp = back_left_square(crp);     break;
          case 7: rp = left_square(crp);
        }
        if (rp && !square_contains_friend(rp) && ON_BOARD(rp->number)) {
          move_found = TRUE;
          if ((ep = square_contains_enemy(rp)))
            c = move_amount;
          else
            crp = rp;
        }
        else {
          c = move_amount;
          rp = crp;
        }
      }
      break;
            
    case 1404:  /* black king */
    case 1460:  /* white king */ 
      move_dir = number(0,7);
      switch (move_dir) {
        case 0: rp = forward_left_square(crp);  break;
        case 1: rp = forward_square(crp);       break;
        case 2: rp = forward_right_square(crp); break;
        case 3: rp = right_square(crp);         break;
        case 4: rp = back_right_square(crp);    break;
        case 5: rp = back_square(crp);          break;
        case 6: rp = back_left_square(crp);     break;
        case 7: rp = left_square(crp);
      }
      if (rp && !square_contains_friend(rp) && ON_BOARD(rp->number)) {
        move_found = TRUE;
        ep = square_contains_enemy(rp);
      }
      break;  
  }

  if (move_found && rp) {
    do_emote(ch, "leaves the room.", 0);    
    char_from_room(ch);
    char_to_room(ch, rp->number);
    do_emote(ch, "has arrived.", 0);
    
    if (ep) {
      if (side)
        switch(number(0,3)) {
          case 0: 
            do_emote(ch, "grins evilly and says, 'ONLY EVIL shall rule!'", 0);  
            break;
          case 1: 
            do_emote(ch, "leers cruelly and says, 'You will die now!'", 0);
            break;
          case 2: 
            do_emote(ch, "issues a bloodcurdling scream.", 0);
            break;
          case 3: 
            do_emote(ch, "glares with black anger.", 0);
        }
      else
        switch(number(0,3)) {
          case 0: 
            do_emote(ch, "glows an even brighter pristine white.", 0);
            break;
          case 1: 
            do_emote(ch, "chants a prayer and begins battle.", 0);
            break;
          case 2: 
            do_emote(ch, "says, 'Black shall lose!", 0);
            break;
          case 3: 
            do_emote(ch, "shouts, 'For the Flame! The Flame!'", 0);
        }
      hit(ch, ep, TYPE_UNDEFINED);
    }
    side = (side + 1) % 2;
    return TRUE;
  }
  return FALSE;
}

int AcidBlob(struct char_data *ch, int cmd, char *arg, struct char_data *mob, int type)
{
  struct obj_data *i;
  
  if (cmd || !AWAKE(ch))
    return(FALSE);
  
  for (i = real_roomp(ch->in_room)->contents; i; i = i->next_content) {
    if (IS_SET(i->obj_flags.wear_flags, ITEM_TAKE) && !strncmp(i->name, "corpse", 6)) {
      act("$n destroys some trash.", FALSE, ch, 0, 0, TO_ROOM);
      
      obj_from_room(i);
      extract_obj(i);
      return(TRUE);
    }
  }
  return(FALSE);
}
 
int death_knight(struct char_data *ch, int cmd, char *arg, struct char_data *mob, int type)
{

  if (cmd) return(FALSE);
  if (!AWAKE(mob)) return(FALSE);

  if (number(0,1)) {
    return(fighter(mob, cmd, arg, mob, type));
  } else {
    return(magic_user(mob, cmd, arg, mob, type));
  }
}
