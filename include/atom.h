/* $Id: atom.h 6 2007-01-22 00:45:22Z drhanson $ */
#ifndef ATOM_INCLUDED
#define ATOM_INCLUDED

typedef 	 void (*AtomMapFn_T)(const char *str, int len, void *aux);
extern       int   Atom_length(const char *str);
extern const char *Atom_new   (const char *str, int len);
extern const char *Atom_string(const char *str);
extern const char *Atom_int   (long n);
extern 		 void  Atom_free(const char *str);
extern		 void  Atom_reset(void);
extern		 void  Atom_vload(const char *str, ...);
extern		 void  Atom_aload(const char *str[]);
extern		 void  Atom_map(AtomMapFn_T mapfn, void *aux);
#endif
