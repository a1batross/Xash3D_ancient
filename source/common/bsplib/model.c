//=======================================================================
//			Copyright XashXT Group 2007 ©
//		 model.c - convert studio models to bsp triangles
//=======================================================================

#include "bsplib.h"
#include "const.h"
#include "matrixlib.h"

/*
=============
InsertModel

adds a studiomodel into the bsp
=============
*/
void InsertModel( const char *name, int body, int seq, float frame, matrix4x4 transform, drawsurf_t *src, int spawnFlags )
{
#if 0	
	// FIXME: implement studio into physic.dll

	int			i, j, k, s, numSurfaces;
	matrix4x4			identity, nTransform;
	cmodel_t			*model;
	const char		*shader;
	csurface_t		*surface;
	bsp_shader_t		*si;
	drawsurf_t		*ds;
	dvertex_t			*dv;
	string			shaderName;
	vec_t			*point, *normal, *st;
	byte			*color;
	int			*indexes;

	// check for attached surface
	if( src == NULL ) return;	
	model = pe->RegisterModel( name );
	if( model == NULL ) return;
	
	if( transform == NULL )
	{
		Matrix4x4_LoadIdentity( identity );
		transform = identity;
	}

	// create transform matrix for normals
	Mem_Copy( nTransform, transform, sizeof( matrix4x4 ));
	if( Matrix4x4_Invert_Full( nTransform, nTransform ))
		MsgDev( D_WARN, "Can't invert model transform matrix, using transpose instead\n" );
	Matrix4x4_Transpose( nTransform, nTransform );
	
	if( src->lightmapScale <= 0.0f ) src->lightmapScale = 1.0f;
	// each surface on the model will become a new map drawsurface
	numSurfaces = pe->GetStudioNumSurfaces( model );

	for( s = 0; s < numSurfaces; s++ )
	{
		surface = pe->GetStudioSurface( model, body, s );
		if( surface == NULL ) continue;
		
		pe->FixStudioNormals( surface );
		
		ds = AllocDrawSurf( SURFACE_TRIANGLES );
		ds->entitynum = src->entitynum;
		ds->castShadows = src->castShadows;
		ds->recvShadows = src->recvShadows;
		
		shader = pe->GetStudioTexture( surface );
		si = FindShader( shader );
		
		ds->shader = si;
		ds->lightmapScale = src->lightmapScale;
		
		if((si != NULL && si->forceMeta) || (spawnFlags & 4))
			ds->type = SURFACE_FORCED_META;
		
		ds->numVerts = pe->GetStudioNumVertexes( surface );
		ds->verts = BSP_Malloc( ds->numVerts * sizeof( ds->verts[0] ));
		
		ds->numIndexes = pe->GetStudioNumIndexes( surface );
		ds->indexes = BSP_Malloc( ds->numIndexes * sizeof( ds->indexes[0] ));
		
		for( i = 0; i < ds->numVerts; i++ )
		{
			dv = &ds->verts[i];
			
			point = pe->StudioGetPoint( surface, i );
			VectorCopy( point, dv->point );
			Matrix4x4_TransformPoint( transform, dv->point );
			
			normal = pe->StudioGetNormal( surface, i );
			VectorCopy( normal, dv->normal );

			// FIXME: may be need Matrix4x4_TransposeRotate ???
			Matrix4x4_Rotate( nTransform, dv->normal, dv->normal );
			VectorNormalize( dv->normal );

			// added support for explicit shader texcoord generation
			if( si->tcGen )
			{
				dv->st[0] = DotProduct( si->vecs[0], dv->point );
				dv->st[1] = DotProduct( si->vecs[1], dv->point );
			}
			else
			{
				st = pe->StudioGetTexCoords( surface, body, i );
				dv->st[0] = st[0];
				dv->st[1] = st[1];
			}
			
			// set lightmap/color bits
			color = pe->StudioGetColor( surface, body, i );
			for( j = 0; j < LM_STYLES; j++ )
			{
				dv->lm[j][0] = 0.0f;
				dv->lm[j][1] = 0.0f;
				dv->color[j][0] = color[0];
				dv->color[j][1] = color[1];
				dv->color[j][2] = color[2];
				dv->color[j][3] = color[3];
			}
		}
		
		indexes = pe->StduioGetIndexes( surface, body, 0 );
		for( i = 0; i < ds->numIndexes; i++ )
			ds->indexes[i] = indexes[i];
		
		// giant hack land: generate clipping brushes for model triangles
		if( si->clipModel || ( spawnFlags & 2 ))
		{
			vec3_t	points[3], backs[3];
			vec4_t	plane, reverse, pa, pb, pc;
			vec3_t	nadir;
			
			if( !si->clipModel && ((si->contents & CONTENTS_TRANSLUCENT) || !(si->contents & CONTENTS_SOLID)))
				continue;
			
			if((nummapplanes + 64) >= (MAX_MAP_PLANES >> 1))
				continue;
			
			for( i = 0; i < ds->numIndexes; i += 3 )
			{
				if((nummapplanes + 64) >= (MAX_MAP_PLANES >> 1) )
				{
					MsgDev( D_WARN, "MAX_MAP_PLANES (%d) hit generating clip brushes for model %s.\n", MAX_MAP_PLANES, name );
					break;
				}
				
				// make points and back points
				for( j = 0; j < 3; j++ )
				{
					dv = &ds->verts[ds->indexes[i+j]];
					
					VectorCopy( dv->point, points[j] );
					VectorCopy( dv->point, backs[j] );
					
					// find nearest axial to normal and push back points opposite
					// NOTE: this doesn't work as well as simply using the plane of the triangle, below
					for( k = 0; k < 3; k++ )
					{
						if(fabs(dv->normal[k]) > fabs( dv->normal[(k+1)%3] ) && fabs(dv->normal[k]) > fabs(dv->normal[(k+2)%3]))
						{
							backs[j][k] += dv->normal[k] < 0.0f ? 64.0f : -64.0f;
							break;
						}
					}
				}
				
				// make plane for triangle
				if( PlaneFromPoints( plane, points[0], points[1], points[2] ))
				{
					// regenerate back points
					for( j = 0; j < 3; j++ )
					{
						dv = &ds->verts[ds->indexes[i+j]];
						
						// copy points
						VectorCopy( dv->point, backs[j] );
						
						// find nearest axial to plane normal and push back points opposite
						for( k = 0; k < 3; k++ )
						{
							if( fabs( plane[k] ) > fabs( plane[(k+1)%3] ) && fabs( plane[k] ) > fabs( plane[(k+2)%3] ))
							{
								backs[j][k] += plane[k] < 0.0f ? 64.0f : -64.0f;
								break;
							}
						}
					}
					
					// make back plane
					VectorScale( plane, -1.0f, reverse );
					reverse[3] = -(plane[3] - 1);
					
					// make back pyramid point
					VectorCopy( points[0], nadir );
					VectorAdd( nadir, points[1], nadir );
					VectorAdd( nadir, points[2], nadir );
					VectorScale( nadir, 0.3333333333333f, nadir );
					VectorMA( nadir, -2.0f, plane, nadir );
					
					/* make 3 more planes */
					if( PlaneFromPoints( pa, points[2], points[1], backs[1] ) &&
					PlaneFromPoints( pb, points[1], points[0], backs[0] ) &&
					PlaneFromPoints( pc, points[0], points[2], backs[2] ))
					{
						// build a brush
						buildBrush = AllocBrush( 48 );
						
						buildBrush->entitynum = entity_num - 1;
						buildBrush->original = buildBrush;
						buildBrush->shader = si;
						buildBrush->contents = si->contents;
						buildBrush->detail = true;
						
						buildBrush->numsides = 5;
						for( j = 0; j < buildBrush->numsides; j++ )
							buildBrush->sides[j].shader = si;
						buildBrush->sides[0].planenum = FindFloatPlane( plane, plane[3], 3, points );
						buildBrush->sides[1].planenum = FindFloatPlane( pa, pa[3], 1, &points[2] );
						buildBrush->sides[2].planenum = FindFloatPlane( pb, pb[3], 1, &points[1] );
						buildBrush->sides[3].planenum = FindFloatPlane( pc, pc[3], 1, &points[0] );
						buildBrush->sides[4].planenum = FindFloatPlane( reverse, reverse[3], 3, points );
						
						// add to entity
						if( CreateBrushWindings( buildBrush ))
						{
							AddBrushBevels();
							buildBrush->next = entities[entity_num - 1].brushes;
							entities[entity_num - 1].brushes = buildBrush;
							entities[entity_num - 1].numBrushes++;
						}
						else Mem_Free( buildBrush );
					}
				}
			}
		}
	}
#endif
}



/*
=============
AddTriangleModels

adds env_model surfaces to the bsp
=============
*/
void AddTriangleModels( bsp_entity_t *e )
{
	int		num, body, seq, castShadows, recvShadows, spawnFlags;
	bsp_entity_t	*e2;
	drawsurf_t	src;
	const char	*targetName;
	const char	*target, *model, *value;
	float		temp, frame;
	vec3_t		origin, scale, angles;
	matrix4x4		transform;
	
	MsgDev( D_NOTE, "--- AddTriangleModels ---\n" );
	
	// get current brush entity targetname
	if( e == entities ) targetName = "";
	else
	{
		targetName = ValueForKey( e, "targetname" );
	
		// misc_model entities target non-worldspawn brush model entities
		if( targetName[0] == '\0' ) return;
	}

	for( num = 1; num < num_entities; num++ )
	{
		e2 = &entities[num];
		
		// convert env_models into raw geometry
		if( com.stricmp( "env_model", ValueForKey( e2, "classname" )))
			continue;

		// added support for md3 models on non-worldspawn models */
		target = ValueForKey( e2, "target" );
		if( com.strcmp( target, targetName )) continue;
		
		// get model name
		model = ValueForKey( e2, "model" );
		if( model[0] == '\0' )
		{
			MsgDev( D_WARN, "env_model at %i %i %i without a model key\n", (int)origin[0], (int)origin[1], (int)origin[2] );
			continue;
		}
		
		frame = FloatForKey( e2, "frame" );
		body = FloatForKey( e2, "body" );
		seq = FloatForKey( e2, "sequence" );
		
		// worldspawn (and func_groups) default to cast/recv shadows in worldspawn group
		if( e == entities )
		{
			castShadows = WORLDSPAWN_CAST_SHADOWS;
			recvShadows = WORLDSPAWN_RECV_SHADOWS;
		}
		else
		{
			castShadows = ENTITY_CAST_SHADOWS;
			recvShadows = ENTITY_RECV_SHADOWS;
		}
		
		// get explicit shadow flags
		GetEntityShadowFlags( e2, e, &castShadows, &recvShadows );
		
		// get spawnflags
		spawnFlags = IntForKey( e2, "spawnflags" );
		
		// get origin
		GetVectorForKey( e2, "origin", origin );
		VectorSubtract( origin, e->origin, origin );	/* offset by parent */
		
		/* get scale */
		scale[0] = scale[1] = scale[2] = 1.0f;
		temp = FloatForKey( e2, "scale" );
		if( temp != 0.0f ) scale[0] = scale[1] = scale[2] = temp;
		
		// get "angle" (yaw) or "angles" (pitch yaw roll)
		angles[0] = angles[1] = angles[2] = 0.0f;
		angles[2] = FloatForKey( e2, "angle" );
		value = ValueForKey( e2, "angles" );
		if( value[0] != '\0' ) sscanf( value, "%f %f %f", &angles[1], &angles[2], &angles[0] );
		
		Matrix4x4_LoadIdentity( transform );
		Matrix4x4_Pivot( transform, origin, angles, scale, vec3_origin );
		memset( &src, 0, sizeof( src ));
		src.castShadows = castShadows;
		src.recvShadows = recvShadows;
		src.lightmapScale = 1.0f;	// FIXME
		src.entitynum = entity_num;	// this is correct ?
		
		// insert the model
		InsertModel( model, body, seq, frame, transform, &src, spawnFlags );
	}
}