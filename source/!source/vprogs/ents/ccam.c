/*
====================================================
= New Ccam - www.inside3d.com/qctut/lesson-39.shtml=
====================================================
*/

void () CCam;

void () CCamChasePlayer = 
{
	makevectors (pev->v_angle);
	traceline ((pev->origin + pev->view_ofs),((((pev->origin + pev->view_ofs) + (v_forward * pev->camview_z)) + (v_up * pev->camview_x)) + (v_right * pev->camview_y)),FALSE,pev);
	setorigin (pev->trigger_field,trace_endpos);
	MsgBegin( 5 );
	WriteEntity (pev->trigger_field);
	MsgEnd( MSG_ONE, '0 0 0', pev );
	pev->weaponmodel = "";
};

void () CCam = 
{
     local entity camera;

     if (pev->aflag == FALSE) 
     {
	pev->aflag = TRUE;
	camera = create("camera", "progs/eyes.mdl", trace_endpos );
	pev->trigger_field = camera;
	camera.classname = "camera";
	camera.movetype = MOVETYPE_FLY;
	camera.solid = SOLID_NOT;
	setmodel (camera,"progs/eyes.mdl");
	setsize (camera,'0 0 0','0 0 0');
	makevectors (pev->v_angle);
	traceline ((pev->origin + pev->view_ofs),(((pev->origin + pev->view_ofs) + (v_forward * -64.000))), FALSE, pev);
        	pev->camview = '0 0 -64'; // added
	setorigin (camera,trace_endpos);
        camera.angles = pev->angles;
        pev->weaponmodel = "";
        
		MsgBegin( 5 );
		WriteEntity (camera);
		WriteByte (10.000);
		WriteAngle (camera.angles_x);
		WriteAngle (camera.angles_y);
		WriteAngle (camera.angles_z);
		MsgEnd( MSG_ONE, '0 0 0', pev ); 
		sprint (pev,"Chase Cam On\n");
	}
	else
	{
 		pev->aflag = FALSE;
		MsgBegin( 5 );
		WriteEntity (pev);
		WriteByte (10);
		WriteAngle (camera.angles_x);
		WriteAngle (camera.angles_y);
		WriteAngle (camera.angles_z);
		MsgEnd( MSG_ONE, '0 0 0', pev ); 
		remove (pev->trigger_field);
		sprint (pev,"Chase Cam Off\n");
		//W_SetCurrentAmmo ();
	}
};