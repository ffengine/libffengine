
#ifndef _EL_MEDIA_INFO_H_
#define _EL_MEDIA_INFO_H_

#include "el_std.h"

EL_STATIC EL_BOOL el_get_media_description(const char *pszUrlOrPathname,
                                           ELMediaMuxerDesc_t *aMediaMuxerDesc,
                                           ELVideoDesc_t *aVideoDesc,
                                           ELAudioDesc_t *aAudioDesc);


#endif  // _EL_MEDIA_INFO_H_
