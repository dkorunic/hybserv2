/*
 * mystring.h
 * HybServ2 Services by HybServ2 team
 *
 * $Id$
 */

#ifndef INCLUDED_mystring_h
#define INCLUDED_mystring_h

char *StrToupper(char *str);
char *StrTolower(char *str);
char *GetString(int ac, char **av);
int SplitBuf(char *buf, char ***array);

#endif /* INCLUDED_mystring_h */
