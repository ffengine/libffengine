#!/bin/bash

# 天天影音的配置文件

export DEVELOPER_ROOT_PATH=/Developer
export CC=${DEVELOPER_ROOT_PATH}/Platforms/iPhoneOS.platform/Developer/usr/bin/arm-apple-darwin10-gcc-4.2.1
export SYS_ROOT=${DEVELOPER_ROOT_PATH}/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS4.2.sdk

./configure --logfile=./logffmpeg.txt \
--cpu=cortex-a8 \
--enable-cross-compile \
--cc=${CC} \
--as='gas-preprocessor.pl ${CC}' \
--prefix=./output.tt/ \
--target-os=darwin \
--arch=armv7-a \
--disable-stripping \
--disable-version3 \
--disable-gpl \
--disable-nonfree \
--disable-ffmpeg \
--disable-ffplay \
--disable-ffserver \
--disable-ffprobe \
--disable-encoders \
--disable-muxers \
--disable-devices \
--disable-avdevice \
--disable-avfilter \
--disable-filters \
--disable-debug \
--enable-memalign-hack \
--disable-parser='adx,dca,dnxhd,dirac,cavsvideo,pnm,mlp' \
--disable-demuxer='act,adx,adf,anm,aea,apc,ape,avs,bethsoftvid,bfi,bink,bmv,c93,cdg,cavsvideo,daud,dxa,dirac,dnxhd,dsicin,ea,ea_cdata,fourxm,gxf' \
--disable-demuxer='ico,iff,iss,ipmovie,lmlm4,mxf,mm,mmf,nuv,mtv,pmp,pva,rl2,rdt,rpl,rso,roq,sap,sol,sox,str,smjpeg,smacker,segafilm,sox,siff,thp,tmv,tty,vmd,vqf,w64,wsaud,wc3,wsvqa,xa,yop' \
--disable-decoders \
--enable-decoder='h263,h263i,h264,flv,wmav1,wmav2,wmavoice,wnv1,wmapro,wavpack,cook,wmv1,wmv2,wmv3,mpeg4,mpegvideo,mpeg1video,mpeg2video,msmpeg4v1,msmpeg4v2,msmpeg4v3' \
--enable-decoder='rv10,rv20,rv30,rv40,svq1,svq3,vp3,vp5,vp6,vp6a,vp6f,vp8' \
--enable-decoder='ansi,prores,mjpegb,prores_lgpl,mjpeg,smc,rpza,r210,theora,v210,v210x,v308,v410,yuv4,amv' \
--enable-decoder='aac,ac3,aasc,aac_latm,eac3,amrnb,amrwb,mp3,mp2,mp1,mp1float,mp2float,mp3adu,mp3adufloat,mp3float,mp3on4float,mp3on4' \
--enable-decoder='alac,flac,eac3,als,vorbis,ra_144,ra_288,g729,g723_1,qcelp' \
--enable-decoder='adpcm_ima_iss,adpcm_ima_ea_sead,adpcm_ima_ea_eacs,adpcm_ima_dk4,adpcm_ima_dk3,adpcm_ima_apc,adpcm_ima_amv,adpcm_g726,adpcm_g722,adpcm_ea_xas,adpcm_ea_r3,adpcm_ea_r2,adpcm_ea_r1,adpcm_ea_maxis_xa,adpcm_ea,adpcm_ct,adpcm_adx,adpcm_4xm' \
--enable-decoder='adpcm_yamaha,adpcm_xa,adpcm_thp,adpcm_swf,adpcm_sbpro_4,adpcm_sbpro_3,adpcm_sbpro_2,adpcm_ms,adpcm_ima_ws,adpcm_ima_wav,adpcm_ima_smjpeg,adpcm_ima_qt' \
--enable-decoder='pcm_mulaw,pcm_alaw,pcm_zork,pcm_bluray,pcm_dvd,pcm_u8,pcm_s8,pcm_s8_planar,pcm_s16le,pcm_s16be,pcm_u16be,pcm_u16le,pcm_u24be,pcm_u24le,pcm_s24be,pcm_s24daud,pcm_s24le,pcm_s32be,pcm_s32le,pcm_u32be,pcm_u32le' \
--enable-decoder='srt,ass,dvbsub,dvdsub,pgssub,xsub' \
--disable-protocol=crypto \
--enable-asm \
--disable-small \
--disable-altivec \
--disable-iwmmxt \
--disable-mmi \
--disable-bsfs \
--disable-vis \
--disable-yasm \
--enable-pic \
--enable-sram \
--enable-mpegaudiodsp \
--disable-doc \
--disable-zlib \
--extra-cflags='-fPIC -D_FILE_OFFSET_BITS=64 -D__thumb__ -mfloat-abi=softfp -mfpu=neon -marm -march=armv7-a -mtune=cortex-a8 -O3 -fasm -fno-short-enums -fno-strict-aliasing -finline-limit=300 -isysroot ${SYS_ROOT}' \
--extra-ldflags='-isysroot ${SYS_ROOT} -lc -lm -ldl' \
--enable-neon \
--enable-armv6t2 \
--enable-armv6 \
--enable-armv5te \
--enable-armvfp

