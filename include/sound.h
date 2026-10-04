#ifndef SOUND_H
#define SOUND_H

#include "libs.h"

#include "loop_mp3_bin.h"
#include "c_mp3_bin.h"
#include "d_mp3_bin.h"
#include "m_mp3_bin.h"

typedef struct
{
    short *pcm;
    int pcm_size;
    int freq;
    int stereo;
} SoundEffect;

SoundEffect sfx_decide;
SoundEffect sfx_cancel;
SoundEffect sfx_move;

void init_sfx();
void PlayBGM();
void PlaySFX(SoundEffect *sfx);

#endif