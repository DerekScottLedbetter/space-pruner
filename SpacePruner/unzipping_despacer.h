//
//  unzipping_despacer.h
//  SpacePruner
//
//  Created by Derek Ledbetter on 2017-07-17.
//

#ifndef unzipping_despacer_h
#define unzipping_despacer_h

#include <ConditionalMacros.h>
#include <stddef.h>

#ifdef __ARM_NEON
size_t neon_unzipping_despace(char *bytes, size_t howmany);
#endif

#endif /* unzipping_despacer_h */
