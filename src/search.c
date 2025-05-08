/*
 * Copyright 1993-2002 Christopher Seiwald and Perforce Software, Inc.
 *
 * This file is part of Jam - see jam.c for Copyright information.
 */

/*
 * search.c - find a target along $(SEARCH) or $(LOCATE) 
 *
 * 11/04/02 (seiwald) - const-ing for string literals
 */

# include "jam.h"
# include "lists.h"

# include "parse.h"
# include "rules.h"

# include "search.h"
# include "timestamp.h"
# include "pathsys.h"
# include "variable.h"
# include "newstr.h"

# include "filesys.h"

static const char *
search_helper( 
	   const char *target,
	   time_t	*time,
	   LIST *(*varget)( const char*, void* ),
	   void *userData,
	   int uncached )
{
	PATHNAME f[1];
	LIST	*varlist;
	char	buf[ MAXJPATH ];
#ifdef OPT_PATH_BINDING_EXT
	PATHNAME bf[1];
#endif
	
	/* Parse the filename */
	
	path_parse( target, f );
	
	f->f_grist.ptr = 0;
	f->f_grist.len = 0;
	
#ifdef OPT_PATH_BINDING_EXT
	if ( list_first(varlist = varget( "BINDING", userData )) )
	{
		path_parse( list_value(list_first(varlist)), bf );
		
		f->f_dir = bf->f_dir;
		f->f_base = bf->f_base;
		f->f_suffix = bf->f_suffix;
	}
#endif
	
	if( list_first(varlist = varget( "LOCATE", userData )) )
	{
		int absolute = 0;

		f->f_root.ptr = list_value(list_first(varlist));
		f->f_root.len = (int)(strlen( list_value(list_first(varlist)) ));
		
#ifdef OPT_ROOT_PATHS_AS_ABSOLUTE_EXT
		path_build( f, buf, 1, 1 );
#else
		path_build( f, buf, 1 );
#endif
		
#ifdef OS_NT
		absolute =
			( ( ( buf[0] >= 'a'  &&  buf[0] <= 'z' )  ||  ( buf[0] >= 'A'  &&  buf[0] <= 'Z' ) )  &&
			buf[1] == ':' )  ||  ( buf[0] == '/'  ||  buf[0] == '\\' );
#else
		absolute = buf[0] == '/';
#endif

		if (!absolute)
		{
			LIST *subdir = varget( "SUBDIR", userData );
			if ( list_first( subdir ) )
			{
				PATHNAME rf[1];
				char	buf2[ MAXJPATH ];
				memset( rf, 0, sizeof( PATHNAME ) );
				rf->f_root.ptr = list_value( list_first( subdir ) );
				rf->f_root.len = (int)(strlen( rf->f_root.ptr ));
				rf->f_dir.ptr = buf;
				rf->f_dir.len = (int)strlen(buf);

#ifdef OPT_ROOT_PATHS_AS_ABSOLUTE_EXT
				path_build( rf, buf2, 1, 1 );
#else
				path_build( rf, buf2, 1 );
#endif
				strcpy( buf, buf2 );
			}
		}

		if( DEBUG_SEARCH )
			printf( "locate %s: %s\n", target, buf );
		
		if ( uncached )
		{
			file_time( buf, time );
		}
		else
		{
			timestamp( buf, time, 0 );
		}
		
		return newstr( buf );
	}
	else if( list_first(varlist = varget( "SEARCH", userData )) )
	{
		LIST *searchextensionslist;
		LISTITEM* var = list_first(varlist);
		while( var )
		{
			f->f_root.ptr = list_value(var);
			f->f_root.len = (int)(strlen( list_value(var) ));
			
#ifdef OPT_ROOT_PATHS_AS_ABSOLUTE_EXT
			path_build( f, buf, 1, 1 );
#else
			path_build( f, buf, 1 );
#endif
			
			if( DEBUG_SEARCH )
				printf( "search %s: %s\n", target, buf );
			
			if ( uncached )
			{
				file_time( buf, time );
			}
			else
			{
				timestamp( buf, time, 0 );
			}
			
			if( *time )
				return newstr( buf );
			
			var = list_next( var );
		}
		
		searchextensionslist = varget( "SEARCH_EXTENSIONS", userData );
		if ( list_first(searchextensionslist) )
		{
			LISTITEM* ext = list_first(searchextensionslist);
			for ( ; ext; ext = list_next(ext) )
			{
				LISTITEM* var = list_first(varlist);
				while( var )
				{
					f->f_root.ptr = list_value(var);
					f->f_root.len = (int)(strlen( list_value(var) ));
					
#ifdef OPT_ROOT_PATHS_AS_ABSOLUTE_EXT
					strcpy( path_build( f, buf, 1, 1 ), list_value(ext) );
#else
					strcpy( path_build( f, buf, 1 ), list_value(ext) );
#endif
					
					if( DEBUG_SEARCH )
						printf( "search %s: %s\n", target, buf );
					
					if ( uncached )
					{
						file_time( buf, time );
					}
					else
					{
						timestamp( buf, time, 0 );
					}
					
					if( *time )
						return newstr( buf );
					
					var = list_next( var );
				}
			}
		}			
	}
	
	/* Look for the obvious */
	/* This is a questionable move.  Should we look in the */
	/* obvious place if SEARCH is set? */
	
	f->f_root.ptr = 0;
	f->f_root.len = 0;
	
#ifdef OPT_ROOT_PATHS_AS_ABSOLUTE_EXT
	path_build( f, buf, 1, 1 );
#else
	path_build( f, buf, 1 );
#endif
	
	if( DEBUG_SEARCH )
		printf( "search %s: %s\n", target, buf );
	
	if ( uncached )
	{
		file_time( buf, time );
	}
	else
	{
		timestamp( buf, time, 0 );
	}
	
	return newstr( buf );
}


static LIST *standard_search_var_get( const char *symbol, void *userData ) {
	return var_get( symbol );
}

const char *search( const char *target, time_t	*time ) {
	return search_helper( target, time, standard_search_var_get, NULL, 0 );
}

static LIST *search_using_target_settings_var_get( const char *symbol, void *userData ) {
	SETTINGS* settings = quicksettingslookup( (TARGET*)userData, symbol );
	return settings ? settings->value : NULL;
}

const char *search_using_target_settings( TARGET *t, const char *target, time_t *time ) {
	return search_helper( target, time, search_using_target_settings_var_get, t, 0 );
}

const char *search_uncached( const char *target, time_t	*time ) {
	return search_helper( target, time, standard_search_var_get, NULL, 1 );
}
