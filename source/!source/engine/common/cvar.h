//=======================================================================
//			Copyright XashXT Group 2007 ©
//			cvar.h - console variables
//=======================================================================
#ifndef CVAR_H
#define CVAR_H

extern cvar_t *cvar_vars;
cvar_t *Cvar_FindVar (const char *var_name);
#define Cvar_Get(name, value, flags) _Cvar_Get( name, value, flags, "no description" )
cvar_t *_Cvar_Get (const char *var_name, const char *value, int flags, const char *description);
void Cvar_Set( const char *var_name, const char *value);
cvar_t *Cvar_Set2 (const char *var_name, const char *value, bool force);
void Cvar_CommandCompletion( void(*callback)(const char *s, const char *m));
void Cvar_FullSet (char *var_name, char *value, int flags);
void Cvar_SetLatched( const char *var_name, const char *value);
void Cvar_SetValue( const char *var_name, float value);
float Cvar_VariableValue (const char *var_name);
char *Cvar_VariableString (const char *var_name);
bool Cvar_Command (void);
void Cvar_WriteVariables( file_t *f );
void Cvar_Init (void);
char *Cvar_Userinfo (void);
char *Cvar_Serverinfo (void);
extern bool userinfo_modified;

#endif//CVAR_H