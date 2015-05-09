/*
  SillyMUD Distribution V1.1b             (c) 1993 SillyMUD Developement
 
  See license.doc for distribution terms.   SillyMUD is based on DIKUMUD
*/

#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#include "protos.h"

void log_sev(char *s, int i);
void log (char *s) { log_sev(s, 1); }
extern struct time_data time_info;
extern struct descriptor_data *descriptor_list;
extern struct char_data *character_list;
extern struct index_data *mob_index, *obj_index;
extern struct chr_app_type chr_apply[];
#if HASH
extern struct hash_header room_db;	                  /* In db.c */
#else
extern struct room_data *room_db[];	                  /* In db.c */
#endif
extern char *dirs[]; 
extern int  RacialMax[][6];


/* external functions */
void stop_fighting(struct char_data *ch);
void fake_setup_dir(FILE *fl, int room, int dir);
void setup_dir(FILE *fl, int room, int dir);
char *fread_string(FILE *fl);
int check_falling(struct char_data *ch);
void NailThisSucker( struct char_data *ch);


#if EGO
int EgoBladeSave(struct char_data *ch)
{
   int total;
   
   if (GetMaxLevel(ch) <= 10) return(FALSE);
   total = (GetMaxLevel(ch) + GET_STR(ch) + GET_CON(ch));
   if (GET_HIT(ch) == 0) return(FALSE);
   total = total - (GET_MAX_HIT(ch) / GET_HIT(ch));
   if (number(1,101) > total) {
      return(FALSE);
   } else return(TRUE);
}
#endif

int MIN(int a, int b)
{
	return a < b ? a:b;
}


int MAX(int a, int b)
{
	return a > b ? a:b;
}

int GetItemClassRestrictions(struct obj_data *obj)
{
  int total=0;

  if (IS_SET(obj->obj_flags.extra_flags, ITEM_ANTI_MAGE)) {
    total += CLASS_MAGIC_USER;
  }
  if (IS_SET(obj->obj_flags.extra_flags, ITEM_ANTI_THIEF)) {
    total += CLASS_THIEF;
  }
  if (IS_SET(obj->obj_flags.extra_flags, ITEM_ANTI_FIGHTER)) {
    total += CLASS_WARRIOR;
  }
  if (IS_SET(obj->obj_flags.extra_flags, ITEM_ANTI_CLERIC)) {
    total += CLASS_CLERIC;
  }

  return(total);

}

int CAN_SEE(struct char_data *s, struct char_data *o)
{

  if (!o || s->in_room < 0 || o->in_room < 0)
    return(FALSE);

  if (IS_IMMORTAL(s)) 
    return(TRUE);

  if (o->invis_level >= LOW_IMMORTAL)  /* change this if you want multiple*/
    return FALSE;                     /* levels of invis.                 */

  if (IS_AFFECTED(s, AFF_TRUE_SIGHT))
    return(TRUE);

  if (IS_AFFECTED(s, AFF_BLIND) || IS_AFFECTED(o, AFF_HIDE))
    return(FALSE);

  if (IS_AFFECTED(o, AFF_INVISIBLE)) {
    if (IS_IMMORTAL(o))
      return(FALSE);
    if (!IS_AFFECTED(s, AFF_DETECT_INVISIBLE))
      return(FALSE);
  }

  if ((IS_DARK(s->in_room) || IS_DARK(o->in_room)) &&
        (!IS_AFFECTED(s, AFF_INFRAVISION)))
        return(FALSE);

  if (IS_AFFECTED2(o, AFF2_ANIMAL_INVIS) && IsAnimal(s))
    return(FALSE);

  return(TRUE);

#if 0
  ((IS_IMMORTAL(sub)) || /* gods can see anything */ \
   (((!IS_AFFECTED((obj),AFF_INVISIBLE)) || /* visible object */ \
     ((IS_AFFECTED((sub),AFF_DETECT_INVISIBLE)) && /* you detect I and */ \
      (!IS_IMMORTAL(obj)))) &&			/* object is not a god */ \
    (!IS_AFFECTED((sub),AFF_BLIND)) &&      /* you are not blind */ \
    ( (IS_LIGHT(sub->in_room)) || (IS_AFFECTED((sub),AFF_INFRAVISION))) \
		/* there is enough light to see or you have infravision */ \
    ))
#endif
}


int exit_ok(struct room_direction_data	*exit, struct room_data **rpp)
{
  struct room_data	*rp;
  if (rpp==NULL)
    rpp = &rp;
  if (!exit) {
    *rpp = NULL;
    return FALSE;
  }
  *rpp = real_roomp(exit->to_room);
  return (*rpp!=NULL);
}

int MobVnum( struct char_data *c)
{
  if (IS_NPC(c)) {
    return(mob_index[c->nr].virtual);
  } else {
    return(0);
  }
}

int ObjVnum( struct obj_data *o)
{
  if (o->item_number >= 0)
     return(obj_index[o->item_number].virtual);
  else
    return(-1);
}


void Zwrite (FILE *fp, char cmd, int tf, int arg1, int arg2, int arg3, 
	     char *desc)
{
   char buf[100];

   if (*desc) {
     sprintf(buf, "%c %d %d %d %d   ; %s\n", cmd, tf, arg1, arg2, arg3, desc);
     fputs(buf, fp);
   } else {
     sprintf(buf, "%c %d %d %d %d\n", cmd, tf, arg1, arg2, arg3); 
     fputs(buf, fp);
   }
}

void RecZwriteObj(FILE *fp, struct obj_data *o)
{
   struct obj_data *t;

   if (ITEM_TYPE(o) == ITEM_CONTAINER) {
     for (t = o->contains; t; t=t->next_content) {
       Zwrite(fp, 'P', 1, ObjVnum(t), obj_index[t->item_number].number, ObjVnum(o), 
	      t->short_description);
       RecZwriteObj(fp, t);
     }
   } else {
     return;
   }
}

FILE *MakeZoneFile( struct char_data *c)
{
  char buf[256];
  FILE *fp;

  sprintf(buf, "zone/%s.zon", GET_NAME(c));

  if ((fp = fopen(buf, "w")) != NULL)
    return(fp);
  else
    return(0);

}

int WeaponImmune(struct char_data *ch)
{

  if (IS_SET(IMM_NONMAG, ch->M_immune) ||
      IS_SET(IMM_PLUS1, ch->M_immune) ||
      IS_SET(IMM_PLUS2, ch->M_immune) ||
      IS_SET(IMM_PLUS3, ch->M_immune) ||
      IS_SET(IMM_PLUS4, ch->M_immune))
    return(TRUE);
   return(FALSE);

}

unsigned IsImmune(struct char_data *ch, int bit)
{
  return(IS_SET(bit, ch->M_immune));
}

unsigned IsResist(struct char_data *ch, int bit)
{
  return(IS_SET(bit, ch->immune));
}

unsigned IsSusc(struct char_data *ch, int bit)
{
  return(IS_SET(bit, ch->susc));
}

/* creates a random number in interval [from;to] */
int number(int from, int to) 
{
   if (to - from + 1 )
	return((random() % (to - from + 1)) + from);
   else
       return(from);
}



/* simulates dice roll */
int dice(int number, int size) 
{
  int r;
  int sum = 0;

	assert(size >= 0);

  if (size == 0) return(0);

  for (r = 1; r <= number; r++) sum += ((random() % size)+1);
  return(sum);
}


/* Causing memory leak, but is a standard c function, so commenting out. -DM


char *strdup(char *source)
{
	char *new;

	CREATE(new, char, strlen(source)+1);
	return(strcpy(new, source));
}
*/

int scan_number(char *text, int *rval)
{
  int	length;
  if (1!=sscanf(text, " %i %n", rval, &length))
    return 0;
  if (text[length] != 0)
    return 0;
  return 1;
}


/* returns: 0 if equal, 1 if arg1 > arg2, -1 if arg1 < arg2  */
/* scan 'till found different or end of both                 */
int str_cmp(char *arg1, char *arg2)
{
  int chk, i;

  if ((!arg2) || (!arg1))
    return(1);

  for (i = 0; *(arg1 + i) || *(arg2 + i); i++)
    if (chk = LOWER(*(arg1 + i)) - LOWER(*(arg2 + i)))
      if (chk < 0)
	return (-1);
      else 
	return (1);
  return(0);
}



/* returns: 0 if equal, 1 if arg1 > arg2, -1 if arg1 < arg2  */
/* scan 'till found different, end of both, or n reached     */
int strn_cmp(char *arg1, char *arg2, int n)
{
  int chk, i;
  
  for (i = 0; (*(arg1 + i) || *(arg2 + i)) && (n>0); i++, n--)
    if (chk = LOWER(*(arg1 + i)) - LOWER(*(arg2 + i)))
      if (chk < 0)
	return (-1);
      else 
	return (1);
  
  return(0);
}



/* writes a string to the log */
void log_sev(char *str,int sev)
{
  long ct;
  char *tmstr;
  static char buf[500];
  struct descriptor_data *i;
  
  
  ct = time(0);
  tmstr = asctime(localtime(&ct));
  *(tmstr + strlen(tmstr) - 1) = '\0';
  fprintf(stderr, "%s :: %s\n", tmstr, str);
  
  
  if (str)
    sprintf(buf,"/* %s */\n\r",str);
  for (i = descriptor_list; i; i = i->next)
    if ((!i->connected) && (GetMaxLevel(i->character)>=LOW_IMMORTAL) &&
	(i->character->specials.sev <= sev) &&
	(!IS_SET(i->character->specials.act, PLR_NOSHOUT)))
      write_to_q(buf, &i->output);
}

void slog(char *str)
{
	long ct;
	char *tmstr;

	ct = time(0);
	tmstr = asctime(localtime(&ct));
	*(tmstr + strlen(tmstr) - 1) = '\0';
	fprintf(stderr, "%s :: %s\n", tmstr, str);

}
	


void sprintbit(unsigned long vektor, char *names[], char *result)
{
  long nr;
  
  *result = '\0';
  
  for(nr=0; vektor; vektor>>=1)
    {
      if (IS_SET(1, vektor))
	if (*names[nr] != '\n') {
	  strcat(result,names[nr]);
	  strcat(result," ");
	} else {
	  strcat(result,"UNDEFINED");
	  strcat(result," ");
	}
      if (*names[nr] != '\n')
	nr++;
    }
  
  if (!*result)
    strcat(result, "NOBITS");
}



void sprinttype(int type, char *names[], char *result)
{
	int nr;

	for(nr=0;(*names[nr]!='\n');nr++);
	if(type < nr)
		strcpy(result,names[type]);
	else
		strcpy(result,"UNDEFINED");
}


/* Calculate the REAL time passed over the last t2-t1 centuries (secs) */
struct time_info_data real_time_passed(time_t t2, time_t t1)
{
	long secs;
	struct time_info_data now;

	secs = (long) (t2 - t1);

  now.hours = (secs/SECS_PER_REAL_HOUR) % 24;  /* 0..23 hours */
  secs -= SECS_PER_REAL_HOUR*now.hours;

  now.day = (secs/SECS_PER_REAL_DAY);          /* 0..34 days  */
  secs -= SECS_PER_REAL_DAY*now.day;

	now.month = -1;
  now.year  = -1;

	return now;
}



/* Calculate the MUD time passed over the last t2-t1 centuries (secs) */
struct time_info_data mud_time_passed(time_t t2, time_t t1)
{
  long secs;
  struct time_info_data now;
  
  secs = (long) (t2 - t1);
  
  now.hours = (secs/SECS_PER_MUD_HOUR) % 24;  /* 0..23 hours */
  secs -= SECS_PER_MUD_HOUR*now.hours;
  
  now.day = (secs/SECS_PER_MUD_DAY) % 35;     /* 0..34 days  */
  secs -= SECS_PER_MUD_DAY*now.day;
  
  now.month = (secs/SECS_PER_MUD_MONTH) % 17; /* 0..16 months */
  secs -= SECS_PER_MUD_MONTH*now.month;
  
  now.year = (secs/SECS_PER_MUD_YEAR);        /* 0..XX? years */
  
  return(now);
}

void mud_time_passed2(time_t t2, time_t t1, struct time_info_data *t)
{
  long secs;

  secs = (long) (t2 - t1);
  
  t->hours = (secs/SECS_PER_MUD_HOUR) % 24;  /* 0..23 hours */
  secs -= SECS_PER_MUD_HOUR*t->hours;
  
  t->day = (secs/SECS_PER_MUD_DAY) % 35;     /* 0..34 days  */
  secs -= SECS_PER_MUD_DAY*t->day;
  
  t->month = (secs/SECS_PER_MUD_MONTH) % 17; /* 0..16 months */
  secs -= SECS_PER_MUD_MONTH*t->month;
  
  t->year = (secs/SECS_PER_MUD_YEAR);        /* 0..XX? years */

}


void age2(struct char_data *ch, struct time_info_data *g)
{

  mud_time_passed2(time(0),ch->player.time.birth, g);
  
  g->year += 17;   /* All players start at 17 */
  
}

struct time_info_data age(struct char_data *ch)
{
  struct time_info_data player_age;
  
  player_age = mud_time_passed(time(0),ch->player.time.birth);
  
  player_age.year += 17;   /* All players start at 17 */
  
  return(player_age);
}


char in_group ( struct char_data *ch1, struct char_data *ch2)
{



/* 
   possibilities ->
   1.  char is char2's master
   2.  char2 is char's master
   3.  char and char2 follow same.
   4.  char rides char2
   5.. char2 rides char

    otherwise not true.
 
*/
   if (ch1 == ch2)
      return(TRUE);

   if ((!ch1) || (!ch2))
      return(0);

   if ((!ch1->master) && (!ch2->master))
      return(0);

   if (ch1==ch2->master)
     return(1);

   if (ch1->master == ch2)
     return(1);

   if (ch1->master == ch2->master) {
     return(1);
   }

   if (MOUNTED(ch1) == ch2 || RIDDEN(ch1) == ch2)
     return(1);

   return(0);
}


/*
  more new procedures 
*/


/*
   these two procedures give the player the ability to buy 2*bread
   or put all.bread in bag, or put 2*bread in bag...
*/

char getall(char *name, char *newname)
{
   char arg[40],tmpname[80], otname[80];
   char prd;

   arg[0] = 0;
   tmpname[0] = 0;
   otname[0] = 0;

   sscanf(name,"%s ",otname);   /* reads up to first space */

   if (strlen(otname)<5)
      return(FALSE);

   sscanf(otname,"%3s%c%s",arg,&prd,tmpname);

   if (prd != '.')
     return(FALSE);
   if (tmpname == NULL) 
      return(FALSE);
   if (strcmp(arg,"all"))
      return(FALSE);

   while (*name != '.')
       name++;

   name++;

   for (; *newname = *name; name++,newname++);

   return(TRUE);
}


int getabunch(char *name, char  *newname)
{
   int num=0;
   char tmpname[80];

   tmpname[0] = 0;
   sscanf(name,"%d*%s",&num,tmpname);
   if (tmpname[0] == '\0')
      return(FALSE);
   if (num < 1)
      return(FALSE);
   if (num>9)
      num = 9;

   while (*name != '*')
       name++;

   name++;

   for (; *newname = *name; name++,newname++);

   return(num);

}


int DetermineExp( struct char_data *mob, int exp_flags)
{

int base;
int phit;
int sab;
char buf[200];

   if (exp_flags > 10) { 
     sprintf(buf, "Exp flags on %s are > 10 (%d)", GET_NAME(mob), exp_flags);
     log(buf);
   }
/* 
reads in the monster, and adds the flags together 
for simplicity, 1 exceptional ability is 2 special abilities 
*/

    if (GetMaxLevel(mob) < 0)
       return(1);

    switch(GetMaxLevel(mob)) {

    case 0:   base = 5;
              phit = 1;
              sab = 10;
              break;

    case 1:   base = 10;
              phit = 1;
              sab =  15;
              break;

    case 2:   base = 20;
              phit = 2;
              sab =  20;
              break;


    case 3:   base = 35;
              phit = 3;
              sab =  25;
              break;

    case 4:   base = 60;
              phit = 4;
              sab =  30;
              break;

    case 5:   base = 90;
              phit = 5;
              sab =  40;
              break;

    case 6:   base = 150;
              phit = 6;
              sab =  75;
              break;

    case 7:   base = 225;
              phit = 8;
              sab =  125;
              break;

    case 8:   base = 600;
              phit = 12;
              sab  = 175;
              break;

    case 9:   base = 900;
              phit = 14;
              sab  = 300;
              break;

    case 10:   base = 1100;
              phit  = 15;
              sab   = 450;
              break;

    case 11:   base = 1300;
              phit  = 16;
              sab   = 700;
              break;

    case 12:   base = 1550;
              phit  = 17;
              sab   = 700;
              break;

    case 13:   base = 1800;
              phit  = 18;
              sab   = 950;
              break;

    case 14:   base = 2100;
              phit  = 19;
              sab   = 950;
              break;

    case 15:   base = 2400;
              phit  = 20;
              sab   = 1250;
              break;

    case 16:   base = 2700;
              phit  = 23;
              sab   = 1250;
              break;

    case 17:   base = 3000;
              phit  = 25;
              sab   = 1550;
              break;

    case 18:   base = 3500;
              phit  = 28;
              sab   = 1550;
              break;

    case 19:   base = 4000;
              phit  = 30;
              sab   = 2100;
              break;

    case 20:   base = 4500;
              phit  = 33;
              sab   = 2100;
              break;

    case 21:   base = 5000;
              phit  = 35;
              sab   =  2600;
              break;

    case 22:   base = 6000;
              phit  = 40;
              sab   = 3000;
              break;

    case 23:   base = 7000;
              phit  = 45;
              sab   = 3500;
              break;

    case 24:   base = 8000;
              phit  = 50;
              sab   = 4000;
              break;

    case 25:   base = 9000;
              phit  = 55;
              sab   = 4500;
              break;

    case 26:   base = 10000;
              phit  = 60;
              sab   =  5000;
              break;

    case 27:   base = 12000;
              phit  = 70;
              sab   = 6000;
              break;

    case 28:   base = 14000;
              phit  = 80;
              sab   = 7000;
              break;

    case 29:   base = 16000;
              phit  = 90;
              sab   = 8000;
              break;

    case 30:   base = 20000;
              phit  = 100;
              sab   = 10000;
              break;

      default : base = 22000;
                phit = 120;
                sab  = 12000;
                break;
    }

    return(base + (phit * GET_HIT(mob)) + (sab * exp_flags));


}

/*
int  DetermineExp( struct char_data *mob, int exp_flags);
char getall(char *name, char *newname);
int getabunch(char *name, char  *newname);
*/


void down_river( int pulse )
{
   struct char_data *ch, *tmp;
   struct obj_data *obj_object, *next_obj;
   int rd, or;
   char buf[80];
   struct room_data *rp;

   if (pulse < 0) 
      return;

   for (ch = character_list; ch; ch = tmp) {
        tmp = ch->next;
    if (!IS_NPC(ch)) {
     if (ch->in_room != NOWHERE) {
	if (real_roomp(ch->in_room)->sector_type == SECT_WATER_NOSWIM)
           if ((real_roomp(ch->in_room))->river_speed > 0) {
              if ((pulse % (real_roomp(ch->in_room))->river_speed)==0) {
                if (((real_roomp(ch->in_room))->river_dir<=5)&&((real_roomp(ch->in_room))->river_dir>=0)) {
 	 	   rd = (real_roomp(ch->in_room))->river_dir;
       		   for (obj_object = (real_roomp(ch->in_room))->contents;
				obj_object; obj_object = next_obj) {
		      next_obj = obj_object->next_content;
		      if ((real_roomp(ch->in_room))->dir_option[rd]) {
                          obj_from_room(obj_object);
   	                  obj_to_room(obj_object, (real_roomp(ch->in_room))->dir_option[rd]->to_room);
			}
		   }
/*
   flyers don't get moved
*/
                   if (!IS_AFFECTED(ch,AFF_FLYING) && !MOUNTED(ch)) {
		     rp = real_roomp(ch->in_room);
		     if (rp && rp->dir_option[rd] &&
			 rp->dir_option[rd]->to_room && 
			(EXIT(ch, rd)->to_room != NOWHERE)) {
      		         if (ch->specials.fighting) {
                               stop_fighting(ch);
			  }
		         if(IS_IMMORTAL(ch) && 
			    IS_SET(ch->specials.act, PLR_NOHASSLE)) {
			   send_to_char("The waters swirl beneath your feet.\n\r",ch);
			 } else {
			   sprintf(buf, "You drift %s...\n\r", dirs[rd]);
			   send_to_char(buf,ch);
			   if (RIDDEN(ch))
			     send_to_char(buf,RIDDEN(ch));
			   
			   or = ch->in_room;
			   char_from_room(ch);
			   if (RIDDEN(ch))  {
			     char_from_room(RIDDEN(ch));
			     char_to_room(RIDDEN(ch), (real_roomp(or))->dir_option[rd]->to_room);			 
			   }
			   char_to_room(ch,(real_roomp(or))->dir_option[rd]->to_room);
			   
			   do_look(ch, "\0",15);
			   if (RIDDEN(ch)) {
			     do_look(RIDDEN(ch), "\0",15);
			   }
			 }
			 if (IS_SET(RM_FLAGS(ch->in_room), DEATH) && 
			     GetMaxLevel(ch) < LOW_IMMORTAL) {
			   if (RIDDEN(ch))
			     NailThisSucker(RIDDEN(ch));
			   NailThisSucker(ch);
			 }
		       }
		    }
		 }
	      }
	   }
        }
      }
    }
 }

void RoomSave(struct char_data *ch, int start, int end)
{
   char fn[80], temp[2048], dots[500];
   int rstart, rend, i, j, k, x;
   struct extra_descr_data *exptr;
   FILE *fp;
   struct room_data	*rp;
   struct room_direction_data	*rdd;


   sprintf(fn, "rooms/%s", ch->player.name);
   if ((fp = fopen(fn,"w")) == NULL) {
     send_to_char("Can't write to disk now..try later \n\r",ch);
     return;
   }

   rstart = start;
   rend = end;

   if (((rstart <= -1) || (rend <= -1)) || 
       ((rstart > 40000) || (rend > 40000))){
    send_to_char("I don't know those room #s.  make sure they are all\n\r",ch);
    send_to_char("contiguous.\n\r",ch);
    fclose(fp);
    return;
   }

   send_to_char("Saving\n",ch);
   strcpy(dots, "\0");
   
   for (i=rstart;i<=rend;i++) {

     rp = real_roomp(i);
     if (rp==NULL)
       continue;

     strcat(dots, ".");

/*
   strip ^Ms from description
*/
     x = 0;

     if (!rp->description) {
       CREATE(rp->description, char, 8);
       strcpy(rp->description, "Empty");
     }

     for (k = 0; k <= strlen(rp->description); k++) {
       if (rp->description[k] != 13)
	 temp[x++] = rp->description[k];
     }
     temp[x] = '\0';

     if (temp[0] == '\0') {
       strcpy(temp, "Empty");
     }

     fprintf(fp,"#%d\n%s~\n%s~\n",rp->number,rp->name,
 	                            temp);
     if (!rp->tele_targ) {
        fprintf(fp,"%d %d %d",rp->zone, rp->room_flags, rp->sector_type);
      } else {
	if (!IS_SET(TELE_COUNT, rp->tele_mask)) {
	   fprintf(fp, "%d %d -1 %d %d %d %d", rp->zone, rp->room_flags,
		rp->tele_time, rp->tele_targ, 
		rp->tele_mask, rp->sector_type);
	} else {
	   fprintf(fp, "%d %d -1 %d %d %d %d %d", rp->zone, rp->room_flags,
		rp->tele_time, rp->tele_targ, 
		rp->tele_mask, rp->tele_cnt, rp->sector_type);
	} 
      }
     if (rp->sector_type == SECT_WATER_NOSWIM) {
        fprintf(fp," %d %d",rp->river_speed,rp->river_dir);
     } 

     if (rp->room_flags & TUNNEL) {
       fprintf(fp, " %d ", (int)rp->moblim);
     }

     fprintf(fp,"\n");     

     for (j=0;j<6;j++) {
       rdd = rp->dir_option[j];
       if (rdd) {
          fprintf(fp,"D%d\n",j);

	  if (rdd->general_description && *rdd->general_description) {
	    if (strlen(rdd->general_description) > 0) {
	      temp[0] = '\0';
              x = 0;
	      
              for (k = 0; k <= strlen(rdd->general_description); k++) {
		if (rdd->general_description[k] != 13)
		  temp[x++] = rdd->general_description[k];
              }
	      temp[x] = '\0';
	      
	      fprintf(fp,"%s~\n", temp);
	    } else {
	      fprintf(fp,"~\n");
	    }
	  } else {
	    fprintf(fp,"~\n");
	  }

	  if (rdd->keyword) {
	   if (strlen(rdd->keyword)>0)
	     fprintf(fp, "%s~\n",rdd->keyword);
	   else
	     fprintf(fp, "~\n");
	  } else { 
	    fprintf(fp, "~\n");
	  }

          if (IS_SET(rdd->exit_info, EX_CLIMB | EX_ISDOOR | EX_PICKPROOF))
            fprintf(fp, "7");
          else if (IS_SET(rdd->exit_info, EX_CLIMB | EX_ISDOOR))
            fprintf(fp, "6");
          else if (IS_SET(rdd->exit_info, EX_CLIMB))
            fprintf(fp, "5");
          else if (IS_SET(rdd->exit_info, EX_ISDOOR | EX_SECRET | EX_PICKPROOF))
            fprintf(fp, "4");
          else if (IS_SET(rdd->exit_info, EX_ISDOOR | EX_SECRET))
            fprintf(fp, "3");
	  if (IS_SET(rdd->exit_info, EX_ISDOOR | EX_PICKPROOF)) {
	    fprintf(fp, "2");
	  } else if (IS_SET(rdd->exit_info, EX_ISDOOR)) {
	    fprintf(fp, "1");
          } else {
	    fprintf(fp, "0");
	  }

	  fprintf(fp," %d ", 
		  rdd->key);

	  fprintf(fp,"%d\n", rdd->to_room);
       }
     }

/*
  extra descriptions..
*/

   for (exptr = rp->ex_description; exptr; exptr = exptr->next) {
     x = 0;

    if (exptr->description) {
      for (k = 0; k <= strlen(exptr->description); k++) {
       if (exptr->description[k] != 13)
	 temp[x++] = exptr->description[k];
      }
      temp[x] = '\0';

     fprintf(fp,"E\n%s~\n%s~\n", exptr->keyword, temp);
    }
   }

   fprintf(fp,"S\n");

   }

   fclose(fp);
   send_to_char(dots, ch);
   send_to_char("\n\rDone\n\r",ch);
}


void RoomLoad( struct char_data *ch, int start, int end)
{
  FILE *fp;
  int vnum, found = FALSE, x;
  char chk[50], buf[80];
  struct room_data *rp, dummy;

  sprintf(buf, "rooms/%s", ch->player.name);
  if ((fp = fopen(buf,"r")) == NULL) {
    send_to_char("You don't appear to have an area...\n\r",ch);
    return;
  }
  
  send_to_char("Searching and loading rooms\n\r",ch);
  
  while ((!found) && ((x = feof(fp)) != TRUE)) {
    
    fscanf(fp, "#%d\n",&vnum);
    if ((vnum >= start) && (vnum <= end)) {
      if (vnum == end)
	found = TRUE;
      
      if ((rp=real_roomp(vnum)) == 0) {  /* empty room */
	rp = (void*)malloc(sizeof(struct room_data));
	bzero(rp, sizeof(struct room_data));
	room_enter(room_db, vnum, rp);
	send_to_char("+",ch);
      } else {
	if (rp->people) {
	  act("$n reaches down and scrambles reality.", FALSE, ch, NULL,
	      rp->people, TO_ROOM);
	}
	cleanout_room(rp);
	send_to_char("-",ch);
      }
      
      rp->number = vnum;
      load_one_room(fp, rp);
      
    } else {
      send_to_char(".",ch);
      /*  read w/out loading */
      dummy.number = vnum;
      load_one_room(fp, &dummy);
      cleanout_room(&dummy);
    }
  }
  fclose(fp);
  
  if (!found) {
    send_to_char("\n\rThe room number(s) that you specified could not all be found.\n\r",ch);
  } else {
    send_to_char("\n\rDone.\n\r",ch);
  }
  
}    

void fake_setup_dir(FILE *fl, int room, int dir)
{
	int tmp;
	char *temp;

	temp = fread_string(fl); /* descr */
	if (temp)
	  free(temp);
	temp = fread_string(fl); /* key */
	if (temp)
	  free(temp);

	fscanf(fl, " %d ", &tmp); 
	fscanf(fl, " %d ", &tmp);
	fscanf(fl, " %d ", &tmp);
}


int IsHumanoid( struct char_data *ch)
{
/* these are all very arbitrary */

  switch(GET_RACE(ch))
    {
    case RACE_HUMAN:
    case RACE_GNOME:
    case RACE_ELVEN:
    case RACE_DWARF:
    case RACE_HALFLING:
    case RACE_ORC:
    case RACE_LYCANTH:
    case RACE_UNDEAD:
    case RACE_GIANT:
    case RACE_GOBLIN:
    case RACE_DEVIL:
    case RACE_TROLL:
    case RACE_VEGMAN:
    case RACE_MFLAYER:
    case RACE_ENFAN:
    case RACE_PATRYN:
    case RACE_SARTAN:
    case RACE_ROO:
    case RACE_SMURF:
    case RACE_TROGMAN:
    case RACE_SKEXIE:
    case RACE_TYTAN:
    case RACE_DROW:
    case RACE_GOLEM:
    case RACE_DEMON:
    case RACE_DRAAGDIM:
    case RACE_ASTRAL:
    case RACE_GOD:
      return(TRUE);
      break;

    default:
      return(FALSE);
      break;
    }

}

int IsRideable( struct char_data *ch)
{
  if (IS_NPC(ch) && !IS_PC(ch)) {
    switch(GET_RACE(ch)) {
    case RACE_HORSE:
    case RACE_DRAGON:
      return(TRUE);
      break;
    default:
      return(FALSE);
      break;
    }
  } else return(FALSE);
}

int IsAnimal( struct char_data *ch)
{

  switch(GET_RACE(ch))
    {
    case RACE_PREDATOR:
    case RACE_FISH:
    case RACE_BIRD:
    case RACE_HERBIV:
    case RACE_REPTILE:
    case RACE_LABRAT:
    case RACE_ROO:
    case RACE_INSECT:
    case RACE_ARACHNID:
      return(TRUE);
      break;
    default:
      return(FALSE);
      break;
    }

}

int IsVeggie( struct char_data *ch)
{

  switch(GET_RACE(ch))
    {
    case RACE_PARASITE:
    case RACE_SLIME:
    case RACE_TREE:
    case RACE_VEGGIE:
    case RACE_VEGMAN:
      return(TRUE);
      break;
    default:
      return(FALSE);
      break;
    }

}

int IsUndead( struct char_data *ch)
{

  switch(GET_RACE(ch)) {
  case RACE_UNDEAD: 
  case RACE_GHOST:
    return(TRUE);
    break;
  default:
    return(FALSE);
    break;
  }

}

int IsLycanthrope( struct char_data *ch)
{
  switch (GET_RACE(ch)) {
  case RACE_LYCANTH:
    return(TRUE);
    break;
  default:
    return(FALSE);
    break;
  }

}

int IsDiabolic( struct char_data *ch)
{

  switch(GET_RACE(ch))
    {
    case RACE_DEMON:
    case RACE_DEVIL:
      return(TRUE);
      break;
    default:
      return(FALSE);
      break;
    }

}

int IsReptile( struct char_data *ch)
{
  switch(GET_RACE(ch)) {
    case RACE_REPTILE:
    case RACE_DRAGON:
    case RACE_DINOSAUR:
    case RACE_SNAKE:
    case RACE_TROGMAN:
    case RACE_SKEXIE:
      return(TRUE);
      break;
    default:
      return(FALSE);
      break;
    }
}

int HasHands( struct char_data *ch)
{

  if (IsHumanoid(ch))
    return(TRUE);
  if (IsUndead(ch)) 
    return(TRUE);
  if (IsLycanthrope(ch)) 
    return(TRUE);
  if (IsDiabolic(ch))
    return(TRUE);
  if (GET_RACE(ch) == RACE_GOLEM || GET_RACE(ch) == RACE_SPECIAL)
    return(TRUE);
  return(FALSE);
}

int IsPerson( struct char_data *ch)
{

  switch(GET_RACE(ch))
    {
    case RACE_HUMAN:
    case RACE_ELVEN:
    case RACE_DROW:
    case RACE_DWARF:
    case RACE_HALFLING:
    case RACE_GNOME:
      return(TRUE);
      break;

    default:
      return(FALSE);
      break;

    }
}

int IsGiantish( struct char_data *ch)
{
  switch(GET_RACE(ch)) {
  case RACE_ENFAN:
  case RACE_GOBLIN:
  case RACE_ORC:
  case RACE_GIANT:
  case RACE_TYTAN:
  case RACE_TROLL:
  case RACE_DRAAGDIM:
    return(TRUE);
  default:
    return(FALSE);
    break;      
  }
}

int IsSmall( struct char_data *ch)
{
  switch(GET_RACE(ch)) {
  case RACE_SMURF:
  case RACE_GNOME:
  case RACE_HALFLING:
  case RACE_GOBLIN:
  case RACE_ENFAN:
    return(TRUE);
  default:
    return(FALSE);
  }
}

int IsGiant ( struct char_data *ch)
{
  switch(GET_RACE(ch)) {
  case RACE_GIANT:
  case RACE_TYTAN:
  case RACE_GOD:
    return(TRUE);
  default:
    return(FALSE);
  }
}

int IsExtraPlanar( struct char_data *ch)
{
  switch(GET_RACE(ch)) {
  case RACE_DEMON:
  case RACE_DEVIL:
  case RACE_PLANAR:
  case RACE_ELEMENT:
  case RACE_ASTRAL:
  case RACE_GOD:
    return(TRUE);
    break;
  default:
    return(FALSE);
    break;
  }
}
int IsOther( struct char_data *ch)
{

  switch(GET_RACE(ch)) {
  case RACE_MFLAYER:
  case RACE_SPECIAL:
  case RACE_GOLEM:
  case RACE_ELEMENT:
  case RACE_PLANAR:
  case RACE_LYCANTH:
    return(TRUE);
  default:
    return(FALSE);
    break;
  }
}
int IsGodly( struct char_data *ch)
{

  if (GET_RACE(ch) == RACE_GOD) return(TRUE);
  if (GET_RACE(ch) == RACE_DEMON || GET_RACE(ch) == RACE_DEVIL)
    if (GetMaxLevel(ch) >= 45)
      return(TRUE);

}

/*
int IsUndead( struct char_data *ch)
int IsUndead( struct char_data *ch)
int IsUndead( struct char_data *ch)
int IsUndead( struct char_data *ch)

*/


void SetHunting( struct char_data *ch, struct char_data *tch)
{
   int persist, dist;
   char buf[256];

#if NOTRACK
return;
#endif
 
   persist =  GetMaxLevel(ch);
   persist *= (int) GET_ALIGNMENT(ch) / 100;

   if (persist < 0)
     persist = -persist;

   dist = GET_ALIGNMENT(tch) - GET_ALIGNMENT(ch);
   dist = (dist > 0) ? dist : -dist;
   if (Hates(ch, tch))
       dist *=2;

   SET_BIT(ch->specials.act, ACT_HUNTING);
   ch->specials.hunting = tch;
   ch->hunt_dist = dist;
   ch->persist = persist;
   ch->old_room = ch->in_room;
#if 0
    if (GetMaxLevel(tch) >= IMMORTAL) {
        sprintf(buf, ">>%s is hunting you from %s\n\r", 
       	   (ch->player.short_descr[0]?ch->player.short_descr:"(null)"),
       	   (real_roomp(ch->in_room)->name[0]?real_roomp(ch->in_room)->name:"(null)"));
        send_to_char(buf, tch);
    }
#endif

}


void CallForGuard
  ( struct char_data *ch, struct char_data *vict, int lev, int area)
{
  struct char_data *i;
  int type1, type2;
  
  switch(area) {
  case MIDGAARD:
    type1 = 3060;
    type2 = 3069;
    break;
  case NEWTHALOS:
    type1 = 3661;
    type2 = 3682;
    break;
  case TROGCAVES:
    type1 = 21114;
    type2 = 21118;
    break;
  case OUTPOST:
    type1 = 21138;
    type2 = 21139;
    break;

  case PRYDAIN:
    type1 = 6606;
    type2 = 6614;
    break;

 default:
    type1 = 3060;
    type2 = 3069;
    break;
  }


  if (lev == 0) lev = 3;

  for (i = character_list; i && lev>0; i = i->next) {
      if (IS_NPC(i) && (i != ch)) {
	 if (!i->specials.fighting) {
	    if (mob_index[i->nr].virtual == type1) {
	       if (number(1,6) == 1) {
	         if (!IS_SET(i->specials.act, ACT_HUNTING)) {
                   if (vict) {
		      SetHunting(i, vict);
                      lev--;
		    }
	         }
	       }
	    } else if (mob_index[i->nr].virtual == type2) {
	       if (number(1,6) == 1) {
	          if (!IS_SET(i->specials.act, ACT_HUNTING)) {
		    if (vict) {
		      SetHunting(i, vict);
	              lev-=2;
		    }
		  }
		}
	    }
	  }
       }
    }
}

void StandUp (struct char_data *ch)
{
   if ((GET_POS(ch)<POSITION_STANDING) && 
       (GET_POS(ch)>POSITION_STUNNED)) {
       if (ch->points.hit > (ch->points.max_hit / 2))
         act("$n quickly stands up.", 1, ch,0,0,TO_ROOM);
       else if (ch->points.hit > (ch->points.max_hit / 6))
         act("$n slowly stands up.", 1, ch,0,0,TO_ROOM);
       else 
         act("$n gets to $s feet very slowly.", 1, ch,0,0,TO_ROOM);
       GET_POS(ch)=POSITION_STANDING;
   }
}


void MakeNiftyAttack( struct char_data *ch)
{
  int num;

  if (!ch->skills)
    SpaceForSkills(ch);

  if (!ch->specials.fighting) return;

  num = number(1,4);
  if (num <= 2) {
      if (!ch->skills[SKILL_BASH].learned)
         ch->skills[SKILL_BASH].learned = 10 + GetMaxLevel(ch)*4;
      do_bash(ch, GET_NAME(ch->specials.fighting), 0);
  } else if (num == 3) {
     if (ch->equipment[WIELD]) {
         if (!ch->skills[SKILL_DISARM].learned)
            ch->skills[SKILL_DISARM].learned = 10 + GetMaxLevel(ch)*4;
         do_disarm(ch, GET_NAME(ch->specials.fighting), 0);
     } else {
       if (!ch->skills[SKILL_KICK].learned)
         ch->skills[SKILL_KICK].learned = 10 + GetMaxLevel(ch)*4;
       do_kick(ch, GET_NAME(ch->specials.fighting), 0);
     }
  } else {
      if (!ch->skills[SKILL_KICK].learned)
         ch->skills[SKILL_KICK].learned = 10 + GetMaxLevel(ch)*4;
      do_kick(ch, GET_NAME(ch->specials.fighting), 0);
   }
}


void FighterMove( struct char_data *ch)
{
  struct char_data *friend;

  if (!ch->skills) {
    SET_BIT(ch->player.class, CLASS_WARRIOR);
    SpaceForSkills(ch);
  }

  if (ch->specials.fighting && ch->specials.fighting->specials.fighting != 0) {
    friend = ch->specials.fighting->specials.fighting;
    if (friend == ch) {
      MakeNiftyAttack(ch);
    } else {
      /* rescue on a 1 or 2, other on a 3 or 4 */
      if (GET_RACE(friend) == (GET_RACE(ch))) {
	 if (GET_HIT(friend) < GET_HIT(ch)) { 
	   if (!ch->skills[SKILL_RESCUE].learned)
	     ch->skills[SKILL_RESCUE].learned = GetMaxLevel(ch)*3+30;
	   do_rescue(ch, GET_NAME(friend), 0);
	 } else {
	   MakeNiftyAttack(ch);
	 }
       } else {
	   MakeNiftyAttack(ch);
       }
    }
  } else {
    return;
  }

}


void MonkMove( struct char_data *ch)
{

  if (!ch->skills) {
    SpaceForSkills(ch);
    ch->skills[SKILL_DODGE].learned = GetMaxLevel(ch)+50;
    SET_BIT(ch->player.class, CLASS_MONK);
  }

  if (!ch->specials.fighting) return;

  if (GET_POS(ch) < POSITION_FIGHTING) {
    if (!ch->skills[SKILL_SPRING_LEAP].learned) 
      ch->skills[SKILL_SPRING_LEAP].learned = (GetMaxLevel(ch)*3)/2+25;
    do_springleap(ch, GET_NAME(ch->specials.fighting), 0);
    return;
  } else {
    char buf[100];

    /* Commented out as a temporary fix to monks fleeing challenges. */
    /* Was easier than rooting around in the spec_proc for the monk */
    /* challenge for it, which is proobably what should be done. */
    /* jdb - was commented back in with the change to use 
       command_interpreter */

    if (GET_HIT(ch) < GET_MAX_HIT(ch)/20) {
      if (!ch->skills[SKILL_RETREAT].learned)  
	ch->skills[SKILL_RETREAT].learned = GetMaxLevel(ch)*2+10;
      strcpy(buf, "flee");
      command_interpreter(ch, buf);
      return;
    } else {

      if (GetMaxLevel(ch)>30 && !number(0,4)) {
	if (GetMaxLevel(ch->specials.fighting) <= GetMaxLevel(ch)) {
	  if (GET_MAX_HIT(ch->specials.fighting) < 2*GET_MAX_HIT(ch)) {
	    if ((!affected_by_spell(ch->specials.fighting, SKILL_QUIV_PALM)) &&
		(!affected_by_spell(ch, SKILL_QUIV_PALM)) && 
		ch->in_room == 551) {
	      if(ch->specials.fighting->skills[SKILL_QUIV_PALM].learned &&
		 ch->in_room == 551) {
		if (!ch->skills[SKILL_QUIV_PALM].learned && ch->in_room == 551)
		  ch->skills[SKILL_QUIV_PALM].learned = GetMaxLevel(ch)*2-5;
		do_quivering_palm(ch, GET_NAME(ch->specials.fighting), 0);
		return;
	      }
	    }
	  }
	}
      }
      if (ch->specials.fighting->equipment[WIELD]) {
	if (!ch->skills[SKILL_DISARM].learned)
	  ch->skills[SKILL_DISARM].learned = (GetMaxLevel(ch)*3)/2+25;
	do_disarm(ch, GET_NAME(ch->specials.fighting), 0);
	return;
      }
      if (!ch->skills[SKILL_KICK].learned)
	ch->skills[SKILL_KICK].learned = (GetMaxLevel(ch)*3)/2+25;
      do_kick(ch, GET_NAME(ch->specials.fighting), 0);
    }
  }
}

void DevelopHatred( struct char_data *ch, struct char_data *v)
{
   int diff, patience, var;

   if (Hates(ch, v))
     return;

  if (ch == v)
    return;

  diff = GET_ALIGNMENT(ch) - GET_ALIGNMENT(v);
  if (diff < 0) diff = -diff;
  
  diff /= 20;

  if (GET_MAX_HIT(ch)) {
     patience = (int) 100 * (float) (GET_HIT(ch) / GET_MAX_HIT(ch));
  } else {
     patience = 10;
  }

  var = number(1,40) - 20;

  if (patience+var < diff)
     AddHated(ch, v);

}

int HasObject( struct char_data *ch, int ob_num)
{
int j, found;
struct obj_data *i;

/*
   equipment too
*/

found = 0;

        for (j=0; j<MAX_WEAR; j++)
     	   if (ch->equipment[j])
       	     found += RecCompObjNum(ch->equipment[j], ob_num);

      if (found > 0)
	return(TRUE);

  /* carrying  */
       	for (i = ch->carrying; i; i = i->next_content)
       	  found += RecCompObjNum(i, ob_num);

     if (found > 0)
       return(TRUE);
     else
       return(FALSE);
}


int room_of_object(struct obj_data *obj)
{
  if (obj->in_room != NOWHERE)
    return obj->in_room;
  else if (obj->carried_by)
    return obj->carried_by->in_room;
  else if (obj->equipped_by)
    return obj->equipped_by->in_room;
  else if (obj->in_obj)
    return room_of_object(obj->in_obj);
  else
    return NOWHERE;
}

struct char_data *char_holding(struct obj_data *obj)
{
  if (obj->in_room != NOWHERE)
    return NULL;
  else if (obj->carried_by)
    return obj->carried_by;
  else if (obj->equipped_by)
    return obj->equipped_by;
  else if (obj->in_obj)
    return char_holding(obj->in_obj);
  else
    return NULL;
}


int RecCompObjNum( struct obj_data *o, int obj_num)
{

int total=0;
struct obj_data *i;

  if (obj_index[o->item_number].virtual == obj_num)
    total = 1;

  if (ITEM_TYPE(o) == ITEM_CONTAINER) {
    for (i = o->contains; i; i = i->next_content)
      total += RecCompObjNum( i, obj_num);
  }
  return(total);

}

void RestoreChar(struct char_data *ch)
{

  GET_MANA(ch) = GET_MAX_MANA(ch);
  GET_HIT(ch) = GET_MAX_HIT(ch);
  GET_MOVE(ch) = GET_MAX_MOVE(ch); 
  if (GetMaxLevel(ch) < LOW_IMMORTAL) {
    GET_COND(ch,THIRST) = 24;
    GET_COND(ch,FULL) = 24;
  } else {
    GET_COND(ch,THIRST) = -1;
    GET_COND(ch,FULL) = -1;
  }

}


void RemAllAffects( struct char_data *ch)
{
  spell_dispel_magic(IMPLEMENTOR,ch,ch,0);

}

int CheckForBlockedMove
  (struct char_data *ch, int cmd, char *arg, int room, int dir, int class)
{
  
  char buf[256], buf2[256];
  
  if (cmd>6 || cmd<1)
    return(FALSE);
  
  strcpy(buf,  "The guard humiliates you, and block your way.\n\r");
  strcpy(buf2, "The guard humiliates $n, and blocks $s way.");
  
  if ((IS_NPC(ch) && (IS_POLICE(ch))) || (GetMaxLevel(ch) >= DEMIGOD) ||
      (IS_AFFECTED(ch, AFF_SNEAK)))
    return(FALSE);


  if ((ch->in_room == room) && (cmd == dir+1)) {
    if (!HasClass(ch,class))  {
      act(buf2, FALSE, ch, 0, 0, TO_ROOM);
      send_to_char(buf, ch);
      return TRUE;
    }  
  }
  return FALSE;

}


void TeleportPulseStuff(int pulse)
{
  
  /*
    check_mobile_activity(pulse);
    Teleport(pulse);
    */
  
  register struct char_data *ch;
  struct char_data *next, *tmp, *bk, *n2;
  int tick, tm;
  struct room_data *rp, *dest;
  struct obj_data *obj_object, *temp_obj;
  
  tm = pulse % PULSE_MOBILE;    /* this is dependent on P_M = 3*P_T */
  
  if (tm == 0) {
    tick = 0;
  } else if (tm == PULSE_TELEPORT) {
    tick = 1;
  } else if (tm == PULSE_TELEPORT*2) {
    tick = 2;
  }
  
  for (ch = character_list; ch; ch = next) {
    next = ch->next;
    if (IS_MOB(ch)) {
      if (ch->specials.tick == tick) {
	mobile_activity(ch);
      }
    } else if (IS_PC(ch)) {
      rp = real_roomp(ch->in_room);
      if (rp &&
	  (rp)->tele_targ > 0 &&
	  rp->tele_targ != rp->number &&
	  (rp)->tele_time > 0 &&
	  (pulse % (rp)->tele_time)==0) {
	
	dest = real_roomp(rp->tele_targ);
	if (!dest) {
	  log("invalid tele_targ");
	  continue;
	}
	
	obj_object = (rp)->contents;
	while (obj_object) {
	  temp_obj = obj_object->next_content;
	  obj_from_room(obj_object);
	  obj_to_room(obj_object, (rp)->tele_targ);
	  obj_object = temp_obj;
	}
	
	bk = 0;

	while(rp->people/* should never fail */) {
	  
	  tmp = rp->people;   /* work through the list of people */
	  if (!tmp) break;

	  if (tmp == bk) break;

	  bk = tmp;

	  char_from_room(tmp); /* the list of people in the room has changed */
	  char_to_room(tmp, rp->tele_targ);

	  if (IS_SET(TELE_LOOK, rp->tele_mask) && IS_PC(tmp)) {
	    do_look(tmp, "\0",15);
	  }

       	  if (IS_SET(dest->room_flags, DEATH) && (!IS_IMMORTAL(tmp))) {
	    if (tmp == next) 
	      next = tmp->next;
	    NailThisSucker(tmp);
	    continue;
	  }
	  if (dest->sector_type == SECT_AIR) {
	    n2 = tmp->next;
	    if (check_falling(tmp)) {	      
	      if (tmp == next)
		next = n2;
	    }
	  }
	}

	if (IS_SET(TELE_COUNT, rp->tele_mask)) {
	  rp->tele_time = 0;   /* reset it for next count */
	}
	if (IS_SET(TELE_RANDOM, rp->tele_mask)) {
	  rp->tele_time = number(1,10)*100;
	}
      }
    }
  }
}

void RiverPulseStuff(int pulse)
{
  /*
    down_river(pulse);
    MakeSound();
    */
  struct descriptor_data *i;
  register struct char_data *ch;
  struct char_data *tmp;
  register struct obj_data *obj_object;
  struct obj_data *next_obj;
  int rd, or;
  char buf[80], buffer[100];
  struct room_data *rp;
  
  if (pulse < 0) 
    return;

  for (i = descriptor_list; i; i=i->next) {
    if (!i->connected) {
      ch = i->character;
      
      if (IS_PC(ch) || RIDDEN(ch)) {
	if (ch->in_room != NOWHERE) {
	  if ((real_roomp(ch->in_room)->sector_type == SECT_WATER_NOSWIM) ||
	      (real_roomp(ch->in_room)->sector_type == SECT_UNDERWATER))
	    if ((real_roomp(ch->in_room))->river_speed > 0) {
	    if ((pulse % (real_roomp(ch->in_room))->river_speed)==0) {
	      if (((real_roomp(ch->in_room))->river_dir<=5)&&
		  ((real_roomp(ch->in_room))->river_dir>=0)) {
		rd = (real_roomp(ch->in_room))->river_dir;
		for (obj_object = (real_roomp(ch->in_room))->contents;
		     obj_object; obj_object = next_obj) {
		  next_obj = obj_object->next_content;
		  if ((real_roomp(ch->in_room))->dir_option[rd]) {
		    obj_from_room(obj_object);
		    obj_to_room(obj_object, 
				(real_roomp(ch->in_room))->dir_option[rd]->to_room);
		  }
		}
		/*
		  flyers don't get moved
		  */
		if(IS_IMMORTAL(ch) &&
		   IS_SET(ch->specials.act, PLR_NOHASSLE)) {
		  send_to_char("The waters swirl and eddy about you.\n\r",ch);
		} else {
		  if(!IS_AFFECTED(ch,AFF_FLYING) ||
		     (real_roomp(ch->in_room)->sector_type == SECT_UNDERWATER)) {
		    if (!MOUNTED(ch)) {
		      rp = real_roomp(ch->in_room);
		      if (rp && rp->dir_option[rd] &&
			  rp->dir_option[rd]->to_room && 
			  (EXIT(ch, rd)->to_room != NOWHERE)) {
			if (ch->specials.fighting) {
			  stop_fighting(ch);
			}
			sprintf(buf, "You drift %s...\n\r", dirs[rd]);
			send_to_char(buf,ch);
			if (RIDDEN(ch))
			  send_to_char(buf,RIDDEN(ch));
			or = ch->in_room;
			char_from_room(ch);
			if (RIDDEN(ch))  {
			  char_from_room(RIDDEN(ch));
			  char_to_room(RIDDEN(ch),
				       (real_roomp(or))->dir_option[rd]->to_room);
			}
			char_to_room(ch,
				     (real_roomp(or))->dir_option[rd]->to_room);
			do_look(ch, "\0",15);
			if (RIDDEN(ch)) {
			  do_look(RIDDEN(ch), "\0",15);
			}
			
			
			if (IS_SET(RM_FLAGS(ch->in_room), DEATH) && 
			    GetMaxLevel(ch) < LOW_IMMORTAL) {
			  NailThisSucker(ch);
			  if (RIDDEN(ch))
			    NailThisSucker(RIDDEN(ch));
			}
		      }
		    }
		  }
		}		/* end of else for is_immort() */
	      }
	    }
	  }
	}
      }
    }
  }

  if (!number(0,4)) {
    for (ch = character_list; ch; ch = tmp) {
      tmp = ch->next;
      
      /*
       *   mobiles
       */	
      if (!IS_PC(ch) && (ch->player.sounds) && (number(0,5)==0)) {
	if (ch->specials.default_pos > POSITION_SLEEPING) {
	  if (GET_POS(ch) > POSITION_SLEEPING) {
	    /*
	     *  Make the sound;
	     */
	    MakeNoise(ch->in_room, ch->player.sounds, 
		      ch->player.distant_snds);
	  } else if (GET_POS(ch) == POSITION_SLEEPING) {
	    /*
	     * snore 
	     */	 
	    sprintf(buffer, "%s snores loudly.\n\r", 
		    ch->player.short_descr);
	    MakeNoise(ch->in_room, buffer, 
		      "You hear a loud snore nearby.\n\r");
	  }
	} else if (GET_POS(ch) == ch->specials.default_pos) {
	  /*
	   * Make the sound
	   */       
	  MakeNoise(ch->in_room, ch->player.sounds, ch->player.distant_snds);
	}
      }
    }      
  }
}

/*
**  Apply soundproof is for ch making noise
*/
int apply_soundproof(struct char_data *ch)
{
  struct room_data *rp;

  if (IS_AFFECTED(ch, AFF_SILENCE)) {
    send_to_char("You are silenced, you can't make a sound!\n\r", ch);
    return(TRUE);
  }

  rp = real_roomp(ch->in_room);

  if (!rp) return(FALSE);  

  if (IS_SET(rp->room_flags, SILENCE)) {
     send_to_char("You are in a silence zone, you can't make a sound!\n\r",ch);
     return(TRUE);   /* for shouts, emotes, etc */
  }

  if (rp->sector_type == SECT_UNDERWATER) {
    send_to_char("Speak underwater, are you mad????\n\r", ch);
    return(TRUE);
  }
  return(FALSE);

}

/*
**  check_soundproof is for others making noise
*/
int check_soundproof(struct char_data *ch)
{
  struct room_data *rp;

  if (IS_AFFECTED(ch, AFF_SILENCE)) {
    return(TRUE);
  }

  rp = real_roomp(ch->in_room);

  if (!rp) return(FALSE);  

  if (IS_SET(rp->room_flags, SILENCE)) {
     return(TRUE);   /* for shouts, emotes, etc */
  }
  if (rp->sector_type == SECT_UNDERWATER)
    return(TRUE);

  return(FALSE);
}

int MobCountInRoom( struct char_data *list)
{
  int i;
  struct char_data *tmp;

  for (i=0, tmp = list; tmp; tmp = tmp->next_in_room, i++)
    ;

  return(i);

}

void *Mymalloc( long size)
{
  if (size < 1) {
    fprintf(stderr, "attempt to malloc negative memory - %d\n", size);
    assert(0);
  }
  return(malloc(size));
}

int SpaceForSkills(struct char_data *ch)
{

  /*
    create space for the skills for some mobile or character.
  */


  ch->skills = (struct char_skill_data *)malloc(MAX_SKILLS*sizeof(struct char_skill_data));

  if (ch->skills == 0)
    assert(0);

}

int CountLims(struct obj_data *obj)
{
  int total=0;

  if (!obj)
    return(0);

  if (obj->contains)
    total += CountLims(obj->contains);
  if (obj->next_content)
    total += CountLims(obj->next_content);
  if (obj->obj_flags.cost_per_day > LIM_ITEM_COST_MIN)
    total+=1;
  return(total);
}


char *lower(char *s)
{
  static char c[1000];
  static char *p;
  int i=0;
  
  strcpy(c, s);

  while (c[i]) {
    if (c[i] < 'a' && c[i] >= 'A' && c[i] <= 'Z')
      c[i] = (char)(int)c[i]+32;
    i++;
  }
  p = c;
  return(p);
}

int getFreeAffSlot( struct obj_data *obj)
{
  int i;

  for (i=0; i < MAX_OBJ_AFFECT; i++)
    if (obj->affected[i].location == APPLY_NONE)
      return(i);

  assert(0);
}

void SetRacialStuff( struct char_data *mob)
{

  switch(GET_RACE(mob)) {
  case RACE_BIRD:
    SET_BIT(mob->specials.affected_by, AFF_FLYING);    
    break;
  case RACE_FISH:
    SET_BIT(mob->specials.affected_by, AFF_WATERBREATH);
    break;
  case RACE_DROW:
  case RACE_DWARF:
  case RACE_GNOME:
  case RACE_MFLAYER:
  case RACE_TROLL:
  case RACE_ORC:
  case RACE_GOBLIN:
  case RACE_HALFLING:
    SET_BIT(mob->specials.affected_by, AFF_INFRAVISION);
    break;
  case RACE_INSECT:
  case RACE_ARACHNID:
    if (IS_PC(mob)) {
      GET_STR(mob) = 18;
      GET_ADD(mob) = 100;
    }
    break;
  case RACE_LYCANTH:
    SET_BIT(mob->M_immune, IMM_NONMAG);
    break;
  case RACE_PREDATOR:
    if (mob->skills)
      mob->skills[SKILL_HUNT].learned = 100;
    break;
    
  default:
    break;
  }

   /* height and weight */
  switch(GET_RACE(mob)) {
  case RACE_HUMAN:
  case RACE_ELVEN:
  case RACE_GNOME:
  case RACE_DWARF:
  case RACE_HALFLING:
  case RACE_LYCANTH:
  case RACE_UNDEAD:
  case RACE_VEGMAN:
  case RACE_MFLAYER:
  case RACE_DROW:
  case RACE_SKEXIE:
  case RACE_TROGMAN:
  case RACE_SARTAN:
  case RACE_PATRYN:
  case RACE_DRAAGDIM:
  case RACE_ASTRAL:
    break;
  case RACE_HORSE:
    mob->player.weight = 400;
    mob->player.height = 175;
  case RACE_ORC:
    mob->player.weight = 150;
    mob->player.height = 140;
    break;
  case RACE_SMURF:
    mob->player.weight = 5;
    mob->player.height = 10;
    break;
  case RACE_GOBLIN:
  case RACE_ENFAN:
    mob->player.weight = 120;
    mob->player.height = 100;
    break;
  case RACE_LABRAT:
  case RACE_INSECT:
  case RACE_ARACHNID:
  case RACE_REPTILE:
  case RACE_DINOSAUR:
  case RACE_FISH:
  case RACE_PREDATOR:
  case RACE_SNAKE:
  case RACE_HERBIV:
  case RACE_VEGGIE:
  case RACE_ELEMENT:
  case RACE_PRIMATE:
  case RACE_GOLEM:
    mob->player.weight = 10+GetMaxLevel(mob)*GetMaxLevel(mob)*2;
    mob->player.height = 20+MIN(mob->player.weight,600);
    break;
  case RACE_DRAGON:
    mob->player.weight = MAX(60, GetMaxLevel(mob)*GetMaxLevel(mob)*2);
    mob->player.height = 100+ MIN(mob->player.weight, 500);
    break;
  case RACE_BIRD:
  case RACE_PARASITE:
  case RACE_SLIME:
  case RACE_GHOST:
    mob->player.weight = GetMaxLevel(mob)*(GetMaxLevel(mob)/5);
    mob->player.height = 10*GetMaxLevel(mob);
    break;
  case RACE_TROLL:
  case RACE_GIANT:
  case RACE_DEMON:
  case RACE_DEVIL:
  case RACE_PLANAR:    
    mob->player.height = 200+GetMaxLevel(mob)*15;
    mob->player.weight = (int)mob->player.height*1.5;
    break;
  case RACE_GOD:
  case RACE_TREE:
  case RACE_TYTAN:
    mob->player.weight = MAX(500, GetMaxLevel(mob)*GetMaxLevel(mob)*10);
    mob->player.height = GetMaxLevel(mob)/2*100;
    break;

  }

}

int check_nomagic(struct char_data *ch, char *msg_ch, char *msg_rm)
{
  struct room_data *rp;

  rp = real_roomp(ch->in_room);
  if (rp && rp->room_flags&NO_MAGIC) {
    if (msg_ch)
      send_to_char(msg_ch, ch);
    if (msg_rm)
      act(msg_rm, FALSE, ch, 0, 0, TO_ROOM);
    return 1;
  }
  return 0;
}

int NumCharmedFollowersInRoom(struct char_data *ch)
{
  struct char_data *t;
  long count = 0;
  struct room_data *rp;

  rp = real_roomp(ch->in_room);
  if (rp) {
    count=0;
    for (t = rp->people;t; t= t->next_in_room) {
      if (IS_AFFECTED(t, AFF_CHARM) && (t->master == ch))
	count++;
    }
    return(count);
  } else return(0);

  return(0);
}


struct char_data *FindMobDiffZoneSameRace(struct char_data *ch)
{
  int num;
  struct char_data *t;
  struct room_data *rp1,*rp2;

  num = number(1,100);

  for (t=character_list;t;t=t->next, num--) {
    if (GET_RACE(t) == GET_RACE(ch) && IS_NPC(t) && !IS_PC(t) && num==0) {
      rp1 = real_roomp(ch->in_room);
      rp2 = real_roomp(t->in_room);
      if (rp1->zone != rp2->zone)
	return(t);
    }
  }
  return(0);
}

int NoSummon(struct char_data *ch)
{
  struct room_data *rp;

  rp = real_roomp(ch->in_room);
  if (!rp) return(TRUE);

  if (IS_SET(rp->room_flags, NO_SUM)) {
    send_to_char("Cryptic powers block your summons.\n\r", ch);
    return(TRUE);
  }

  if (IS_SET(rp->room_flags, TUNNEL)) {
    send_to_char("Strange forces collide in your brain,\n\r", ch);
    send_to_char("Laws of nature twist, and dissapate before\n\r", ch);
    send_to_char("your eyes, strange ideas wrestle with green furry\n\r", ch);
    send_to_char("things, which are crawling up your super-ego...\n\r", ch);
    send_to_char("  You lose a sanity point.\n\r\n\r", ch);
    send_to_char("  OOPS!  Sorry, wronge Genre.  :-) \n\r", ch);
    return(TRUE);
  }

  return(FALSE);
}

int GetNewRace(struct char_file_u *s)
{
  int ok, newrace, i;

  while (1) {
    newrace = number(1,MAX_RACE);
    if (newrace == RACE_UNDEAD)
      continue;
    else {
      ok = TRUE;
      for (i=0;i<MAX_CLASS;i++) {
	if (RacialMax[newrace][i] <= 0) {
	  ok = FALSE;
	  break;
	}
      }
      if (ok) {
	return(newrace);
      }
    }
  }
}

int GetApprox(int num, int perc)
{
  /* perc = 0 - 100 */
  int adj, r;
  float fnum, fadj;

  adj = 100 - perc;
  if (adj < 0) adj = 0;
  adj *=2;  /* percentage of play (+- x%) */

  r = number(1,adj);

  perc += r;

  fnum = (float)num;
  fadj = (float)perc*2;
  fnum *= (float)(fadj/(200.0));

  num = (int)fnum;

  return(num);
}

int MountEgoCheck(struct char_data *ch, struct char_data *horse)
{
  int ride_ego, drag_ego, align, check;

  if (GET_RACE(horse) == RACE_DRAGON) {
    if (ch->skills) {
      drag_ego = GetMaxLevel(horse)*2;
      if (IS_SET(horse->specials.act, ACT_AGGRESSIVE) ||
	  IS_SET(horse->specials.act, ACT_META_AGG)) {
	drag_ego += GetMaxLevel(horse);
      }
      ride_ego = ch->skills[SKILL_RIDE].learned/10 +
	GetMaxLevel(ch)/2;
      if (IS_AFFECTED(ch, AFF_DRAGON_RIDE)) {
	ride_ego += ((GET_INT(ch) + GET_WIS(ch))/2);
      }
      align = GET_ALIGNMENT(ch) - GET_ALIGNMENT(horse);
      if (align < 0) align = -align;
      align/=100;
      align -= 5;
      drag_ego += align;
      if (GET_HIT(horse) > 0)
	drag_ego -= GET_MAX_HIT(horse)/GET_HIT(horse);
      else 
	    drag_ego = 0;
      if (GET_HIT(ch) > 0)
	ride_ego -= GET_MAX_HIT(ch)/GET_HIT(ch);
      else 
	ride_ego = 0;
      
      check = drag_ego+number(1,10)-(ride_ego+number(1,10));
      return(check);
      
    } else {
      return(-GetMaxLevel(horse));
    }
  } else {
    if (!ch->skills) return(-GetMaxLevel(horse));

    drag_ego = GetMaxLevel(horse);

    if (drag_ego > 15) 
      drag_ego *= 2;

    ride_ego = ch->skills[SKILL_RIDE].learned/10 +
      GetMaxLevel(ch);

    if (IS_AFFECTED(ch, AFF_DRAGON_RIDE)) {
      ride_ego += (GET_INT(ch) + GET_WIS(ch));
    }
    check = drag_ego+number(1,5)-(ride_ego+number(1,10));
    return(check);
  }    
} 

int RideCheck( struct char_data *ch, int mod)
{
  if (ch->skills) {
    if (!IS_AFFECTED(ch, AFF_DRAGON_RIDE)) {
      if (number(1,90) > ch->skills[SKILL_RIDE].learned+mod) {
	if (number(1,91-mod) > ch->skills[SKILL_RIDE].learned/2) {
	  if (ch->skills[SKILL_RIDE].learned < 90) {
	    send_to_char("You learn from your mistake\n\r", ch);
	    ch->skills[SKILL_RIDE].learned+=2;
	  }
	}
	return(FALSE);
      }
      return(TRUE);
    } else {
      if (number(1,90) > (ch->skills[SKILL_RIDE].learned+
			  GET_LEVEL(ch, BestMagicClass(ch))+mod))
	return(FALSE);
    }
  } else {
    return(FALSE);
  }
}

void FallOffMount(struct char_data *ch, struct char_data *h)
{
  act("$n loses control and falls off of $N", FALSE, ch, 0, h, TO_NOTVICT);
  act("$n loses control and falls off of you", FALSE, ch, 0, h, TO_VICT);
  act("You lose control and fall off of $N", FALSE, ch, 0, h, TO_CHAR);

}

int EqWBits(struct char_data *ch, int bits)
{
  int i;

  for (i=0;i< MAX_WEAR;i++){
    if (ch->equipment[i] && IS_SET(ch->equipment[i]->obj_flags.extra_flags, bits))
      return(TRUE);
  }
  return(FALSE);
}

int InvWBits(struct char_data *ch, int bits)
{
  struct obj_data *o;

  for (o = ch->carrying;o;o=o->next_content) {
    if (IS_SET(o->obj_flags.extra_flags, bits))
      return(TRUE);
  }
  return(FALSE);
}

int HasWBits(struct char_data *ch, int bits)
{
  if (EqWBits(ch, bits))
    return(TRUE);
  if (InvWBits(ch, bits))
    return(TRUE);
  return(FALSE);
}

int LearnFromMistake(struct char_data *ch, int sknum, int silent, int max)
{
  if (!ch->skills) return(0);

  if (!IS_SET(ch->skills[sknum].flags, SKILL_KNOWN)) {
    if (HasClass(ch, CLASS_MONK)) {
        SET_BIT(ch->skills[sknum].flags, SKILL_KNOWN);
    }
    return(0);
  }

  if (ch->skills[sknum].learned < max && ch->skills[sknum].learned > 0) {
    if (number(1, 101) > ch->skills[sknum].learned/2) {
      if (!silent)
	send_to_char("You learn from your mistake\n\r", ch);
      ch->skills[sknum].learned+=1;
      if (ch->skills[sknum].learned >= max)
	if (!silent)
	  send_to_char("You are now learned in this skill!\n\r", ch);
    }
  }
}

int IsOnPmp(int room_nr)
{
  extern struct zone_data *zone_table;

  if (real_roomp(room_nr)) {
    if (!IS_SET(zone_table[real_roomp(room_nr)->zone].reset_mode, ZONE_ASTRAL))
      return(TRUE);
    return(FALSE);
  } else {
    return(FALSE);
  }

}


int GetSumRaceMaxLevInRoom( struct char_data *ch)
{
  struct room_data *rp;
  struct char_data *i;
  int sum=0;

  rp = real_roomp(ch->in_room);

  if (!rp) return(0);

  for (i = rp->people; i; i=i->next_in_room) {
    if (GET_RACE(i) == GET_RACE(ch)) {
      sum += GetMaxLevel(i);
    }
  }
  return(sum);
}

int too_many_followers(struct char_data *ch) 
{
  struct follow_type *k;
  int max_followers,actual_fol;
  char buf[80];

  max_followers = (int) chr_apply[GET_CHR(ch)].num_fol;
  
  for(k=ch->followers,actual_fol=0; k; k=k->next) 
    if (IS_AFFECTED(k->follower, AFF_CHARM)) 
      actual_fol++;

  if(actual_fol < max_followers)
    return FALSE;
}

int follow_time(struct char_data *ch)
{
  int fol_time=0;
  fol_time= (int) (24*GET_CHR(ch)/11);
  return fol_time;
}

int ItemAlignClash(struct char_data *ch, struct obj_data *obj)
{
  if ((IS_OBJ_STAT(obj, ITEM_ANTI_EVIL) && IS_EVIL(ch)) ||
      (IS_OBJ_STAT(obj, ITEM_ANTI_GOOD) && IS_GOOD(ch)) ||
      (IS_OBJ_STAT(obj, ITEM_ANTI_NEUTRAL) && IS_NEUTRAL(ch))) {
    return(TRUE);
  }
  return(FALSE);
}

int ItemEgoClash(struct char_data *ch, struct obj_data *obj, int bon)
{

  int obj_ego, p_ego, tmp;

  obj_ego = obj->obj_flags.cost_per_day;

  if (obj_ego >= MAX(LIM_ITEM_COST_MIN,14000) || obj_ego < 0) {
    
    if (obj_ego < 0)
      obj_ego = 50000;

    obj_ego /= 666;

/*    
  alignment stuff
    */    

    if (IS_OBJ_STAT(obj, ITEM_ANTI_GOOD) || 
	(IS_OBJ_STAT(obj, ITEM_ANTI_EVIL))) {
      if (IS_NEUTRAL(ch))
	obj_ego += obj_ego/4;
    }

    if (IS_PC(ch)) {
      p_ego = GetMaxLevel(ch)+HowManyClasses(ch);

      if (p_ego > 40) {
	p_ego *= (p_ego-39);
      } else if (p_ego > 20) {
	p_ego += (p_ego -20);
      }
      
    } else {
      p_ego = 10000;
    }

    tmp = GET_INT(ch)+GET_WIS(ch)+GET_CHR(ch);
    tmp /= 3;


    tmp *= GET_HIT(ch);
    tmp /= GET_MAX_HIT(ch);


    return((p_ego + tmp + bon + number(1,6))-(obj_ego+number(1,6)));
  }

  return(1);
}

void IncrementZoneNr(int nr)
{
  struct char_data *c;
  extern struct char_data *character_list;
  extern int top_of_zone_table;

  if (nr > top_of_zone_table)
    return;

  if (nr >= 0) {
    for (c = character_list;c;c=c->next) {
      if (c->specials.zone >= nr)
	c->specials.zone++;
    }
  } else {
    for (c = character_list;c;c=c->next) {
      if (c->specials.zone >= nr)
	c->specials.zone--;
    }    
  }
}

int IsDarkOutside(struct room_data *rp)
{
  extern int gLightLevel;

  if (gLightLevel >= 4)
    return(0);

  if (IS_SET(rp->room_flags, INDOORS) || IS_SET(rp->room_flags, DEATH))
    return(0);

  if (rp->sector_type == SECT_FOREST) {
    if (gLightLevel <= 1)
      return(1);
  } else {
    if (gLightLevel == 0)
      return(0);
  }
}
