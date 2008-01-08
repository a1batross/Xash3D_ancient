//=======================================================================
//			Copyright XashXT Group 2007 ©
//		cg_user.c - stuff that will moved into client.dat
//=======================================================================

#include "client.h"

extern cvar_t	*scr_centertime;
extern cvar_t	*scr_showpause;

#define DISPLAY_ITEMS	17
#define STAT_MINUS		10 // num frame for '-' stats digit

char *cg_nums[11] =
{
"hud/num0", "hud/num1", "hud/num2", 
"hud/num3", "hud/num4", "hud/num5",
"hud/num6", "hud/num7", "hud/num8", 
"hud/num9", "hud/num-",
};

/*
================
CG_Init

initialize cg
================
*/
void CG_Init( void )
{
	cls.cg_numstats = 0; // reset hudprogram statsnames
	cls.cg_numcvars = 0; // reset hudprogram statsnames
	cls.cg_init = Com_LoadScript( "scripts/hud.txt", NULL, 0 );
	CG_ExecuteProgram( "Hud_Setup" ); // get stats and cvars names
}

/*
================
V_RenderSplash

menu background
================
*/
void V_RenderSplash( void )
{
	// execute hudprogram
	CG_ExecuteProgram( "Hud_MenuBackground" );
}

/*
================
V_RenderLogo

loading splash
================
*/
void V_RenderLogo( void )
{
	// execute hudprogram
	CG_ExecuteProgram( "Hud_DrawPlaque" );
}

/*
================
V_RenderHUD

user hud rendering
================
*/
void V_RenderHUD( void )
{
	CG_MakeLevelShot();
	CG_DrawCenterString();
	CG_ExecuteProgram(cl.configstrings[CS_STATUSBAR]);
	CG_DrawInventory();
	CG_DrawLayout();
}


/*
==============
CG_MakeLevelShot

used as splash logo
==============
*/
void CG_MakeLevelShot( void )
{
	if(cl.make_levelshot)
	{
		Con_ClearNotify();
		cl.make_levelshot = false;

		// make levelshot at nextframe()
		Cbuf_ExecuteText(EXEC_APPEND, "levelshot\n");
	}
}

/*
==============================================================================

CG Draw Tools
==============================================================================
*/
/*
================
SCR_GetBigStringWidth

skips color escape codes
================
*/
int CG_GetBigStringWidth( const char *str )
{
	return com.cstrlen( str ) * 16;
}

/*
================
CG_FadeColor
================
*/
float *CG_FadeColor( float starttime, float endtime )
{
	static vec4_t	color;
	float		time, fade_time;

	if( starttime == 0 ) return NULL;
	time = cls.realtime - starttime;
	if( time >= endtime ) return NULL;

	// fade time is 1/4 of endtime
	fade_time = endtime / 4;
	fade_time = bound( 0.3f, fade_time, 10.0f );

	// fade out
	if((endtime - time) < fade_time)
		color[3] = (endtime - time) * 1.0f / fade_time;
	else color[3] = 1.0;
	color[0] = color[1] = color[2] = 1.0f;

	return color;
}

void CG_SetColor( float r, float g, float b, float a )
{
	Vector4Set( cls.cg_color, r, g, b, a );
	re->SetColor( cls.cg_color );
}

void CG_ResetColor( void )
{
	Vector4Set( cls.cg_color, 1.0f, 1.0f, 1.0f, 1.0f );
	re->SetColor( cls.cg_color );
}

/*
================
CG_DrawLayout
================
*/
void CG_DrawLayout( void )
{
	if(cl.frame.playerstate.stats[STAT_LAYOUTS] & 1)
		CG_ExecuteProgram( cl.layout );
}

/*
================
CG_DrawInventory
================
*/
void CG_DrawInventory( void )
{
	float		*select_color = NULL;
	int		num = 0, selected_num = 0;
	int		item, index[MAX_ITEMS];
	bool		force_color = false;
	char		string[1024];
	int		i, j, x, y;
	char		binding[1024];
	char		*bind;
	int		selected;
	int		top;

	if(!(cl.frame.playerstate.stats[STAT_LAYOUTS] & 2)) 
		return;

	if(strcmp(cl.layout, "" ))
	{
		CG_ExecuteProgram( cl.layout );
		return;
	}

	selected = cl.frame.playerstate.stats[STAT_SELECTED_ITEM];

	num = 0;
	selected_num = 0;
	for ( i = 0; i < MAX_ITEMS; i++)
	{
		if (i == selected) 
			selected_num = num;
		if (cl.inventory[i])
		{
			index[num] = i;
			num++;
		}
	}

	// determine scroll point
	top = selected_num - DISPLAY_ITEMS/2;
	if (num - top < DISPLAY_ITEMS) top = num - DISPLAY_ITEMS;
	if (top < 0) top = 0;

	x = (SCREEN_WIDTH - 256)>>1;
	y = (SCREEN_HEIGHT - 240)>>1;
	CG_DrawCenterPic( 256, 192, "hud/inventory" );

	y += 42;
	x += 24;
	SCR_DrawSmallStringExt(x, y, "hotkey ### item", NULL, false );
	SCR_DrawSmallStringExt(x, y + 8, "------ --- ----", NULL, false );
	y += 16;

	for (i = top; i < num && i < top + DISPLAY_ITEMS; i++)
	{
		item = index[i];
		// search for a binding
		sprintf(binding, "use %s", cl.configstrings[CS_ITEMS + item]);
		bind = "";
		for (j = 0; j < 256; j++)
		{
			if(Key_IsBind(j) && !strcasecmp(Key_IsBind(j), binding))
			{
				bind = Key_KeynumToString(j);
				break;
			}
		}
		sprintf(string, "%6s %3i %s", bind, cl.inventory[item], cl.configstrings[CS_ITEMS+item] );
		if (item != selected)
		{
			select_color = GetRGBA( 1.0f, 0.5f, 0.0f, 1.0f );
			force_color = true;
		}
		else // draw a blinky cursor by the selected item
		{
			select_color = NULL;
			force_color = false;
			if((int)(cls.realtime * 5.0f) & 1) SCR_DrawSmallChar( x - 8, y, 15);
		}
		SCR_DrawSmallStringExt(x, y, string, select_color, force_color );
		y += 8;
	}


}

void CG_DrawField( int value, int x, int y, int color )
{
	char	num[16], *ptr;
	int	l, frame;

	sprintf(num, "%i", value );
	l = strlen( num );
	ptr = num;

	re->SetColor( g_color_table[ColorIndex(color)] );

	while (*ptr && l)
	{
		if (*ptr == '-') frame = STAT_MINUS;
		else frame = *ptr -'0';
		SCR_DrawPic( x, y, GIANTCHAR_WIDTH, GIANTCHAR_HEIGHT, cg_nums[frame]);
		x += GIANTCHAR_WIDTH;
		ptr++;
		l--;
	}

	re->SetColor( NULL );
}

void CG_DrawBarImage( float percent, char *picname, int x, int y, int w, int h )
{
	float progress = bound(0.0f, scr_loading->value * 0.01f, 100.0f );

	w = bound(64.0f, w, 512.0f);
	h = bound(16.0f, h, 64.0f);

	re->SetColor(GetRGBA(1.0f, 1.0f, 1.0f, 1.0f));
	SCR_DrawPic( x, y, w, h, picname );
	re->SetColor(GetRGBA(1.0f, 1.0f, 1.0f, 0.3f));
	SCR_DrawPic( x, y, w * progress, h, "common/fill_rect");
	CG_ResetColor();
}

void CG_DrawCenterBarImage( float percent, char *picname, int w, int h )
{
	CG_DrawBarImage(percent, picname, (SCREEN_WIDTH - w)>>1, (SCREEN_HEIGHT - h)>>1, w, h ); 
}

void CG_DrawBarGeneric( float percent, int x, int y, int w, int h )
{
	float progress = bound(0.0f, percent * 0.01f, 100.0f );

	w = bound(64.0f, w, 512.0f);
	h = bound(16.0f, h, 64.0f);

	SCR_FillRect(x, y, w, h, g_color_table[0] );
	SCR_DrawPic(x + 1, y + 1, w - 2, h - 2, "common/bar_back");
	SCR_DrawPic(x + 1, y + 1, (w - 2) * progress, h - 2, "common/bar_load");
}

void CG_DrawCenterBarGeneric( float percent, int w, int h )
{
	CG_DrawBarGeneric( percent, (SCREEN_WIDTH - w)>>1, (SCREEN_HEIGHT - h)>>1, w, h ); 
}

/*
===================
CG_DrawCenterString
===================
*/
void CG_DrawCenterString( void )
{
	char	*start;
	int	l, x, y, w;
	float	*color;

	if(!cl.centerPrintTime ) return;

	color = CG_FadeColor( cl.centerPrintTime, scr_centertime->value );
	if( !color ) 
	{
		cl.centerPrintTime = 0.0f;
		return;
	}

	re->SetColor( color );
	start = cl.centerPrint;
	y = cl.centerPrintY - cl.centerPrintLines * BIGCHAR_HEIGHT/2;

	while( 1 )
	{
		char linebuffer[1024];

		for ( l = 0; l < 50; l++ )
		{
			if ( !start[l] || start[l] == '\n' )
				break;
			linebuffer[l] = start[l];
		}
		linebuffer[l] = 0;

		w = cl.centerPrintCharWidth * com.cstrlen( linebuffer );
		x = ( SCREEN_WIDTH - w )>>1;

		SCR_DrawStringExt( x, y, cl.centerPrintCharWidth, BIGCHAR_HEIGHT, linebuffer, color, false );

		y += cl.centerPrintCharWidth * 1.5;
		while ( *start && ( *start != '\n' )) start++;
		if( !*start ) break;
		start++;
	}
	re->SetColor( NULL );
}

/*
==============
CG_CenterPrint

Called for important messages that should stay in the center of the screen
for a few moments
==============
*/
void CG_CenterPrint( const char *str, int y, int charWidth )
{
	char	*s;

	strncpy( cl.centerPrint, str, sizeof(cl.centerPrint));

	cl.centerPrintTime = cls.realtime;
	cl.centerPrintY = y;
	cl.centerPrintCharWidth = charWidth;

	// count the number of lines for centering
	cl.centerPrintLines = 1;
	s = cl.centerPrint;
	while( *s )
	{
		if (*s == '\n') cl.centerPrintLines++;
		s++;
	}
}

/*
==============
CG_DrawCenterPic

Called for important messages that should stay in the center of the screen
for a few moments
==============
*/
void CG_DrawCenterPic( int w, int h, char *picname )
{
	SCR_DrawPic((SCREEN_WIDTH - w)>>1, (SCREEN_HEIGHT - h)>>1, w, h, picname );
}

void CG_DrawCenterStringBig( char *text, float alpha )
{
	int xpos = (SCREEN_WIDTH - com.cstrlen(text) * BIGCHAR_WIDTH)>>1;
	int ypos = (SCREEN_HEIGHT - BIGCHAR_HEIGHT)>>1;

	SCR_DrawBigString(xpos, ypos, text, alpha ); 
	re->SetColor( NULL );
}

/*
==============================================================================

CG Virtual Machine
==============================================================================
*/
/*
================
CG_GetAliasValue

get name from registered list
================
*/
bool CG_GetAliasValue( const char *name, int *value, char *string )
{
	int	i, find = 0;

	if(!name || !name[0]) return false;

	// lookup stats
	for(i = 0; i < cls.cg_numstats; i++)
	{
		if(!strcmp(cls.cg_stats[i].name, name ))
		{
			find = cl.frame.playerstate.stats[cls.cg_stats[i].value];
			break;
		}
	}
	if(i != cls.cg_numstats) 
	{
		*value = find;
		return true;
	}

	// lookup cvars
	for(i = 0; i < cls.cg_numcvars; i++)
	{
		if(!strcmp(cls.cg_cvars[i].name, name ))
		{
			cvar_t *alias = Cvar_FindVar( cls.cg_cvars[i].cvar );
			if(alias) 
			{
				find = alias->integer;
				if(string) strncpy( string, alias->string, MAX_QPATH );
				break;
			}
			MsgDev(D_WARN, "CG_GetAliasValue: alias %s have invalid cvar name %s\n", name, cls.cg_cvars[i].cvar );
		}
	}
	if(i != cls.cg_numcvars) 
	{
		*value = find;
		return true;
	}

	// constant digit ?
	find = atoi(name);
	if(find) 
	{
		*value = find;
		return true;
	}

	*value = 0;
	return false;
}

char *CG_StatsString( const char *name, int start, int end )
{
	int	i, value;

	if(!name || !name[0]) return "common/black";

	// clear tempstring
	memset( cls.cg_tempstring, 0, MAX_QPATH );

	// search for alias
	if(!CG_GetAliasValue( name, &value, cls.cg_tempstring ))
	{
		// search for normal name
		for(i = 0; i < end; i++)
		{
			if(!strcmp(cl.configstrings[start + i], name ))
			{
				// index can be changed from server
				return cl.configstrings[start + i];
			}
		}
	}
	else if(strlen(cls.cg_tempstring))
	{
		return cls.cg_tempstring;
	}
	else if(value < end)
	{
		// static image index
		return cl.configstrings[start + value];
	}

	// direct path ?
	return (char *)name;
}

char *CG_ImageIndex( const char *name )
{
	return CG_StatsString(name, CS_IMAGES, MAX_IMAGES );
}

char *CG_ModelIndex( const char *name )
{
	return CG_StatsString(name, CS_MODELS, MAX_MODELS );
}

bool CG_GetInteger( const char *name, int *value ) 
{
	return CG_GetAliasValue( name, value, NULL );
}

/*
================
CG_ParseInventory
================
*/
void CG_ParseInventory (void)
{
	int		i;

	for (i = 0; i < MAX_ITEMS; i++)
	{
		cl.inventory[i] = MSG_ReadShort (&net_message);
	}
}

/*
================
CG_SetStatsAlias

register new alias for stats num (0-32)
================
*/
void CG_SetStatsAlias( const char *name, int value )
{
	int	i;

	if(!name) return;
	if(strlen(name) > MAX_QPATH)
	{
		MsgDev(D_WARN, "SetStatsAlias: %s too long name, limit is %d\n", name, MAX_QPATH );
	}
	if(value < 0 || value >= MAX_STATS)
	{
		MsgDev(D_WARN, "SetStatsAlias: value %d out of range\n", value );
		value = bound(0, value, MAX_STATS - 1 );
	}
	if(cls.cg_numstats + 1 >= MAX_STATS)
	{
		MsgDev(D_WARN, "SetStatsAlias: aliases limit exceeded\n" );
		return;
	}

	// check for duplicate
	for(i = 0; i < cls.cg_numstats; i++)
	{
		if(!strcmp(cls.cg_stats[i].name, name ))
		{
			if(cls.cg_stats[i].value != value)
				MsgDev(D_WARN, "SetStatsAlias: redefinition stat alias %s\n", name );
			else MsgDev(D_WARN, "SetStatsAlias: duplicated stat alias %s\n", name );
			break;
		}		
	}

	// register new stat alias
	if(i == cls.cg_numstats)
	{
		strncpy(cls.cg_stats[cls.cg_numstats].name, name, MAX_QPATH );
		cls.cg_stats[cls.cg_numstats].value = value;
		cls.cg_numstats++;
	}
}

/*
================
CG_SetCvarAlias

register new alias for console variable
================
*/
void CG_SetCvarAlias( const char *name, const char *cvar )
{
	int	i;

	if(!name || !cvar) return;
	if((strlen(name) > MAX_QPATH))
	{
		MsgDev(D_WARN, "SetCvarAlias: %s too long name, limit is %d\n", name, MAX_QPATH );
	}
	if(strlen(cvar) > MAX_QPATH)
	{
		MsgDev(D_WARN, "SetCvarAlias: %s too long cvar name, limit is %d\n", cvar, MAX_QPATH );
	}
	if(cls.cg_numstats + 1 >= 128 )
	{
		MsgDev(D_WARN, "SetCvarAlias: aliases limit exceeded\n" );
		return;
	}

	// check for duplicate
	for(i = 0; i < cls.cg_numcvars; i++)
	{
		if(!strcmp(cls.cg_cvars[i].name, name ))
		{
			if(strcmp(cls.cg_cvars[i].cvar, cvar ))
				MsgDev(D_WARN, "SetCvarAlias: redefinition cvar alias %s\n", name );
			else MsgDev(D_WARN, "SetCvarAlias: duplicated cvar alias %s\n", name );
			break;
		}		
	}

	// register new cvar alias
	if(i == cls.cg_numcvars)
	{
		strncpy(cls.cg_cvars[cls.cg_numcvars].name, name, MAX_QPATH );
		strncpy(cls.cg_cvars[cls.cg_numcvars].cvar, cvar, MAX_QPATH );
		cls.cg_numcvars++;
	}
}

/*
================
CG_ParseArgs

soul of virtual machine
================
*/
bool CG_ParseArgs( int num_argc )
{
	cls.cg_argc = 0;
	memset(cls.cg_argv, 0, MAX_PARMS * MAX_QPATH );
	strncpy( cls.cg_builtin, com_token, MAX_QPATH );

	// bound range silently 
	num_argc = bound(0, num_argc, MAX_PARMS - 1); 

	while(Com_TryToken())
	{
		if(!num_argc) continue;	// nothing to handle
		if(num_argc > 0 && cls.cg_argc > num_argc - 1 )
		{
			MsgDev(D_ERROR, "CG_ParseArgs: %s have too many parameters\n", cls.cg_builtin );
			return false; // stack overflow
		}
		else if(Com_MatchToken(";")) break;		// end of parsing
		else if(Com_MatchToken(",")) cls.cg_argc++;	// new argument
		else if(Com_MatchToken("(") || Com_MatchToken(")"))
			continue; // skip punctuation
		else strncpy(cls.cg_argv[cls.cg_argc], com_token, MAX_QPATH ); // fill stack
	}

	if(num_argc > 0 && cls.cg_argc < num_argc - 1)
		MsgDev(D_WARN, "CG_ParseArgs: %s have too few parameters\n", cls.cg_builtin );

	return true;
}

void CG_SkipBlock( void )
{
	cls.cg_depth2 =  cls.cg_depth;

	do {
		if(Com_MatchToken("{"))
		{
			// for bounds cheking
			cls.cg_depth++;
		}
		else if(Com_MatchToken("}")) 
		{
			cls.cg_depth--;
			if(cls.cg_depth == cls.cg_depth2)
				break;
		}
		while(Com_TryToken());
	} while(Com_GetToken( true ));

	if(cls.cg_depth != cls.cg_depth2)
	{
		MsgDev(D_ERROR, "CG_SkipBlock: missing } in function %s\n", cls.cg_function );
	}
}

bool CG_ParseExpression( void )
{
	cg_def_t		expression;
	int		j = 0, result = 0;

	memset(&expression, 0, sizeof(cg_def_t));

	cls.cg_depth2 = cls.cg_depth;

	while(Com_TryToken())
	{
		if(Com_MatchToken("("))
		{
		}
		else if(Com_MatchToken(")")) break;
		else if(Com_MatchToken("&&")) expression.op = OP_LOGIC_AND;
		else if(Com_MatchToken("||")) expression.op = OP_LOGIC_OR;
		else if(Com_MatchToken("==")) expression.op = OP_EQUAL;
		else if(Com_MatchToken("!=")) expression.op = OP_NOTEQUAL;
		else if(Com_MatchToken(">")) expression.op = OP_MORE;
		else if(Com_MatchToken(">=")) expression.op = OP_MORE_OR_EQUAL;
		else if(Com_MatchToken("<")) expression.op = OP_SMALLER;
		else if(Com_MatchToken("<=")) expression.op = OP_SMALLER_OR_EQUAL;
		else if(Com_MatchToken("&")) expression.op = OP_WITH;
		else
		{
			int	val;

			if(CG_GetInteger( com_token[0] == '!' ? com_token + 1 : com_token, &val ))
			{
				if(com_token[0] == '!') expression.val[j] = !val;
				else expression.val[j] = val;

				if(expression.op == OP_UNKNOWN)
					expression.op = OP_NOTEQUAL;	// may be overrided
				j++; // force to done
			}
			else MsgDev(D_ERROR, "CG_ParseExpression: unknown variable %s\n", com_token ); 
		}
	}

	// exec expression now
	switch(expression.op)
	{
	case OP_LOGIC_OR:
		if(expression.val[0] || expression.val[1]) result++;
		else result--;
		break;
	case OP_LOGIC_AND:
		if(expression.val[0] && expression.val[1]) result++;
		else result--;
		break;
	case OP_EQUAL:
		if(expression.val[0] == expression.val[1]) result++;
		else result--;
		break;
	case OP_NOTEQUAL:
		if(expression.val[0] != expression.val[1]) result++;
		else result--;
		break;
	case OP_MORE:
		if(expression.val[0] > expression.val[1]) result++;
		else result--;
		break;
	case OP_MORE_OR_EQUAL:
		if(expression.val[0] >= expression.val[1]) result++;
		else result--;
		break;
	case OP_SMALLER:
		if(expression.val[0] < expression.val[1]) result++;
		else result--;
		break;
	case OP_SMALLER_OR_EQUAL:
		if(expression.val[0] <= expression.val[1]) result++;
		else result--;
		break;
	case OP_WITH:
		if(expression.val[0] & expression.val[1]) result++;
		else result--;
		break;
	}
	return result;
}

bool CG_ExecBuiltins( void )
{
	int	value = 0;

	// builtins
	if(Com_MatchToken("{"))
	{
		cls.cg_depth++;
		return false;
	}
	else if(Com_MatchToken("}"))
	{
		cls.cg_depth--;
		return false;
	}
	else if(Com_MatchToken("if"))
	{	
		// parse expression
		if(CG_ParseExpression() == -1)
		{
			CG_SkipBlock();
		}
		return false;
	}
	else if(Com_MatchToken("SetStatAlias"))
	{
		// set alias name for stats numbers
		if(!CG_ParseArgs( 2 )) return false;
		CG_SetStatsAlias(cls.cg_argv[0], atoi(cls.cg_argv[1]));
	}
	else if(Com_MatchToken("SetCvarAlias"))
	{
		// set alias name for stats numbers
		if(!CG_ParseArgs( 2 )) return false;
		CG_SetCvarAlias(cls.cg_argv[0], cls.cg_argv[1]);
	}
	else if(Com_MatchToken("LoadPic"))
	{
		// cache hud pics
		if(!CG_ParseArgs( 1 )) return false;
		re->RegisterPic(cls.cg_argv[0]);
	}
	else if(Com_MatchToken("DrawField"))
	{
		// displayed health, armor, e.t.c.
		if(!CG_ParseArgs( 4 )) return false;
		if(!CG_GetInteger(cls.cg_argv[0], &value))
			MsgDev(D_ERROR, "%s: can't use undefined alias %s\n", cls.cg_builtin, cls.cg_argv[0]);
		else CG_DrawField( value, atoi(cls.cg_argv[1]), atoi(cls.cg_argv[2]), atoi(cls.cg_argv[3]));
	}
	else if(Com_MatchToken("SetColor"))
	{
		// set custom color
		if(!CG_ParseArgs( 4 )) return false;
		CG_SetColor(atof(cls.cg_argv[0]), atof(cls.cg_argv[1]), atof(cls.cg_argv[2]), atof(cls.cg_argv[3])); 
	}
	else if(Com_MatchToken("ResetColor"))
	{
		// reset custom color
		if(!CG_ParseArgs( 0 )) return false;
		CG_ResetColor();
	}
	else if(Com_MatchToken("DrawPic"))
	{
		// draw named pic
		if(!CG_ParseArgs( 5 )) return false;
		SCR_DrawPic( atoi(cls.cg_argv[1]), atoi(cls.cg_argv[2]), atoi(cls.cg_argv[3]), atoi(cls.cg_argv[4]), CG_ImageIndex(cls.cg_argv[0]));
	}
	else if(Com_MatchToken("DrawCenterPic"))
	{
		// draw named pic
		if(!CG_ParseArgs( 3 )) return false;
		CG_DrawCenterPic( atoi(cls.cg_argv[1]), atoi(cls.cg_argv[2]), CG_ImageIndex(cls.cg_argv[0]));
	}
	else if(Com_MatchToken("DrawBarImage"))
	{
		// fill image with "common/fill_rect"
		if(!CG_ParseArgs( 6 )) return false;
		if(!CG_GetInteger(cls.cg_argv[0], &value))
			MsgDev(D_ERROR, "%s: can't use undefined alias %s\n", cls.cg_builtin, cls.cg_argv[0]);
		else CG_DrawBarImage((float)value, CG_ImageIndex(cls.cg_argv[1]), atoi(cls.cg_argv[2]), atoi(cls.cg_argv[3]), atoi(cls.cg_argv[4]), atoi(cls.cg_argv[5]));
	}
	else if(Com_MatchToken("DrawCenterBarImage"))
	{
		// fill image with "common/fill_rect"
		if(!CG_ParseArgs( 4 )) return false;
		if(!CG_GetInteger(cls.cg_argv[0], &value))
			MsgDev(D_ERROR, "%s: can't use undefined alias %s\n", cls.cg_builtin, cls.cg_argv[0]);
		else CG_DrawCenterBarImage( (float)value, CG_ImageIndex(cls.cg_argv[1]), atoi(cls.cg_argv[2]), atoi(cls.cg_argv[3]));
	}
	else if(Com_MatchToken("DrawBarGeneric"))
	{
		// draw progress bar
		if(!CG_ParseArgs( 5 )) return false;
		if(!CG_GetInteger(cls.cg_argv[0], &value))
			MsgDev(D_ERROR, "%s: can't use undefined alias %s\n", cls.cg_builtin, cls.cg_argv[0]);
		else CG_DrawBarGeneric( (float)value, atoi(cls.cg_argv[1]), atoi(cls.cg_argv[2]), atoi(cls.cg_argv[3]), atoi(cls.cg_argv[4]));
	}
	else if(Com_MatchToken("DrawCenterBarGeneric"))
	{
		// draw progress bar
		if(!CG_ParseArgs( 3 )) return false;
		if(!CG_GetInteger(cls.cg_argv[0], &value))
			MsgDev(D_ERROR, "%s: can't use undefined alias %s\n", cls.cg_builtin, cls.cg_argv[0]);
		else CG_DrawCenterBarGeneric( (float)value, atoi(cls.cg_argv[1]), atoi(cls.cg_argv[2]));
	}
	else if(Com_MatchToken("DrawString"))
	{
		// draw named pic
		if(!CG_ParseArgs( 3 )) return false;
                   	SCR_DrawBigString( atoi(cls.cg_argv[1]), atoi(cls.cg_argv[2]), CG_ImageIndex(cls.cg_argv[0]), 1.0f );
	}
	else if(Com_MatchToken("DrawCenterString"))
	{
		// draw named pic
		if(!CG_ParseArgs( 1 )) return false;
                   	CG_DrawCenterStringBig(CG_ImageIndex(cls.cg_argv[0]), 1.0f );
	}
	else if(Com_MatchToken("(") || Com_MatchToken(")"));
	else 
	{
		MsgDev(D_WARN, "%s: can't exec function %s\n", cls.cg_builtin, com_token );
		while(Com_TryToken());
	}
	return true;
}


/*
================
CG_ExecuteProgram

Hudprogram executor
================
*/
void CG_ExecuteProgram( char *section )
{
	bool	skip = true;

	Com_ResetScript();
	strncpy( cls.cg_function, section, MAX_QPATH );
	cls.cg_depth = 0;

	// not loaded
	if(!cls.cg_init) return;

	// section not specified
	if(!section || !*section )
		return;

	while(Com_GetToken( true ))
	{
		// first, we must find start of section
		if(Com_MatchToken("void"))
		{
			Com_GetToken( false ); // get section name
			if(Com_MatchToken( section )) skip = false;
			CG_ParseArgs( 1 ); // go to end of line
		}

		if(skip) continue;

		if(Com_MatchToken("return"))
		{
                    	break;
                    }
		else if(!CG_ExecBuiltins())
		{
			// end of section
			if(!cls.cg_depth) break;
			continue;
		}

	}
	CG_ResetColor(); // don't forget reset color
}