//
//  interleaved_despacer.h
//  SpacePruner
//
//  Created by Derek Ledbetter on 2017-07-07.
//  Copyright © 2017 Derek Ledbetter. All rights reserved.
//

#ifndef interleaved_despacer_h
#define interleaved_despacer_h

#include <ConditionalMacros.h>
#include <stddef.h>

#if __ARM_NEON
size_t neon_interleaved_despace(char *bytes, size_t howmany);
#endif

#endif /* interleaved_despacer_h */
