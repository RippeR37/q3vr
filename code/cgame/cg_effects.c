/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
//
// cg_effects.c -- these functions generate localentities, usually as a result
// of event processing

#include "cg_local.h"
#include "../vr/vr_clientinfo.h"

extern vr_clientinfo_t* vr;

/*
==================
CG_BubbleTrail

Bullets shot underwater
==================
*/
void CG_BubbleTrail( vec3_t start, vec3_t end, float spacing ) {
	vec3_t		move;
	vec3_t		vec;
	float		len;
	int			i;

	if ( cg_noProjectileTrail.integer ) {
		return;
	}

	VectorCopy (start, move);
	VectorSubtract (end, start, vec);
	len = VectorNormalize (vec);

	// advance a random amount first
	i = rand() % (int)spacing;
	VectorMA( move, i, vec, move );

	VectorScale (vec, spacing, vec);

	for ( ; i < len; i += spacing ) {
		localEntity_t	*le;
		refEntity_t		*re;

		le = CG_AllocLocalEntity();
		le->leFlags = LEF_PUFF_DONT_SCALE;
		le->leType = LE_MOVE_SCALE_FADE;
		le->startTime = cg.time;
		le->endTime = cg.time + 1000 + random() * 250;
		le->lifeRate = 1.0 / ( le->endTime - le->startTime );

		re = &le->refEntity;
		re->shaderTime = cg.time / 1000.0f;

		re->reType = RT_SPRITE;
		re->rotation = 0;
		re->radius = 3;
		re->customShader = cgs.media.waterBubbleShader;
		re->shaderRGBA[0] = 0xff;
		re->shaderRGBA[1] = 0xff;
		re->shaderRGBA[2] = 0xff;
		re->shaderRGBA[3] = 0xff;

		le->color[3] = 1.0;

		le->pos.trType = TR_LINEAR;
		le->pos.trTime = cg.time;
		VectorCopy( move, le->pos.trBase );
		le->pos.trDelta[0] = crandom()*5;
		le->pos.trDelta[1] = crandom()*5;
		le->pos.trDelta[2] = crandom()*5 + 6;

		VectorAdd (move, vec, move);
	}
}

/*
=====================
CG_SmokePuff

Adds a smoke puff or blood trail localEntity.
=====================
*/
localEntity_t *CG_SmokePuff( const vec3_t p, const vec3_t vel, 
				   float radius,
				   float r, float g, float b, float a,
				   float duration,
				   int startTime,
				   int fadeInTime,
				   int leFlags,
				   qhandle_t hShader ) {
	static int	seed = 0x92;
	localEntity_t	*le;
	refEntity_t		*re;
//	int fadeInTime = startTime + duration / 2;

	le = CG_AllocLocalEntity();
	le->leFlags = leFlags;
	le->radius = radius;

	re = &le->refEntity;
	re->rotation = Q_random( &seed ) * 360;
	re->radius = radius;
	re->shaderTime = startTime / 1000.0f;

	le->leType = LE_MOVE_SCALE_FADE;
	le->startTime = startTime;
	le->fadeInTime = fadeInTime;
	le->endTime = startTime + duration;
	if ( fadeInTime > startTime ) {
		le->lifeRate = 1.0 / ( le->endTime - le->fadeInTime );
	}
	else {
		le->lifeRate = 1.0 / ( le->endTime - le->startTime );
	}
	le->color[0] = r;
	le->color[1] = g; 
	le->color[2] = b;
	le->color[3] = a;


	le->pos.trType = TR_LINEAR;
	le->pos.trTime = startTime;
	VectorCopy( vel, le->pos.trDelta );
	VectorCopy( p, le->pos.trBase );

	VectorCopy( p, re->origin );
	re->customShader = hShader;

	// rage pro can't alpha fade, so use a different shader
	if ( cgs.glconfig.hardwareType == GLHW_RAGEPRO ) {
		re->customShader = cgs.media.smokePuffRageProShader;
		re->shaderRGBA[0] = 0xff;
		re->shaderRGBA[1] = 0xff;
		re->shaderRGBA[2] = 0xff;
		re->shaderRGBA[3] = 0xff;
	} else {
		re->shaderRGBA[0] = le->color[0] * 0xff;
		re->shaderRGBA[1] = le->color[1] * 0xff;
		re->shaderRGBA[2] = le->color[2] * 0xff;
		re->shaderRGBA[3] = 0xff;
	}

	re->reType = RT_SPRITE;
	re->radius = le->radius;

	return le;
}

/*
==================
CG_SpawnEffect

Player teleporting in or out
==================
*/
void CG_SpawnEffect( vec3_t org ) {
	localEntity_t	*le;
	refEntity_t		*re;

	le = CG_AllocLocalEntity();
	le->leFlags = 0;
	le->leType = LE_FADE_RGB;
	le->startTime = cg.time;
	le->endTime = cg.time + 500;
	le->lifeRate = 1.0 / ( le->endTime - le->startTime );

	le->color[0] = le->color[1] = le->color[2] = le->color[3] = 1.0;

	re = &le->refEntity;

	re->reType = RT_MODEL;
	re->shaderTime = cg.time / 1000.0f;

#ifndef MISSIONPACK
	re->customShader = cgs.media.teleportEffectShader;
#endif
	re->hModel = cgs.media.teleportEffectModel;
	AxisClear( re->axis );

	VectorCopy( org, re->origin );
#ifdef MISSIONPACK
	re->origin[2] += 16;
#else
	re->origin[2] -= 24;
#endif
}


#ifdef MISSIONPACK
/*
===============
CG_LightningBoltBeam
===============
*/
void CG_LightningBoltBeam( vec3_t start, vec3_t end ) {
	localEntity_t	*le;
	refEntity_t		*beam;

	le = CG_AllocLocalEntity();
	le->leFlags = 0;
	le->leType = LE_SHOWREFENTITY;
	le->startTime = cg.time;
	le->endTime = cg.time + 50;

	beam = &le->refEntity;

	VectorCopy( start, beam->origin );
	// this is the end point
	VectorCopy( end, beam->oldorigin );

	beam->reType = RT_LIGHTNING;
	beam->customShader = cgs.media.lightningShader;
}

/*
==================
CG_KamikazeEffect
==================
*/
void CG_KamikazeEffect( vec3_t org ) {
	localEntity_t	*le;
	refEntity_t		*re;

	le = CG_AllocLocalEntity();
	le->leFlags = 0;
	le->leType = LE_KAMIKAZE;
	le->startTime = cg.time;
	le->endTime = cg.time + 3000;//2250;
	le->lifeRate = 1.0 / ( le->endTime - le->startTime );

	le->color[0] = le->color[1] = le->color[2] = le->color[3] = 1.0;

	VectorClear(le->angles.trBase);

	re = &le->refEntity;

	re->reType = RT_MODEL;
	re->shaderTime = cg.time / 1000.0f;

	re->hModel = cgs.media.kamikazeEffectModel;

	VectorCopy( org, re->origin );

}

/*
==================
CG_ObeliskExplode
==================
*/
void CG_ObeliskExplode( vec3_t org, int entityNum ) {
	localEntity_t	*le;
	vec3_t origin;

	// create an explosion
	VectorCopy( org, origin );
	origin[2] += 64;
	le = CG_MakeExplosion( origin, vec3_origin,
						   cgs.media.dishFlashModel,
						   cgs.media.rocketExplosionShader,
						   600, qtrue );
	le->light = 300;
	le->lightColor[0] = 1;
	le->lightColor[1] = 0.75;
	le->lightColor[2] = 0.0;
}

/*
==================
CG_ObeliskPain
==================
*/
void CG_ObeliskPain( vec3_t org ) {
	float r;
	sfxHandle_t sfx;

	// hit sound
	r = rand() & 3;
	if ( r < 2 ) {
		sfx = cgs.media.obeliskHitSound1;
	} else if ( r == 2 ) {
		sfx = cgs.media.obeliskHitSound2;
	} else {
		sfx = cgs.media.obeliskHitSound3;
	}
	trap_S_StartSound ( org, ENTITYNUM_NONE, CHAN_BODY, sfx );
}


/*
==================
CG_InvulnerabilityImpact
==================
*/
void CG_InvulnerabilityImpact( vec3_t org, vec3_t angles ) {
	localEntity_t	*le;
	refEntity_t		*re;
	int				r;
	sfxHandle_t		sfx;

	le = CG_AllocLocalEntity();
	le->leFlags = 0;
	le->leType = LE_INVULIMPACT;
	le->startTime = cg.time;
	le->endTime = cg.time + 1000;
	le->lifeRate = 1.0 / ( le->endTime - le->startTime );

	le->color[0] = le->color[1] = le->color[2] = le->color[3] = 1.0;

	re = &le->refEntity;

	re->reType = RT_MODEL;
	re->shaderTime = cg.time / 1000.0f;

	re->hModel = cgs.media.invulnerabilityImpactModel;

	VectorCopy( org, re->origin );
	AnglesToAxis( angles, re->axis );

	r = rand() & 3;
	if ( r < 2 ) {
		sfx = cgs.media.invulnerabilityImpactSound1;
	} else if ( r == 2 ) {
		sfx = cgs.media.invulnerabilityImpactSound2;
	} else {
		sfx = cgs.media.invulnerabilityImpactSound3;
	}
	trap_S_StartSound (org, ENTITYNUM_NONE, CHAN_BODY, sfx );
}

/*
==================
CG_InvulnerabilityJuiced
==================
*/
void CG_InvulnerabilityJuiced( vec3_t org ) {
	localEntity_t	*le;
	refEntity_t		*re;
	vec3_t			angles;

	le = CG_AllocLocalEntity();
	le->leFlags = 0;
	le->leType = LE_INVULJUICED;
	le->startTime = cg.time;
	le->endTime = cg.time + 10000;
	le->lifeRate = 1.0 / ( le->endTime - le->startTime );

	le->color[0] = le->color[1] = le->color[2] = le->color[3] = 1.0;

	re = &le->refEntity;

	re->reType = RT_MODEL;
	re->shaderTime = cg.time / 1000.0f;

	re->hModel = cgs.media.invulnerabilityJuicedModel;

	VectorCopy( org, re->origin );
	VectorClear(angles);
	AnglesToAxis( angles, re->axis );

	trap_S_StartSound (org, ENTITYNUM_NONE, CHAN_BODY, cgs.media.invulnerabilityJuicedSound );
}

#endif

/*
==================
CG_ScorePlum
==================
*/
void CG_ScorePlum( int client, vec3_t org, int score ) {
	localEntity_t	*le;
	refEntity_t		*re;
	vec3_t			angles;
	static vec3_t lastPos;

	// only visualize for the client that scored
	if (client != cg.predictedPlayerState.clientNum || cg_scorePlum.integer == 0) {
		return;
	}

	le = CG_AllocLocalEntity();
	le->leFlags = 0;
	le->leType = LE_SCOREPLUM;
	le->startTime = cg.time;
	le->endTime = cg.time + 4000;
	le->lifeRate = 1.0 / ( le->endTime - le->startTime );


	le->color[0] = le->color[1] = le->color[2] = le->color[3] = 1.0;
	le->radius = score;

	VectorCopy( org, le->pos.trBase );
	if (org[2] >= lastPos[2] - 20 && org[2] <= lastPos[2] + 20) {
		le->pos.trBase[2] -= 20;
	}

	//CG_Printf( "Plum origin %i %i %i -- %i\n", (int)org[0], (int)org[1], (int)org[2], (int)Distance(org, lastPos));
	VectorCopy(org, lastPos);


	re = &le->refEntity;

	re->reType = RT_SPRITE;
	re->radius = 16;

	VectorClear(angles);
	AnglesToAxis( angles, re->axis );
}

/*
==================
CG_DamagePlum
==================
*/
void CG_DamagePlum( vec3_t org, int damage ) {
	localEntity_t	*le;
	refEntity_t		*re;
	vec3_t			angles;
	float			random_x, random_y;

	if ( cg_damagePlums.integer == 0 ) {
		return;
	}

	le = CG_AllocLocalEntity();
	le->leFlags = 0;
	le->leType = LE_DAMAGEPLUM;
	le->startTime = cg.time;
	le->endTime = cg.time + 1000;
	le->lifeRate = 1.0 / ( le->endTime - le->startTime );

	le->color[0] = 1.0;
	le->color[1] = 0.5;
	le->color[2] = 0.0;
	le->color[3] = 1.0;
	le->radius = damage;

	VectorCopy( org, le->pos.trBase );

	random_x = (random() * 2.0 - 1.0);
	random_y = (random() * 2.0 - 1.0);

	le->pos.trDelta[0] = random_x;
	le->pos.trDelta[1] = random_y;
	le->pos.trDelta[2] = 0.5 + random() * 0.5;

	re = &le->refEntity;

	re->reType = RT_SPRITE;
	re->radius = 16;

	VectorClear(angles);
	AnglesToAxis( angles, re->axis );
}


/*
====================
CG_MakeExplosion
====================
*/
localEntity_t *CG_MakeExplosion( vec3_t origin, vec3_t dir, 
								qhandle_t hModel, qhandle_t shader,
								int msec, qboolean isSprite ) {
	float			ang;
	localEntity_t	*ex;
	int				offset;
	vec3_t			tmpVec, newOrigin;

	if ( msec <= 0 ) {
		CG_Error( "CG_MakeExplosion: msec = %i", msec );
	}

	// skew the time a bit so they aren't all in sync
	offset = rand() & 63;

	ex = CG_AllocLocalEntity();
	if ( isSprite ) {
		ex->leType = LE_SPRITE_EXPLOSION;

		// randomly rotate sprite orientation
		ex->refEntity.rotation = rand() % 360;
		VectorScale( dir, 16, tmpVec );
		VectorAdd( tmpVec, origin, newOrigin );
	} else {
		ex->leType = LE_EXPLOSION;
		VectorCopy( origin, newOrigin );

		// set axis with random rotate
		if ( !dir ) {
			AxisClear( ex->refEntity.axis );
		} else {
			ang = rand() % 360;
			VectorCopy( dir, ex->refEntity.axis[0] );
			RotateAroundDirection( ex->refEntity.axis, ang );
		}
	}

	ex->startTime = cg.time - offset;
	ex->endTime = ex->startTime + msec;

	// bias the time so all shader effects start correctly
	ex->refEntity.shaderTime = ex->startTime / 1000.0f;

	ex->refEntity.hModel = hModel;
	ex->refEntity.customShader = shader;

	// set origin
	VectorCopy( newOrigin, ex->refEntity.origin );
	VectorCopy( newOrigin, ex->refEntity.oldorigin );

	ex->color[0] = ex->color[1] = ex->color[2] = 1.0;

	return ex;
}


/*
=================
CG_Bleed

This is the spurt of blood when a character gets hit.
Spawns multiple blood droplet particles that spray along the impact direction.
dir = direction the projectile was traveling (NULL for omnidirectional spray)
weapon = weapon type for determining particle count
=================
*/
#define BLOOD_PARTICLE_SPEED_EXIT	600
#define BLOOD_PARTICLE_SPEED_ENTRY	50

static int CG_BloodParticleCount( int weapon ) {
	// Returns droplet count (reduced by 1 since we also spawn a puff)
	switch ( weapon ) {
		case WP_PLASMAGUN:
			return 4;
		case WP_BFG:
			return 7;
		case WP_ROCKET_LAUNCHER:
		case WP_GRENADE_LAUNCHER:
			return 9;
		// Weapons that trigger CG_Bleed via EV_MISSILE_HIT or EV_BULLET_HIT_FLESH:
		// Gauntlet, machine gun, shotgun, lightning gun, or even grappling hook
		// Railgun doesn't trigger a blood event.
		default:
			return 2;
	}
}

void CG_Bleed( vec3_t origin, vec3_t dir, int entityNum, int weapon ) {
	localEntity_t	*le;
	refEntity_t		*re;
	int				i;
	int				particleCount;
	vec3_t			velocity;
	vec3_t			baseDir;
	vec3_t			perpA, perpB;
	float			spread, forwardBias;
	float			speed;
	qboolean		isPlayer;

	if ( !cg_blood.integer ) {
		return;
	}

	isPlayer = ( entityNum == cg.snap->ps.clientNum );

	// If particles disabled, use original sprite-based blood effect
	if ( !cg_bloodParticles.integer ) {
		le = CG_AllocLocalEntity();
		le->leType = LE_EXPLOSION;
		le->startTime = cg.time;
		le->endTime = le->startTime + 500;

		VectorCopy( origin, le->refEntity.origin );
		le->refEntity.reType = RT_SPRITE;
		le->refEntity.rotation = rand() % 360;
		le->refEntity.radius = 24;
		le->refEntity.customShader = cgs.media.bloodExplosionShader;

		// don't show player's own blood in view
		if ( isPlayer ) {
			le->refEntity.renderfx |= RF_THIRD_PERSON;
		}
		return;
	}

	qboolean inLiquid = ( CG_PointContents( origin, -1 ) & MASK_WATER ) != 0;

	if ( inLiquid ) {
		// Single puff underwater
		float puffRadius = 2 + random() * 3;  // 2-5 units
		int puffDuration = 300 + random() * 200;  // 300-500ms

		le = CG_SmokePuff( origin, vec3_origin,
			puffRadius,
			1, 1, 1, 1,
			puffDuration,
			cg.time, 0, 0,
			cgs.media.bloodTrailShader );
		le->leType = LE_FALL_SCALE_FADE;
		le->pos.trDelta[2] = -2;  // Slow rise

		if ( isPlayer ) {
			le->refEntity.renderfx |= RF_THIRD_PERSON;
		}
		return;
	}

	particleCount = CG_BloodParticleCount( weapon );

	// Set up directional basis if we have a direction
	if ( dir && ( dir[0] != 0 || dir[1] != 0 || dir[2] != 0 ) ) {
		VectorNormalize2( dir, baseDir );
		// Create perpendicular vectors for spray spread
		PerpendicularVector( perpA, baseDir );
		CrossProduct( baseDir, perpA, perpB );
	} else {
		// No direction - use upward as default
		VectorSet( baseDir, 0, 0, 1 );
		VectorSet( perpA, 1, 0, 0 );
		VectorSet( perpB, 0, 1, 0 );
	}

	// Spawn the blood droplet particles
	for ( i = 0; i < particleCount; i++ ) {
		le = CG_AllocLocalEntity();
		re = &le->refEntity;

		le->leFlags = LEF_PUFF_DONT_SCALE;
		le->leType = LE_BLOOD_PARTICLE;
		le->startTime = cg.time;
		le->endTime = cg.time + 800 + random() * 400;
		le->lifeRate = 1.0f / ( le->endTime - le->startTime );

		// Directional spray along bullet path
		// 80% exit wound (away from shooter), 20% entry (random horizontal splash)
		qboolean isExitWound = ( random() < 0.8f );
		if ( isExitWound ) {
			speed = BLOOD_PARTICLE_SPEED_EXIT * (0.4f + random() * 0.6f);
			forwardBias = 0.8f + random() * 0.4f;
			spread = (random() - 0.5f) * 0.4f;      // Tight perpendicular spread

			velocity[0] = baseDir[0] * speed * forwardBias
						+ perpA[0] * speed * spread
						+ perpB[0] * speed * (random() - 0.5f) * 0.6f;
			velocity[1] = baseDir[1] * speed * forwardBias
						+ perpA[1] * speed * spread
						+ perpB[1] * speed * (random() - 0.5f) * 0.6f;
			velocity[2] = baseDir[2] * speed * forwardBias
						+ perpA[2] * speed * spread
						+ perpB[2] * speed * (random() - 0.5f) * 0.6f
						+ speed * 0.2f;  // Slight upward bias
		} else {
			// Entry wound - spray in the plane perpendicular to projectile direction
			float angle = random() * M_PI * 2;
			float perpSpeed;
			speed = BLOOD_PARTICLE_SPEED_ENTRY * (0.4f + random() * 0.6f);
			perpSpeed = speed * (0.8f + random() * 0.4f);

			velocity[0] = perpA[0] * cos( angle ) * perpSpeed + perpB[0] * sin( angle ) * perpSpeed;
			velocity[1] = perpA[1] * cos( angle ) * perpSpeed + perpB[1] * sin( angle ) * perpSpeed;
			velocity[2] = perpA[2] * cos( angle ) * perpSpeed + perpB[2] * sin( angle ) * perpSpeed;
		}

		le->pos.trType = TR_GRAVITY;
		le->pos.trTime = cg.time;
		VectorCopy( origin, le->pos.trBase );
		VectorCopy( velocity, le->pos.trDelta );

		// Use bloodTrail shader for particles
		re->reType = RT_SPRITE;
		re->rotation = rand() % 360;
		re->radius = 3 + random() * 5;  // Varied sizes 3-8
		re->customShader = cgs.media.bloodTrailShader;
		re->shaderTime = cg.time / 1000.0f;

		VectorCopy( le->pos.trBase, re->origin );

		// Set color (shader handles actual blood color)
		le->color[0] = 1.0f;
		le->color[1] = 1.0f;
		le->color[2] = 1.0f;
		le->color[3] = 1.0f;

		re->shaderRGBA[0] = 0xff;
		re->shaderRGBA[1] = 0xff;
		re->shaderRGBA[2] = 0xff;
		re->shaderRGBA[3] = 0xff;

		le->radius = re->radius;

		// don't show player's own blood in view
		if ( isPlayer ) {
			re->renderfx |= RF_THIRD_PERSON;
		}
	}

	// Add a blood mist at entry or exit wound
	{
		float puffRadius = 2 + random() * 3;  // 2-5 units
		int puffDuration = 300 + random() * 200;  // 300-500ms

		le = CG_SmokePuff( origin, vec3_origin,
			puffRadius,
			1, 1, 1, 1,
			puffDuration,
			cg.time, 0, 0,
			cgs.media.bloodTrailShader );
		le->leType = LE_FALL_SCALE_FADE;
		le->pos.trDelta[2] = 4;  // Slow fall

		if ( isPlayer ) {
			le->refEntity.renderfx |= RF_THIRD_PERSON;
		}
	}
}



/*
==================
CG_LaunchGib
==================
*/
void CG_LaunchGib( vec3_t origin, vec3_t velocity, qhandle_t hModel ) {
	localEntity_t	*le;
	refEntity_t		*re;

	le = CG_AllocLocalEntity();
	re = &le->refEntity;

	le->leType = LE_FRAGMENT;
	le->startTime = cg.time;
	le->endTime = le->startTime + 5000 + random() * 3000;

	VectorCopy( origin, re->origin );
	AxisCopy( axisDefault, re->axis );
	re->hModel = hModel;

	le->pos.trType = TR_GRAVITY;
	VectorCopy( origin, le->pos.trBase );
	VectorCopy( velocity, le->pos.trDelta );
	le->pos.trTime = cg.time;

	le->bounceFactor = 0.6f;

	le->leBounceSoundType = LEBS_BLOOD;
	le->leMarkType = LEMT_BLOOD;
}

/*
===================
CG_GibPlayer

Generated a bunch of gibs launching out from the bodies location
===================
*/
#define	GIB_VELOCITY	250
#define	GIB_JUMP		250
void CG_GibPlayer( vec3_t playerOrigin ) {
	vec3_t	origin, velocity;

	if ( !cg_blood.integer ) {
		return;
	}

	VectorCopy( playerOrigin, origin );
	velocity[0] = crandom()*GIB_VELOCITY;
	velocity[1] = crandom()*GIB_VELOCITY;
	velocity[2] = GIB_JUMP + crandom()*GIB_VELOCITY;
	if ( rand() & 1 ) {
		CG_LaunchGib( origin, velocity, cgs.media.gibSkull );
	} else {
		CG_LaunchGib( origin, velocity, cgs.media.gibBrain );
	}

	// allow gibs to be turned off for speed
	if ( !cg_gibs.integer ) {
		return;
	}

	//Allows lots of extra gibs - woohoo gore!
	int megagibs = cg_megagibs.integer ? 1 : 0;

	int i;
	for (i = 0; i < (1 + (megagibs * 2)); ++i) {
		VectorCopy(playerOrigin, origin);
		velocity[0] = crandom() * GIB_VELOCITY;
		velocity[1] = crandom() * GIB_VELOCITY;
		velocity[2] = GIB_JUMP + crandom() * GIB_VELOCITY;
		CG_LaunchGib(origin, velocity, cgs.media.gibAbdomen);
	}

	for (i = 0; i < (1 + megagibs); ++i) {
		VectorCopy(playerOrigin, origin);
		velocity[0] = crandom() * GIB_VELOCITY;
		velocity[1] = crandom() * GIB_VELOCITY;
		velocity[2] = GIB_JUMP + crandom() * GIB_VELOCITY;
		CG_LaunchGib(origin, velocity, cgs.media.gibArm);
	}

	VectorCopy(playerOrigin, origin);
	velocity[0] = crandom() * GIB_VELOCITY;
	velocity[1] = crandom() * GIB_VELOCITY;
	velocity[2] = GIB_JUMP + crandom() * GIB_VELOCITY;
	CG_LaunchGib(origin, velocity, cgs.media.gibChest);

	for (i = 0; i < (1 + megagibs); ++i) {
		VectorCopy(playerOrigin, origin);
		velocity[0] = crandom() * GIB_VELOCITY;
		velocity[1] = crandom() * GIB_VELOCITY;
		velocity[2] = GIB_JUMP + crandom() * GIB_VELOCITY;
		CG_LaunchGib(origin, velocity, cgs.media.gibFist);
	}

	for (i = 0; i < (1 + megagibs); ++i) {
		VectorCopy(playerOrigin, origin);
		velocity[0] = crandom() * GIB_VELOCITY;
		velocity[1] = crandom() * GIB_VELOCITY;
		velocity[2] = GIB_JUMP + crandom() * GIB_VELOCITY;
		CG_LaunchGib(origin, velocity, cgs.media.gibFoot);
	}

	for (i = 0; i < (1 + megagibs); ++i) {
		VectorCopy(playerOrigin, origin);
		velocity[0] = crandom() * GIB_VELOCITY;
		velocity[1] = crandom() * GIB_VELOCITY;
		velocity[2] = GIB_JUMP + crandom() * GIB_VELOCITY;
		CG_LaunchGib(origin, velocity, cgs.media.gibForearm);
	}

	for (i = 0; i < (1 + megagibs); ++i) {
		VectorCopy(playerOrigin, origin);
		velocity[0] = crandom() * GIB_VELOCITY;
		velocity[1] = crandom() * GIB_VELOCITY;
		velocity[2] = GIB_JUMP + crandom() * GIB_VELOCITY;
		CG_LaunchGib(origin, velocity, cgs.media.gibIntestine);
	}

	for (i = 0; i < (1 + megagibs); ++i) {
		VectorCopy(playerOrigin, origin);
		velocity[0] = crandom() * GIB_VELOCITY;
		velocity[1] = crandom() * GIB_VELOCITY;
		velocity[2] = GIB_JUMP + crandom() * GIB_VELOCITY;
		CG_LaunchGib(origin, velocity, cgs.media.gibLeg);
	}
}

/*
==================
CG_LaunchExplode
==================
*/
void CG_LaunchExplode( vec3_t origin, vec3_t velocity, qhandle_t hModel ) {
	localEntity_t	*le;
	refEntity_t		*re;

	le = CG_AllocLocalEntity();
	re = &le->refEntity;

	le->leType = LE_FRAGMENT;
	le->startTime = cg.time;
	le->endTime = le->startTime + 10000 + random() * 6000;

	VectorCopy( origin, re->origin );
	AxisCopy( axisDefault, re->axis );
	re->hModel = hModel;

	le->pos.trType = TR_GRAVITY;
	VectorCopy( origin, le->pos.trBase );
	VectorCopy( velocity, le->pos.trDelta );
	le->pos.trTime = cg.time;

	le->bounceFactor = 0.1f;

	le->leBounceSoundType = LEBS_BRASS;
	le->leMarkType = LEMT_NONE;
}

#define	EXP_VELOCITY	100
#define	EXP_JUMP		150
/*
===================
CG_BigExplode

Generated a bunch of gibs launching out from the bodies location
===================
*/
void CG_BigExplode( vec3_t playerOrigin ) {
	vec3_t	origin, velocity;

	if ( !cg_blood.integer ) {
		return;
	}

	VectorCopy( playerOrigin, origin );
	velocity[0] = crandom()*EXP_VELOCITY;
	velocity[1] = crandom()*EXP_VELOCITY;
	velocity[2] = EXP_JUMP + crandom()*EXP_VELOCITY;
	CG_LaunchExplode( origin, velocity, cgs.media.smoke2 );

	VectorCopy( playerOrigin, origin );
	velocity[0] = crandom()*EXP_VELOCITY;
	velocity[1] = crandom()*EXP_VELOCITY;
	velocity[2] = EXP_JUMP + crandom()*EXP_VELOCITY;
	CG_LaunchExplode( origin, velocity, cgs.media.smoke2 );

	VectorCopy( playerOrigin, origin );
	velocity[0] = crandom()*EXP_VELOCITY*1.5;
	velocity[1] = crandom()*EXP_VELOCITY*1.5;
	velocity[2] = EXP_JUMP + crandom()*EXP_VELOCITY;
	CG_LaunchExplode( origin, velocity, cgs.media.smoke2 );

	VectorCopy( playerOrigin, origin );
	velocity[0] = crandom()*EXP_VELOCITY*2.0;
	velocity[1] = crandom()*EXP_VELOCITY*2.0;
	velocity[2] = EXP_JUMP + crandom()*EXP_VELOCITY;
	CG_LaunchExplode( origin, velocity, cgs.media.smoke2 );

	VectorCopy( playerOrigin, origin );
	velocity[0] = crandom()*EXP_VELOCITY*2.5;
	velocity[1] = crandom()*EXP_VELOCITY*2.5;
	velocity[2] = EXP_JUMP + crandom()*EXP_VELOCITY;
	CG_LaunchExplode( origin, velocity, cgs.media.smoke2 );
}

