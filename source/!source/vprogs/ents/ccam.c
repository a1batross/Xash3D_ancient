/*
====================================================
= New Ccam - www.inside3d.com/qctut/lesson-39.shtml=
====================================================
*/

void () CCam;

void () CCamChasePlayer = 
{
	makevectors (self.v_angle);
	traceline ((self.origin + self.view_ofs),((((self.origin + self.view_ofs) + (v_forward * self.camview_z)) + (v_up * self.camview_x)) + (v_right * self.camview_y)),FALSE,self);
	setorigin (self.trigger_field,trace_endpos);
	MsgBegin( 5 );
	WriteEntity (self.trigger_field);
	MsgEnd( MSG_ONE, '0 0 0', self );
	self.weaponmodel = "";
};

void () CCam = 
{
     local entity camera;
     local entity spot;

     if (self.aflag == FALSE) 
     {
        self.aflag = TRUE;
        camera = spawn ();
        spot = spawn ();
        self.trigger_field = camera;
        camera.classname = "camera";
        camera.movetype = MOVETYPE_FLY;
        camera.solid = SOLID_NOT;
        setmodel (camera,"progs/eyes.mdl");
        setsize (camera,'0 0 0','0 0 0');
        makevectors (self.v_angle);
        traceline ((self.origin + self.view_ofs),(((self.origin + self.view_ofs)
           + (v_forward * -64.000))),FALSE,self);
        self.camview = '0 0 -64'; // added
        setorigin (camera,trace_endpos);
        camera.angles = self.angles;
        self.weaponmodel = "";
        msg_entity = self;
        
		MsgBegin( 5 );
		WriteEntity (camera);
		WriteByte (10.000);
		WriteAngle (camera.angles_x);
		WriteAngle (camera.angles_y);
		WriteAngle (camera.angles_z);
		MsgEnd( MSG_ONE, '0 0 0', self ); 
		sprint (self,"Chase Cam On\n");
	}
	else
	{
 		self.aflag = FALSE;
		msg_entity = self;
		MsgBegin( 5 );
		WriteEntity (self);
		WriteByte (10);
		WriteAngle (camera.angles_x);
		WriteAngle (camera.angles_y);
		WriteAngle (camera.angles_z);
		MsgEnd( MSG_ONE, '0 0 0', self ); 
		remove (self.trigger_field);
		sprint (self,"Chase Cam Off\n");
		//W_SetCurrentAmmo ();
	}
};