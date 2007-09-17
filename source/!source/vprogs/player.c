/*
+------+
|Player|
+------+-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-+
| Scratch                        http://www.inside3d.com/qctut/scratch.shtml |
+=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-+
| Handle player animations and other misc player functions                   |
+=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-+
*/
.float anim_time; // used for animation timing
.float anim_end; // end frame for current scene
.float anim_priority; // prioritize animations
.float anim_run; // running or not

// client_t->anim_priority
float ANIM_BASIC		= 0;	// stand / run
float ANIM_PAIN			= 1;
float ANIM_ATTACK		= 2;
float ANIM_DEATH		= 3;


// running

$frame axrun1 axrun2 axrun3 axrun4 axrun5 axrun6
$frame rockrun1 rockrun2 rockrun3 rockrun4 rockrun5 rockrun6

// standing
$frame stand1 stand2 stand3 stand4 stand5
$frame axstnd1 axstnd2 axstnd3 axstnd4 axstnd5 axstnd6
$frame axstnd7 axstnd8 axstnd9 axstnd10 axstnd11 axstnd12

// pain
$frame axpain1 axpain2 axpain3 axpain4 axpain5 axpain6
$frame pain1 pain2 pain3 pain4 pain5 pain6

// death
$frame axdeth1 axdeth2 axdeth3 axdeth4 axdeth5 axdeth6
$frame axdeth7 axdeth8 axdeth9
$frame deatha1 deatha2 deatha3 deatha4 deatha5 deatha6 deatha7 deatha8
$frame deatha9 deatha10 deatha11
$frame deathb1 deathb2 deathb3 deathb4 deathb5 deathb6 deathb7 deathb8
$frame deathb9
$frame deathc1 deathc2 deathc3 deathc4 deathc5 deathc6 deathc7 deathc8
$frame deathc9 deathc10 deathc11 deathc12 deathc13 deathc14 deathc15
$frame deathd1 deathd2 deathd3 deathd4 deathd5 deathd6 deathd7
$frame deathd8 deathd9
$frame deathe1 deathe2 deathe3 deathe4 deathe5 deathe6 deathe7
$frame deathe8 deathe9

// attacks
$frame nailatt1 nailatt2
$frame light1 light2
$frame rockatt1 rockatt2 rockatt3 rockatt4 rockatt5 rockatt6
$frame shotatt1 shotatt2 shotatt3 shotatt4 shotatt5 shotatt6
$frame axatt1 axatt2 axatt3 axatt4 axatt5 axatt6
$frame axattb1 axattb2 axattb3 axattb4 axattb5 axattb6
$frame axattc1 axattc2 axattc3 axattc4 axattc5 axattc6
$frame axattd1 axattd2 axattd3 axattd4 axattd5 axattd6


void () SetClientFrame =
{
// note: call whenever weapon frames are called!
    if (pev->anim_time > time)
        return; //don't call every frame, if it is the animations will play too fast
    pev->anim_time = time + 0.1;

    local float anim_change, run;

    if (pev->velocity_x || pev->velocity_y)
        run = TRUE;
    else
        run = FALSE;

    anim_change = FALSE;

    // check for stop/go and animation transitions
    if (run != pev->anim_run && pev->anim_priority == ANIM_BASIC)
        anim_change = TRUE;

    if (anim_change != TRUE)
    {
        if (pev->frame < pev->anim_end)
        {   // continue an animation
            pev->frame = pev->frame + 1;
            return;
        }
        if (pev->anim_priority == ANIM_DEATH)
        {
            if (pev->deadflag == DEAD_DYING)
            {
                pev->nextthink = -1;
                pev->deadflag = DEAD_DEAD;
            }
            return;    // stay there
        }
    }

    // return to either a running or standing frame
    pev->anim_priority = ANIM_BASIC;
    pev->anim_run = run;

    if (pev->velocity_x || pev->velocity_y)
    {   // running
        pev->frame = $rockrun1;
        pev->anim_end = $rockrun6;
    }
    else
    {   // standing
        pev->frame = $stand1;
        pev->anim_end = $stand5;
    }
};

/*
=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
 Pain sound, and Pain animation function
=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
*/
void() PainSound =
{
    if (pev->health < 0)
        return;
    pev->noise = "";

    if (pev->watertype == CONTENT_WATER && pev->waterlevel == 3)
    { // water pain sounds
        if (random() <= 0.5)
            pev->noise = "player/drown1.wav";
        else
            pev->noise = "player/drown2.wav";
    }
    else if (pev->watertype == CONTENT_SLIME || pev->watertype == CONTENT_LAVA)
    { // slime/lava pain sounds
        if (random() <= 0.5)
            pev->noise = "player/lburn1.wav";
        else
            pev->noise = "player/lburn2.wav";
    }

    if (pev->noise)
    {
        sound (pev, CHAN_VOICE, pev->noise, 1, ATTN_NORM);
        return;
    }
//don't make multiple pain sounds right after each other
    if (pev->pain_finished > time)
        return;
    pev->pain_finished = time + 0.5;

    local float		rs;
    rs = rint((random() * 5) + 1); // rs = 1-6

    if (rs == 1)
        pev->noise = "player/pain1.wav";
    else if (rs == 2)
        pev->noise = "player/pain2.wav";
    else if (rs == 3)
        pev->noise = "player/pain3.wav";
    else if (rs == 4)
        pev->noise = "player/pain4.wav";
    else if (rs == 5)
        pev->noise = "player/pain5.wav";
    else
        pev->noise = "player/pain6.wav";

    sound (pev, CHAN_VOICE, pev->noise, 1, ATTN_NORM);
};

void () PlayerPain =
{
    if (pev->anim_priority < ANIM_PAIN) // call only if not attacking and not already in pain
    { 
        pev->anim_priority = ANIM_PAIN;
        pev->frame = $pain1;
        pev->anim_end = $pain6;
    }
    PainSound ();
};

/*
=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
 Death sound, and Death function
=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
*/
void() DeathSound =
{
    local float		rs;
    rs = rint ((random() * 4) + 1); // rs = 1-5

    if (pev->waterlevel == 3) // water death sound
        pev->noise = "player/h2odeath.wav";
    else if (rs == 1)
        pev->noise = "player/death1.wav";
    else if (rs == 2)
        pev->noise = "player/death2.wav";
    else if (rs == 3)
        pev->noise = "player/death3.wav";
    else if (rs == 4)
        pev->noise = "player/death4.wav";
    else if (rs == 5)
        pev->noise = "player/death5.wav";

    sound (pev, CHAN_VOICE, pev->noise, 1, ATTN_NONE);
};

void () PlayerDie =
{
    pev->view_ofs = '0 0 -8';
    pev->angles_x = pev->angles_z = 0;
    pev->deadflag = DEAD_DYING;
    pev->solid = SOLID_NOT;
    pev->movetype = MOVETYPE_TOSS;
    pev->aiflags = pev->aiflags - (pev->aiflags & AI_ONGROUND);
    if (pev->velocity_z < 10)
        pev->velocity_z = pev->velocity_z + random()*300;

    local float rand;
    rand = rint ((random() * 4) + 1); // rand = 1-5

    pev->anim_priority = ANIM_DEATH;
    if (rand == 1)
    {
        pev->frame = $deatha1;
        pev->anim_end = $deatha11;
    }
    else if (rand == 2)
    {
        pev->frame = $deathb1;
        pev->anim_end = $deathb9;
    }
    else if (rand == 3)
    {
        pev->frame = $deathc1;
        pev->anim_end = $deathc15;
    }
    else if (rand == 4)
    {
        pev->frame = $deathd1;
        pev->anim_end = $deathd9;
    }
    else 
    {
        pev->frame = $deathe1;
        pev->anim_end = $deathe9;
    }
    DeathSound();
};
