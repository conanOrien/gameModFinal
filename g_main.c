
#include "g_local.h"

game_locals_t	game;
level_locals_t	level;
game_import_t	gi;
game_export_t	globals;
spawn_temp_t	st;

int	sm_meat_index;
int	snd_fry;
int meansOfDeath;
int roundNum;

int monsterTime = 100;
int hold;
int roundStart = 0;
int numMonsters;
int powerUpKey;

edict_t		*g_edicts;

cvar_t	*deathmatch;
cvar_t	*coop;
cvar_t	*dmflags;
cvar_t	*skill;
cvar_t	*fraglimit;
cvar_t	*timelimit;
cvar_t	*password;
cvar_t	*spectator_password;
cvar_t	*needpass;
cvar_t	*maxclients;
cvar_t	*maxspectators;
cvar_t	*maxentities;
cvar_t	*g_select_empty;
cvar_t	*dedicated;

cvar_t	*filterban;

cvar_t	*sv_maxvelocity;
cvar_t	*sv_gravity;

cvar_t	*sv_rollspeed;
cvar_t	*sv_rollangle;
cvar_t	*gun_x;
cvar_t	*gun_y;
cvar_t	*gun_z;

cvar_t	*run_pitch;
cvar_t	*run_roll;
cvar_t	*bob_up;
cvar_t	*bob_pitch;
cvar_t	*bob_roll;

cvar_t	*sv_cheats;

cvar_t	*flood_msgs;
cvar_t	*flood_persecond;
cvar_t	*flood_waitdelay;

cvar_t	*sv_maplist;

void SpawnEntities (char *mapname, char *entities, char *spawnpoint);
void ClientThink (edict_t *ent, usercmd_t *cmd);
qboolean ClientConnect (edict_t *ent, char *userinfo);
void ClientUserinfoChanged (edict_t *ent, char *userinfo);
void ClientDisconnect (edict_t *ent);
void ClientBegin (edict_t *ent);
void ClientCommand (edict_t *ent);
void RunEntity (edict_t *ent);
void WriteGame (char *filename, qboolean autosave);
void ReadGame (char *filename);
void WriteLevel (char *filename);
void ReadLevel (char *filename);
void InitGame (void);
void G_RunFrame (void);

void SP_monster_berserk (edict_t *self);
void SP_monster_gladiator (edict_t *self);
void SP_monster_gunner (edict_t *self);
void SP_monster_infantry (edict_t *self);
void SP_monster_soldier_light (edict_t *self);
void SP_monster_soldier (edict_t *self);
void SP_monster_soldier_ss (edict_t *self);
void SP_monster_tank (edict_t *self);
void SP_monster_medic (edict_t *self);
void SP_monster_flipper (edict_t *self);
void SP_monster_chick (edict_t *self);
void SP_monster_parasite (edict_t *self);
void SP_monster_flyer (edict_t *self);
void SP_monster_brain (edict_t *self);
void SP_monster_floater (edict_t *self);
void SP_monster_hover (edict_t *self);
void SP_monster_mutant (edict_t *self);
void SP_monster_supertank (edict_t *self);
void SP_monster_boss2 (edict_t *self);
void SP_monster_jorg (edict_t *self);
void SP_monster_boss3_stand (edict_t *self);
void SelectSpawnPoint (edict_t *ent, vec3_t origin, vec3_t angles);
void walkmonster_start_go (edict_t *self);
void monstSpawn();
void HuntTarget (edict_t *self);

//===================================================================


void ShutdownGame (void)
{
	gi.dprintf ("==== ShutdownGame ====\n");

	gi.FreeTags (TAG_LEVEL);
	gi.FreeTags (TAG_GAME);
}


/*
=================
GetGameAPI

Returns a pointer to the structure with all entry points
and global variables
=================
*/
game_export_t *GetGameAPI (game_import_t *import)
{
	gi = *import;

	globals.apiversion = GAME_API_VERSION;
	globals.Init = InitGame;
	globals.Shutdown = ShutdownGame;
	globals.SpawnEntities = SpawnEntities;

	globals.WriteGame = WriteGame;
	globals.ReadGame = ReadGame;
	globals.WriteLevel = WriteLevel;
	globals.ReadLevel = ReadLevel;

	globals.ClientThink = ClientThink;
	globals.ClientConnect = ClientConnect;
	globals.ClientUserinfoChanged = ClientUserinfoChanged;
	globals.ClientDisconnect = ClientDisconnect;
	globals.ClientBegin = ClientBegin;
	globals.ClientCommand = ClientCommand;

	globals.RunFrame = G_RunFrame;

	globals.ServerCommand = ServerCommand;

	globals.edict_size = sizeof(edict_t);

	return &globals;
}

#ifndef GAME_HARD_LINKED
// this is only here so the functions in q_shared.c and q_shwin.c can link
void Sys_Error (char *error, ...)
{
	va_list		argptr;
	char		text[1024];

	va_start (argptr, error);
	vsprintf (text, error, argptr);
	va_end (argptr);

	gi.error (ERR_FATAL, "%s", text);
}

void Com_Printf (char *msg, ...)
{
	va_list		argptr;
	char		text[1024];

	va_start (argptr, msg);
	vsprintf (text, msg, argptr);
	va_end (argptr);

	gi.dprintf ("%s", text);
}

#endif

//======================================================================


/*
=================
ClientEndServerFrames
=================
*/
void ClientEndServerFrames (void)
{
	int		i;
	edict_t	*ent;

	// calc the player views now that all pushing
	// and damage has been added
	for (i=0 ; i<maxclients->value ; i++)
	{
		ent = g_edicts + 1 + i;
		if (!ent->inuse || !ent->client)
			continue;
		ClientEndServerFrame (ent);
	}

}

/*
=================
CreateTargetChangeLevel

Returns the created target changelevel
=================
*/
edict_t *CreateTargetChangeLevel(char *map)
{
	edict_t *ent;

	ent = G_Spawn ();
	ent->classname = "target_changelevel";
	Com_sprintf(level.nextmap, sizeof(level.nextmap), "%s", map);
	ent->map = level.nextmap;
	return ent;
}

/*
=================
EndDMLevel

The timelimit or fraglimit has been exceeded
=================
*/
void EndDMLevel (void)
{
	edict_t		*ent;
	char *s, *t, *f;
	static const char *seps = " ,\n\r";

	// stay on same level flag
	if ((int)dmflags->value & DF_SAME_LEVEL)
	{
		BeginIntermission (CreateTargetChangeLevel (level.mapname) );
		return;
	}

	// see if it's in the map list
	if (*sv_maplist->string) {
		s = strdup(sv_maplist->string);
		f = NULL;
		t = strtok(s, seps);
		while (t != NULL) {
			if (Q_stricmp(t, level.mapname) == 0) {
				// it's in the list, go to the next one
				t = strtok(NULL, seps);
				if (t == NULL) { // end of list, go to first one
					if (f == NULL) // there isn't a first one, same level
						BeginIntermission (CreateTargetChangeLevel (level.mapname) );
					else
						BeginIntermission (CreateTargetChangeLevel (f) );
				} else
					BeginIntermission (CreateTargetChangeLevel (t) );
				free(s);
				return;
			}
			if (!f)
				f = t;
			t = strtok(NULL, seps);
		}
		free(s);
	}

	if (level.nextmap[0]) // go to a specific map
		BeginIntermission (CreateTargetChangeLevel (level.nextmap) );
	else {	// search for a changelevel
		ent = G_Find (NULL, FOFS(classname), "target_changelevel");
		if (!ent)
		{	// the map designer didn't include a changelevel,
			// so create a fake ent that goes back to the same level
			BeginIntermission (CreateTargetChangeLevel (level.mapname) );
			return;
		}
		BeginIntermission (ent);
	}
}


/*
=================
CheckNeedPass
=================
*/
void CheckNeedPass (void)
{
	int need;

	// if password or spectator_password has changed, update needpass
	// as needed
	if (password->modified || spectator_password->modified) 
	{
		password->modified = spectator_password->modified = false;

		need = 0;

		if (*password->string && Q_stricmp(password->string, "none"))
			need |= 1;
		if (*spectator_password->string && Q_stricmp(spectator_password->string, "none"))
			need |= 2;

		gi.cvar_set("needpass", va("%d", need));
	}
}

/*
=================
CheckDMRules
=================
*/
void CheckDMRules (void)
{
	int			i;
	gclient_t	*cl;

	if(level.total_monsters == 0)
	{
		roundNum = 0;
	}

	if (level.intermissiontime)
		return;

	if (!deathmatch->value)
		return;

	if (timelimit->value)
	{
		if (level.time >= timelimit->value*60)
		{
			gi.bprintf (PRINT_HIGH, "Timelimit hit.\n");
			EndDMLevel ();
			return;
		}
	}

	if (fraglimit->value)
	{
		for (i=0 ; i<maxclients->value ; i++)
		{
			cl = game.clients + i;
			if (!g_edicts[i+1].inuse)
				continue;

			if (cl->resp.score >= fraglimit->value)
			{
				gi.bprintf (PRINT_HIGH, "Fraglimit hit.\n");
				EndDMLevel ();
				return;
			}
		}
	}
}


/*
=============
ExitLevel
=============
*/
void ExitLevel (void)
{
	int		i;
	edict_t	*ent;
	char	command [256];

	Com_sprintf (command, sizeof(command), "gamemap \"%s\"\n", level.changemap);
	gi.AddCommandString (command);
	level.changemap = NULL;
	level.exitintermission = 0;
	level.intermissiontime = 0;
	ClientEndServerFrames ();

	// clear some things before going to next level
	for (i=0 ; i<maxclients->value ; i++)
	{
		ent = g_edicts + 1 + i;
		if (!ent->inuse)
			continue;
		if (ent->health > ent->client->pers.max_health)
			ent->health = ent->client->pers.max_health;
	}

}

/*
================
G_RunFrame

Advances the world by 0.1 seconds
================
*/
void G_RunFrame (void)
{
//	gitem_t *it;
//	edict_t *it_ent;
	int		i;
	edict_t	*ent;

	level.framenum++;
	level.time = level.framenum*FRAMETIME;

	// choose a client for monsters to target this frame
	AI_SetSightClient ();

	//DM code
	if (deathmatch->value)
	{	
//		gi.bprintf(PRINT_HIGH, "Round %i\n", roundNum);

		if(monsterTime > 0)
		{
			monsterTime --;
		}		
		if(monsterTime == 0 && hold != 1 )
		{
			monstSpawn();
		}

		//Round Control logic ow5
		if(roundNum == 1 && (level.killed_monsters == level.total_monsters))
		{
			//Print congrats
			//notify of/advance to next round
			//grant powerups
			gi.bprintf(PRINT_HIGH, "Round 1 Complete.. Enjoy your powerups \nBeginning Round 2\n");
			powerUpKey = 1;
			roundNum ++;
			hold = 0;
			monstSpawn();
		}
		if(roundNum == 2 && (level.killed_monsters == level.total_monsters))
		{
			//Print congrats
			//notify of/advance to next round
			//grant powerups
			gi.bprintf(PRINT_HIGH, "Round 2 Complete.. Enjoy your powerups \nBeginning Round 3\n");
			powerUpKey = 2;
			roundNum ++;
			hold = 0;
			monstSpawn();
		}
		if(roundNum == 3 && (level.killed_monsters == level.total_monsters))
		{
			//Print congrats
			//notify of/advance to next round
			//grant powerups
			gi.bprintf(PRINT_HIGH, "Round 3 Complete.. Enjoy your powerups \nBeginning Final Round\n");
			roundNum ++;
			powerUpKey = 3;
			hold = 0;
			monstSpawn();
		}
		if(roundNum == 4 && (level.killed_monsters == level.total_monsters))
		{
			//Print congrats
			//notify of/advance to next round
			//grant powerups
			gi.bprintf(PRINT_HIGH, "Final Round Complete. Conglaturations! You've made Happy end!");
		}
}



		
	// exit intermissions

	if (level.exitintermission)
	{
		ExitLevel ();
		return;
	}

	//
	// treat each object in turn
	// even the world gets a chance to think
	//
	ent = &g_edicts[0];
	for (i=0 ; i<globals.num_edicts ; i++, ent++)
	{
		if (!ent->inuse)
			continue;

		level.current_entity = ent;

		VectorCopy (ent->s.origin, ent->s.old_origin);

		// if the ground entity moved, make sure we are still on it
		if ((ent->groundentity) && (ent->groundentity->linkcount != ent->groundentity_linkcount))
		{
			ent->groundentity = NULL;
			if ( !(ent->flags & (FL_SWIM|FL_FLY)) && (ent->svflags & SVF_MONSTER) )
			{
				M_CheckGround (ent);
			}
		}

		if (i > 0 && i <= maxclients->value)
		{
			ClientBeginServerFrame (ent);
			continue;
		}

		G_RunEntity (ent);
	}

	// see if it is time to end a deathmatch
	CheckDMRules ();

	// see if needpass needs updated
	CheckNeedPass ();

	// build the playerstate_t structures for all players
	ClientEndServerFrames ();
}

void monstSpawn()
{
	int dice;
	if(level.total_monsters == 10 ||level.total_monsters == 20 || level.total_monsters == 30|| level.total_monsters == 40)
		{
			roundStart = 0;
			hold = 1;
		}
	
	if(roundNum == 0)
	{
		roundNum = 1;
	}
	if(roundNum == 1)
	{
		//spawn number of monsters for first round
		//have a random number gen pick the monster that is spawned, 4 possible options so a 1-4 rand
		//when monsters are dead, increment numRound
		if(roundStart == 0)
		{
			numMonsters = 10;
			gi.bprintf(PRINT_HIGH, "Round 1 - Total Monsters: 10");
			roundStart ++;
		}
		while(((level.total_monsters-level.killed_monsters) < numMonsters) && monsterTime == 0)
		{
			dice = (rand() % 4+1-1)+1;
			if(dice == 1)
			{
				edict_t *monster;
				vec3_t origin,angles;
				monster = G_Spawn();
				
				SelectSpawnPoint (monster, origin, angles);
				monster->classname = "monster_soldier";
				monster->think = HuntTarget;
				VectorCopy(origin,monster->s.origin);
				SP_monster_soldier(monster);
				monsterTime = 10;
			}
			if(dice == 3)
			{
				edict_t *monster;
				vec3_t origin,angles;
				monster = G_Spawn();
				
				SelectSpawnPoint (monster, origin, angles);
				monster->classname = "monster_berserk";
				monster->think = walkmonster_start_go;
				VectorCopy(origin,monster->s.origin);
				SP_monster_berserk(monster);
				monsterTime = 10;
			}
			if(dice == 2)
			{
				edict_t *monster;
				vec3_t origin,angles;
				monster = G_Spawn();
				
				SelectSpawnPoint (monster, origin, angles);
				monster->classname = "monster_soldier_light";
				monster->think = walkmonster_start_go;
				VectorCopy(origin,monster->s.origin);
				SP_monster_soldier_light(monster);
				monsterTime = 10;
			}
			if(dice == 4)
			{
				edict_t *monster;
				vec3_t origin,angles;
				monster = G_Spawn();
				
				SelectSpawnPoint (monster, origin, angles);
				monster->classname = "monster_soldier_ss";
				monster->think = walkmonster_start_go;
				VectorCopy(origin,monster->s.origin);
				SP_monster_soldier_ss(monster);
				monsterTime = 10;
			}
		}
	}
	if(roundNum == 2)
	{
		if(roundStart == 0)
		{
			numMonsters = 10;
			gi.bprintf(PRINT_HIGH, "Round 2 - Total Monsters: 10");
			roundStart ++;
		}
		while((level.total_monsters-level.killed_monsters) < numMonsters && monsterTime == 0)
		{
			dice = (rand() % 4+1-1)+1;
			if(dice == 1)
			{
				edict_t *monster;
				vec3_t origin,angles;
				monster = G_Spawn();
				
				SelectSpawnPoint (monster, origin, angles);
				monster->classname = "monster_parasite";
				VectorCopy(origin,monster->s.origin);
				SP_monster_parasite(monster);
				walkmonster_start_go(monster);
				monsterTime = 10;
			}
			if(dice == 3)
			{
				edict_t *monster;
				vec3_t origin,angles;
				monster = G_Spawn();
				
				SelectSpawnPoint (monster, origin, angles);
				monster->classname = "monster_berserk";
				VectorCopy(origin,monster->s.origin);
				SP_monster_berserk(monster);
				walkmonster_start_go(monster);
				monsterTime = 10;
			}
			if(dice == 2)
			{
				edict_t *monster;
				vec3_t origin,angles;
				monster = G_Spawn();
				
				SelectSpawnPoint (monster, origin, angles);
				monster->classname = "monster_mutant";
				VectorCopy(origin,monster->s.origin);
				SP_monster_mutant(monster);
				walkmonster_start_go(monster);
				monsterTime = 10;
			}
			if(dice == 4)
			{
				edict_t *monster;
				vec3_t origin,angles;
				monster = G_Spawn();
				
				SelectSpawnPoint (monster, origin, angles);
				monster->classname = "monster_soldier_ss";
				VectorCopy(origin,monster->s.origin);
				SP_monster_soldier_ss(monster);
				walkmonster_start_go(monster);
				monsterTime = 10;
			}
		}
	}
	if(roundNum == 3)
	{
		if(roundStart == 0)
		{
			numMonsters = 10;
			gi.bprintf(PRINT_HIGH, "Round 3 - Total Monsters: 10");
			roundStart ++;
		}
		while((level.total_monsters-level.killed_monsters) < numMonsters && monsterTime == 0)
		{
			dice = (rand() % 4+1-1)+1;
			if(dice == 1)
			{
				edict_t *monster;
				vec3_t origin,angles;
				monster = G_Spawn();
				
				SelectSpawnPoint (monster, origin, angles);
				monster->classname = "monster_parasite";
				VectorCopy(origin,monster->s.origin);
				SP_monster_parasite(monster);
				walkmonster_start_go(monster);
				monsterTime = 10;
			}
			if(dice == 3)
			{
				edict_t *monster;
				vec3_t origin,angles;
				monster = G_Spawn();
				
				SelectSpawnPoint (monster, origin, angles);
				monster->classname = "monster_gunner";
				VectorCopy(origin,monster->s.origin);
				SP_monster_gunner(monster);
				walkmonster_start_go(monster);
				monsterTime = 10;
			}
			if(dice == 2)
			{
				edict_t *monster;
				vec3_t origin,angles;
				monster = G_Spawn();
				
				SelectSpawnPoint (monster, origin, angles);
				monster->classname = "monster_mutant";
				VectorCopy(origin,monster->s.origin);
				SP_monster_mutant(monster);
				walkmonster_start_go(monster);
				monsterTime = 10;
			}
			if(dice == 4)
			{
				edict_t *monster;
				vec3_t origin,angles;
				monster = G_Spawn();
				
				SelectSpawnPoint (monster, origin, angles);
				monster->classname = "monster_infantry";
				VectorCopy(origin,monster->s.origin);
				SP_monster_infantry(monster);
				walkmonster_start_go(monster);
				monsterTime = 10;
			}
		}
	}
	if(roundNum == 4)
	{
		if(roundStart == 0)
		{
			numMonsters = 10;
			gi.bprintf(PRINT_HIGH, "Round 4 - Total Monsters: 10");
			roundStart ++;
		}
		while((level.total_monsters-level.killed_monsters) < numMonsters)
		{
			dice = (rand() % 4+1-1)+1;
			if(dice == 1)
			{
				edict_t *monster;
				vec3_t origin,angles;
				monster = G_Spawn();
				
				SelectSpawnPoint (monster, origin, angles);
				monster->classname = "monster_gladiator";
				VectorCopy(origin,monster->s.origin);
				SP_monster_gladiator(monster);
				walkmonster_start_go(monster);
			}
			if(dice == 3)
			{
				edict_t *monster;
				vec3_t origin,angles;
				monster = G_Spawn();
				
				SelectSpawnPoint (monster, origin, angles);
				monster->classname = "monster_boss2";
				VectorCopy(origin,monster->s.origin);
				SP_monster_boss2(monster);
				walkmonster_start_go(monster);
			}
			if(dice == 2)
			{
				edict_t *monster;
				vec3_t origin,angles;
				monster = G_Spawn();
				
				SelectSpawnPoint (monster, origin, angles);
				monster->classname = "monster_parasite";
				VectorCopy(origin,monster->s.origin);
				SP_monster_parasite(monster);
				walkmonster_start_go(monster);
			}
			if(dice == 4)
			{
				edict_t *monster;
				vec3_t origin,angles;
				monster = G_Spawn();
				
				SelectSpawnPoint (monster, origin, angles);
				monster->classname = "monster_mutant";
				VectorCopy(origin,monster->s.origin);
				SP_monster_mutant(monster);
				walkmonster_start_go(monster);
			}
		}
	}
}

