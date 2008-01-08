	void MatrixAngles(matrix4x4 matrix, vec3_t origin, vec3_t angles )
	{ 
		vec3_t		forward, right, up;
		float		xyDist;

		forward[0] = matrix[0][0];
		forward[1] = matrix[0][2];
		forward[2] = matrix[0][1];
		right[0] = matrix[1][0];
		right[1] = matrix[1][2];
		right[2] = matrix[1][1];
		up[2] = matrix[2][1];
	
		xyDist = sqrt( forward[0] * forward[0] + forward[1] * forward[1] );
	
		if ( xyDist > EQUAL_EPSILON )	// enough here to get angles?
		{
			angles[1] = RAD2DEG( atan2( forward[1], forward[0] ));
			angles[0] = RAD2DEG( atan2( -forward[2], xyDist ));
			angles[2] = RAD2DEG( atan2( -right[2], up[2] ));
		}
		else
		{
			angles[1] = RAD2DEG( atan2( right[0], -right[1] ) );
			angles[0] = RAD2DEG( atan2( -forward[2], xyDist ) );
			angles[2] = 0;
		}
		VectorCopy(matrix[3], origin );// extract origin
		ConvertPositionToGame( origin );
	}

	void AnglesMatrix(vec3_t origin, vec3_t angles, matrix4x4 matrix )
	{
		float		angle;
		float		sr, sp, sy, cr, cp, cy;

		angle = DEG2RAD(angles[YAW]);
		sy = sin(angle);
		cy = cos(angle);
		angle = DEG2RAD(angles[PITCH]);
		sp = sin(angle);
		cp = cos(angle);

		MatrixLoadIdentity( matrix );

		// forward
		matrix[0][0] = cp*cy;
		matrix[0][2] = cp*sy;
		matrix[0][1] = -sp;

		if (angles[ROLL] == 180)
		{
			angle = DEG2RAD(angles[ROLL]);
			sr = sin(angle);
			cr = cos(angle);

			// right
			matrix[1][0] = -1*(sr*sp*cy+cr*-sy);
			matrix[1][2] = -1*(sr*sp*sy+cr*cy);
			matrix[1][1] = -1*(sr*cp);

			// up
			matrix[2][0] = (cr*sp*cy+-sr*-sy);
			matrix[2][2] = (cr*sp*sy+-sr*cy);
			matrix[2][1] = cr*cp;
		}
		else
		{
			// right
			matrix[1][0] = sy;
			matrix[1][2] = -cy;
			matrix[1][1] = 0;

			// up
			matrix[2][0] = (sp*cy);
			matrix[2][2] = (sp*sy);
			matrix[2][1] = cp;
		}
		VectorCopy(origin, matrix[3] ); // pack origin
		ConvertPositionToPhysic( matrix[3] );
	}
	void AnglesMatrix2(vec3_t origin, vec3_t angles, matrix4x4 matrix )
	{
		float		angle;
		float		sr, sp, sy, cr, cp, cy;

		angle = DEG2RAD(angles[YAW]);
		sy = sin(angle);
		cy = cos(angle);
		angle = DEG2RAD(angles[PITCH]);
		sp = sin(angle);
		cp = cos(angle);

		MatrixLoadIdentity( matrix );

		// forward
		matrix[0][0] = cp*cy;
		matrix[0][1] = cp*sy;
		matrix[0][2] = -sp;

		if (angles[ROLL])
		{
			angle = DEG2RAD(angles[ROLL]);
			sr = sin(angle);
			cr = cos(angle);

			// right
			matrix[1][0] = (sr*sp*cy+cr*-sy);
			matrix[1][1] = (sr*sp*sy+cr*cy);
			matrix[1][2] = (sr*cp);

			// up
			matrix[2][0] = (cr*sp*cy+-sr*-sy);
			matrix[2][1] = (cr*sp*sy+-sr*cy);
			matrix[2][2] = cr*cp;
		}
		else
		{
			// right
			matrix[1][0] = -sy;
			matrix[1][1] = cy;
			matrix[1][2] = 0;

			// up
			matrix[2][0] = (sp*cy);
			matrix[2][1] = (sp*sy);
			matrix[2][2] = cp;
		}
		VectorCopy(origin, matrix[3] ); // pack origin
	}