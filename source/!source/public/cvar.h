//=======================================================================
//			Copyright XashXT Group 2007 ©
//			cvar.h - console variables
//=======================================================================
#ifndef CVAR_H
#define CVAR_H

#define CVAR_ARCHIVE	1	// set to cause it to be saved to vars.rc
#define CVAR_USERINFO	2	// added to userinfo  when changed
#define CVAR_SERVERINFO	4	// added to serverinfo when changed
#define CVAR_NOSET		8	// don't allow change from console at all, but can be set from the command line
#define CVAR_LATCH		16	// save changes until server restart

#define CVAR_MAXFLAGSVAL	31	//maximum number of flags

// nothing outside the Cvar_*() functions should modify these fields!
typedef struct cvar_s
{
	int	flags;
	char	*name;

	char	*string;
	char	*description;
	int	integer;
	float	value;
	float	vector[3];
	char	*defstring;

	struct cvar_s *next;
	struct cvar_s *hash;

	//FIXME: remove these old variables
	char	*latched_string;	// for CVAR_LATCH vars
	bool	modified;		// set each time the cvar is changed
} cvar_t;

extern	cvar_t	*cvar_vars;

cvar_t *Cvar_FindVar (char *var_name);

cvar_t *Cvar_Get (char *var_name, char *value, int flags);
// creates the variable if it doesn't exist, or returns the existing one
// if it exists, the value will not be changed, but flags will be ORed in
// that allows variables to be unarchived without needing bitflags

cvar_t 	*Cvar_Set (char *var_name, char *value);
// will create the variable if it doesn't exist

cvar_t *Cvar_ForceSet (char *var_name, char *value);
// will set the variable even if NOSET or LATCH

cvar_t 	*Cvar_FullSet (char *var_name, char *value, int flags);

void	Cvar_SetValue (char *var_name, float value);
// expands value to a string and calls Cvar_Set

float	Cvar_VariableValue (char *var_name);
// returns 0 if not defined or non numeric

char	*Cvar_VariableString (char *var_name);
// returns an empty string if not defined

char 	*Cvar_CompleteVariable (char *partial);
// attempts to match a partial variable name for command line completion
// returns NULL if nothing fits

void	Cvar_GetLatchedVars (void);
// any CVAR_LATCHED variables that have been set will now take effect

bool Cvar_Command (void);
// called by Cmd_ExecuteString when Cmd_Argv(0) doesn't match a known
// command.  Returns true if the command was a variable reference that
// was handled. (print or change)

void 	Cvar_WriteVariables (char *path);
// appends lines containing "set variable value" for all variables
// with the archive flag set to true.

void	Cvar_Init (void);

char	*Cvar_Userinfo (void);
// returns an info string containing all the CVAR_USERINFO cvars

char	*Cvar_Serverinfo (void);
// returns an info string containing all the CVAR_SERVERINFO cvars

extern	bool	userinfo_modified;
// this is set each time a CVAR_USERINFO variable is changed
// so that the client knows to send it to the server

#endif//CVAR_H