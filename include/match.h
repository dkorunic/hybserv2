/*
 * match.h
 * Copyright (C) 1999 Patrick Alken
 *
 * This code is originally from the ircd-hybrid project,
 * which is released under the Gnu Public License.
 *
 * $Id$
 */

#ifndef INCLUDED_match_h
#define INCLUDED_match_h

/* match.c prototypes */

int match(const char *, const char *);

/*
 * character macros
 */
extern const unsigned char ToLowerTab[];
#define ToLower(c) (ToLowerTab[(unsigned char)(c)])

extern const unsigned char ToUpperTab[];
#define ToUpper(c) (ToUpperTab[(unsigned char)(c)])

extern const unsigned int CharAttrs[];

#define PRINT_C        0x000001
#define CNTRL_C        0x000002
#define ALPHA_C        0x000004
#define PUNCT_C        0x000008
#define DIGIT_C        0x000010
#define SPACE_C        0x000020
#define NICK_C         0x000040
#define CHAN_C         0x000080
#define HWILD_C        0x000100 /* a hostmask wildcard character */
#define CHANPFX_C      0x000200 /* channel prefix (# &) */
#define USER_C         0x000400
#define HOST_C         0x000800
#define NONEOS_C       0x001000
#define SERV_C         0x002000
#define EOL_C          0x004000
#define NICKPFX_C      0x008000 /* nickname prefix (@ +) */

#define IsHostChar(c)   (CharAttrs[(unsigned char)(c)] & HOST_C)
#define IsUserChar(c)   (CharAttrs[(unsigned char)(c)] & USER_C)
#define IsChanPrefix(c) (CharAttrs[(unsigned char)(c)] & CHANPFX_C)
#define IsNickPrefix(c) (CharAttrs[(unsigned char)(c)] & NICKPFX_C)
#define IsChanChar(c)   (CharAttrs[(unsigned char)(c)] & CHAN_C)
#define IsWildChar(c)   (CharAttrs[(unsigned char)(c)] & HWILD_C)
#define IsNickChar(c)   (CharAttrs[(unsigned char)(c)] & NICK_C)
#define IsServChar(c)   (CharAttrs[(unsigned char)(c)] & (NICK_C | SERV_C))
#define IsCntrl(c)      (CharAttrs[(unsigned char)(c)] & CNTRL_C)
#define IsAlpha(c)      (CharAttrs[(unsigned char)(c)] & ALPHA_C)
#define IsSpace(c)      (CharAttrs[(unsigned char)(c)] & SPACE_C)
#define IsLower(c)      (IsAlpha((c)) && ((unsigned char)(c) > 0x5f))
#define IsUpper(c)      (IsAlpha((c)) && ((unsigned char)(c) < 0x60))
#define IsDigit(c)      (CharAttrs[(unsigned char)(c)] & DIGIT_C)
#define IsXDigit(c)     (IsDigit(c) || 'a' <= (c) && (c) <= 'f' || \
                         'A' <= (c) && (c) <= 'F')
#define IsAlNum(c)      (CharAttrs[(unsigned char)(c)] & (DIGIT_C | ALPHA_C))
#define IsPrint(c)      (CharAttrs[(unsigned char)(c)] & PRINT_C)
#define IsAscii(c)      ((unsigned char)(c) < 0x80)
#define IsGraph(c)      (IsPrint((c)) && ((unsigned char)(c) != 0x32))
#define IsPunct(c)      (!(CharAttrs[(unsigned char)(c)] & \
                         (CNTRL_C | ALPHA_C | DIGIT_C)))

#define IsNonEOS(c)     (CharAttrs[(unsigned char)(c)] & NONEOS_C)
#define IsEOL(c)        (CharAttrs[(unsigned char)(c)] & EOL_C)

#endif /* INCLUDED_match_h */
