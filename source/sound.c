#include "sound.h"

#include "soundlib/spu_soundlib.h"
#include "soundlib/audioplayer.h"

int LoadSFX(SoundEffect *sfx, const void *data, int size)
{
    sfx->pcm_size = size * 20;
	sfx->pcm = memalign(32, sfx->pcm_size);
    if(!sfx->pcm) return 0;
    if( DecodeAudio((void *)data,size,sfx->pcm,&sfx->pcm_size,&sfx->freq,&sfx->stereo) != 0)
    {
        free(sfx->pcm);
        sfx->pcm = NULL;
        return 0;
    }
	return 1;
}

void init_sfx()
{
	LoadSFX(&sfx_cancel, c_mp3_bin, c_mp3_bin_size);
	LoadSFX(&sfx_decide, d_mp3_bin, d_mp3_bin_size);
	LoadSFX(&sfx_move, m_mp3_bin, m_mp3_bin_size);
}

void PlayBGM()
{
	FILE *fp = fopen("/dev_hdd0/game/ACIDSTORE/USRDIR/menu.mp3", "rb");
	
	if(!fp)
    {
        printf("Error! No music found.\n");
        return;
    }
    
    PlayAudiofd(fp, 0, AUDIO_INFINITE_TIME);
}

void PlaySFX(SoundEffect *sfx)
{
	int voice = SND_GetFirstUnusedVoice();
    if (voice < 1) return;
    SND_SetVoice(voice, sfx->stereo ? VOICE_STEREO_16BIT : VOICE_MONO_16BIT, sfx->freq,
        0, sfx->pcm, sfx->pcm_size, 255, 255, NULL);
}