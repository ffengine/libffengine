#include "el_media_player.h"
#include "el_media_player_engine.h"


EL_STATIC EL_BOOL el_media_player_init()
{
	EL_BOOL rst;

	rst = el_player_engine_init();
	if(rst)
	{
		return EL_TRUE;
	}

	return EL_FALSE;
}

EL_STATIC EL_BOOL el_media_player_uninit()
{	
	return el_player_engine_uninit();
}

EL_STATIC EL_BOOL el_media_player_open(EL_CONST EL_UTF8* a_pszUrlOrPathname)
{
	if (EL_NULL == a_pszUrlOrPathname)
		return EL_FALSE;
	
	return el_player_engine_open_media(a_pszUrlOrPathname);		
}

EL_STATIC EL_BOOL el_media_player_close()
{
	return el_player_engine_close_media();	
}

EL_STATIC EL_BOOL el_media_player_start()
{
	return el_player_engine_start();
}

EL_STATIC EL_BOOL el_media_player_stop()
{
	return el_player_engine_stop();
}

EL_STATIC EL_BOOL el_media_player_pause()
{
	return el_player_engine_pause();	
}

EL_STATIC EL_INT  el_media_player_seek_to(EL_SIZE a_SeekPos)
{
	return el_player_engine_seek_to(a_SeekPos);	
}

EL_STATIC EL_BOOL el_media_player_play()
{
	return el_player_engine_play();
}


EL_STATIC EL_BOOL el_media_player_get_description(ELMediaMuxerDesc_t *aMediaMuxerDesc,ELVideoDesc_t *aVideoDesc, ELAudioDesc_t *aAudioDesc)
{
	return el_player_engine_get_description(aMediaMuxerDesc, aVideoDesc, aAudioDesc);
}


#if 1
EL_VOID el_media_player_set_dynamic_reference_table(el_dynamic_reference_table_t tbl)
{
	el_media_player_blk.dynamic_reference_table = tbl;
}
#endif

