#include <pthread.h>
#import <Foundation/Foundation.h>
#import <AudioToolbox/AudioQueue.h>
#import <AudioToolbox/AudioServices.h>

#include "el_media_player.h"
#include "el_platform_audio_render_filter.h"
#include "el_log.h"
#include "el_platform_sys_info.h"

#define AUDIO_LOG 0
#define ENABLE_AUDIO 1

#define MY_AUDIO_QUEUE_NUM      3

typedef struct MyAudioStatus {
	AudioQueueRef audioQueue;                                       // the audio queue
	AudioQueueBufferRef audioQueueBuffer[MY_AUDIO_QUEUE_NUM];		// audio queue buffers
	
	unsigned int fillBufferIndex;	// the index of the audioQueueBuffer that is being filled
	long bytesFilled;				// how many bytes have been filled
	long bytesReceived;              // how many bytes have been received
    
    unsigned char *sampleData;      // sample data from ffmpeg
    
	bool inuse[MY_AUDIO_QUEUE_NUM]; // flags to indicate that a buffer is still in use
	bool aqStarted;					// flag to indicate that the queue has been started
	bool aqFailed;					// flag to indicate an error occurred
    bool playStarted;               // flag to indicate the audio play started
    bool playStop;                  // flag to indicate the play stop
    bool playPause;                 // flag to indicate the play pause

pthread_mutex_t mutex_aq;		// a mutex_aq to protect the inuse flags    
    pthread_mutex_t mutex_pause;
    
	pthread_cond_t cond_aq_buf;		// a condition varable for handling the inuse flags
} MyAudioStatus;


static long kAQBufSize = 32 * 1024;		// number of bytes in each audio queue buffer

static el_audio_get_sample_cb audio_get_sample;
static el_audio_consume_sample_cb audio_consume_sample;
static el_audio_set_sample_first_play_time_cb audio_set_sample_first_play_time_cb;

static pthread_t g_workerThread = NULL;


static MyAudioStatus myAudioStatus = {0};
static ELAudioRenderAudioParms_s audioParms = {0};
static ELAudioRenderFilter_t audioRenderfilter = {0};


EL_STATIC void GetSampleDataFunc(void *pData);
EL_STATIC int MyFindQueueBuffer(AudioQueueBufferRef inBuffer);

EL_STATIC EL_VOID el_audio_render_filter_init(el_audio_get_sample_cb get_cb, el_audio_consume_sample_cb consume_cb,
                                     el_audio_set_sample_first_play_time_cb set_sample_first_play_time_cb) {
#if ENABLE_AUDIO
    
#if AUDIO_LOG 
    EL_DBG_LOG("AudioLog : el_audio_render_filter_init\n");
#endif

    g_workerThread = NULL;
    
    audio_get_sample = get_cb;
    audio_consume_sample = consume_cb;
	audio_set_sample_first_play_time_cb = set_sample_first_play_time_cb;
    
	// initialize a mutex and condition so that we can block on buffers in use.
	pthread_mutex_init(&myAudioStatus.mutex_aq, NULL);
    //pthread_mutex_init(&myAudioStatus.mutex_over, NULL);
    pthread_mutex_init(&myAudioStatus.mutex_pause, NULL);
	pthread_cond_init(&myAudioStatus.cond_aq_buf, NULL);
    //pthread_cond_init(&myAudioStatus.cond_wait, NULL);

#endif
}

EL_STATIC EL_VOID el_audio_render_filter_uninit() {
	int i;
#if ENABLE_AUDIO
#if AUDIO_LOG 
    EL_DBG_LOG("AudioLog : el_audio_render_filter_uninit\n");
#endif

    
//    EL_DBG_LOG("el_audio_render_filter_uninit...\n");
    
    myAudioStatus.aqStarted = false;
    myAudioStatus.playStarted = false;
    myAudioStatus.playStop = true;
    myAudioStatus.playPause = false;
    
    if (g_workerThread) 
    {
        EL_DBG_LOG("before join audio thread\n");
        pthread_join(g_workerThread, NULL);
        EL_DBG_LOG("after join audio thread\n");
    }
    
    g_workerThread = NULL;
    
    pthread_mutex_lock(&myAudioStatus.mutex_aq);
	pthread_cond_signal(&myAudioStatus.cond_aq_buf);
    AudioQueueDispose(myAudioStatus.audioQueue, true);
    pthread_mutex_unlock(&myAudioStatus.mutex_aq);
    
    for ( i=0; i<MY_AUDIO_QUEUE_NUM; i++) {
        myAudioStatus.inuse[i] = false;
        myAudioStatus.audioQueueBuffer[i] = nil;
    }
    
    pthread_mutex_destroy(&myAudioStatus.mutex_aq);
    pthread_mutex_destroy(&myAudioStatus.mutex_pause);
    //pthread_mutex_destroy(&myAudioStatus.mutex_over);

    pthread_cond_destroy(&myAudioStatus.cond_aq_buf);
    //pthread_cond_destroy(&myAudioStatus.cond_wait);
    //pthread_cancel(g_workerThread);

    
    //el_render_stop_player();

#endif
}

EL_STATIC void HandleAudioQueueOutputCallback (void *inUserData, AudioQueueRef inAQ, AudioQueueBufferRef inBuffer) {
    // this is called by the audio queue when it has finished decoding our data. 
	// The buffer is now free to be reused.
#if AUDIO_LOG 
    EL_DBG_LOG("AudioLog : HandleAudioQueueOutputCallback\n");
#endif
        
    if(myAudioStatus.playStop)
    {
        EL_DBG_LOG("audio render already stopped, directly return in call back\n!");
        return;
    }
	
    unsigned bufIndex = MyFindQueueBuffer(inBuffer);
    //EL_DBG_LOG("bufIndex : %u\n", bufIndex);
	
	// signal waiting thread that the buffer is free.
	pthread_mutex_lock(&myAudioStatus.mutex_aq);
	myAudioStatus.inuse[bufIndex] = false;
	pthread_cond_signal(&myAudioStatus.cond_aq_buf);
	pthread_mutex_unlock(&myAudioStatus.mutex_aq);
}

EL_STATIC EL_VOID el_audio_render_filter_set_param(ELAudioRenderAudioParms_s a_AudioRenderAudioPams) {
	int i;
#if ENABLE_AUDIO
#if AUDIO_LOG 
    EL_DBG_LOG("AudioLog : el_audio_render_filter_set_param\n");
#endif

    
//    EL_DBG_LOG("el_audio_render_filter_set_param...\n");
    
    OSStatus err = noErr;
    float ftemp = 0.8f;
    AudioStreamBasicDescription asbd = {0};
    asbd.mSampleRate = a_AudioRenderAudioPams.uiSampleRate;
    asbd.mFormatID = kAudioFormatLinearPCM;

    asbd.mChannelsPerFrame = a_AudioRenderAudioPams.iChannelCount;
    if(a_AudioRenderAudioPams.iChannelCount > 2)
    {
        asbd.mChannelsPerFrame = 2;
    }
   
    asbd.mFormatFlags = kLinearPCMFormatFlagIsPacked | kLinearPCMFormatFlagIsSignedInteger;
    asbd.mBitsPerChannel = 16;
    asbd.mBytesPerPacket = asbd.mBytesPerFrame = 2 * asbd.mChannelsPerFrame;
    asbd.mFramesPerPacket = 1;
    
        
    kAQBufSize = (asbd.mSampleRate * asbd.mChannelsPerFrame * 16 / 8) * ftemp;
    if(kAQBufSize < 2000) kAQBufSize = 2000;
    
    AudioQueueRef *pAudioQueue = &myAudioStatus.audioQueue;
    err = AudioQueueNewOutput(&asbd, HandleAudioQueueOutputCallback, nil, nil, nil, 0, pAudioQueue);
    EL_DBG_LOG("[AudioQueueNewOutput] err = %ld", err);
    for ( i = 0; i < MY_AUDIO_QUEUE_NUM; ++i) {
        err = AudioQueueAllocateBuffer(*pAudioQueue, kAQBufSize, &myAudioStatus.audioQueueBuffer[i]);
        if (err) {
            myAudioStatus.aqFailed = true;
            break;
        }
    }
    
    for ( i=0; i<MY_AUDIO_QUEUE_NUM; i++) 
    {
        myAudioStatus.inuse[i] = false;
    }

    audioParms = a_AudioRenderAudioPams;
    myAudioStatus.playStarted = true;
    myAudioStatus.playStop = false;
    myAudioStatus.playPause = true;
    
    myAudioStatus.fillBufferIndex = 0;
    myAudioStatus.bytesFilled = 0;
    
    //
    {    
    OSStatus err2;
    UInt32 size = sizeof(Float32);
    Float32 delay;
    err2 = AudioSessionGetProperty(kAudioSessionProperty_CurrentHardwareOutputLatency,&size,&delay);
//    EL_DBG_LOG("err = %d, delay = %f\n",err2,delay);
    err2 = AudioSessionGetProperty(kAudioSessionProperty_CurrentHardwareIOBufferDuration,&size,&delay);
//    EL_DBG_LOG("err = %d, delay2 = %f\n",err2,delay);
    }
    
    EL_DBG_LOG("pthread_create...\n");
    pthread_create(&g_workerThread, NULL, (void *)&GetSampleDataFunc, NULL);

#endif
}


EL_STATIC EL_VOID el_audio_render_filter_get_param(ELAudioRenderAudioParms_s* a_pAudioParams) {
#if ENABLE_AUDIO
    
    *a_pAudioParams = audioParms;

#endif
}

EL_STATIC EL_VOID el_audio_render_filter_set_volume(EL_SIZE a_nVolume) {
#if ENABLE_AUDIO
#if AUDIO_LOG 
    EL_DBG_LOG("AudioLog : el_audio_render_filter_set_volume\n");
#endif

    
    EL_DBG_LOG("el_audio_render_filter_set_volume\n");
    
    AudioQueueRef *pAudioQueue = &myAudioStatus.audioQueue;
    AudioQueueSetParameter(*pAudioQueue, kAudioQueueParam_Volume, (float)a_nVolume/(float)EL_AUDIO_MAX_VOLUME);
    
#endif
}

#if 0
EL_STATIC EL_VOID el_audio_render_filter_get_volume(EL_SIZE* a_nVolume) {
#if ENABLE_AUDIO
#if AUDIO_LOG 
    EL_DBG_LOG("AudioLog : el_audio_render_filter_get_volume\n");
#endif
    EL_DBG_LOG("el_audio_render_filter_get_volume\n");
    
    float fVolume = 1.0;
    AudioQueueRef *pAudioQueue = &myAudioStatus.audioQueue;
    AudioQueueGetParameter(*pAudioQueue, kAudioQueueParam_Volume, &fVolume);
    
#endif
}
#endif


EL_STATIC EL_VOID el_audio_render_filter_stop() {
#if ENABLE_AUDIO
#if AUDIO_LOG 
    EL_DBG_LOG("AudioLog : el_audio_render_filter_stop\n");
#endif

    
    EL_DBG_LOG("el_audio_render_filter_stop enter...\n");
    myAudioStatus.aqStarted = false;
    myAudioStatus.playStarted = false;
    myAudioStatus.playStop = true;

    EL_DBG_LOG("el_audio_render_filter_stop before semphore...\n");    
    pthread_mutex_lock(&myAudioStatus.mutex_aq);
    pthread_cond_broadcast(&myAudioStatus.cond_aq_buf);
    pthread_mutex_unlock(&myAudioStatus.mutex_aq);
    
    EL_DBG_LOG("el_audio_render_filter_stop after semphore...\n");    
    
    AudioQueueRef *pAudioQ = &myAudioStatus.audioQueue;
    if (*pAudioQ /*&& myAudioStatus.aqStarted*/) {
 //       myAudioStatus.aqStarted = false;
        AudioQueueReset(*pAudioQ);
        AudioQueueStop(*pAudioQ, false);
        //AudioQueueDispose(*pAudioQ, true);        
    }
    EL_DBG_LOG("el_audio_render_filter_stop exit...\n");    
    
#endif
}

EL_STATIC EL_VOID el_audio_render_filter_pause() {
#if ENABLE_AUDIO
#if AUDIO_LOG 
    EL_DBG_LOG("AudioLog : el_audio_render_filter_pause\n");
#endif

    
    EL_DBG_LOG("el_audio_render_filter_pause...\n");
    
    pthread_mutex_lock(&myAudioStatus.mutex_pause);
    myAudioStatus.playPause = true;
    pthread_mutex_unlock(&myAudioStatus.mutex_pause);

    OSStatus err = noErr;
    AudioQueueRef *pAudioQ = &myAudioStatus.audioQueue;
    if (pAudioQ) {
        err = AudioQueuePause(*pAudioQ);
        EL_DBG_LOG("el_audio_render_filter_pause AudioQueuePauset] err = %ld",err);
        myAudioStatus.aqStarted = false;
    }
        
    EL_DBG_LOG("el_audio_render_filter_end...\n");
#endif
}

EL_STATIC EL_VOID el_audio_render_filter_resume() {
#if ENABLE_AUDIO
#if AUDIO_LOG 
    EL_DBG_LOG("AudioLog : el_audio_render_filter_resume\n");
#endif

    pthread_mutex_lock(&myAudioStatus.mutex_pause);
    
    myAudioStatus.playPause = false;
    
    pthread_mutex_unlock(&myAudioStatus.mutex_pause);
    
    EL_DBG_LOG("myAudioStatus.playPause:%d\n", myAudioStatus.playPause);
        
    OSStatus err = noErr;
    if (!myAudioStatus.aqStarted) {
        err = AudioQueueStart(myAudioStatus.audioQueue, NULL);  
        EL_DBG_LOG("[AudioQueueStart] err = %ld",err);
    }
    myAudioStatus.aqStarted = true;
    
#endif
}

EL_STATIC EL_VOID el_audio_render_filter_reset() {
#if ENABLE_AUDIO
#if AUDIO_LOG 
    EL_DBG_LOG("AudioLog : el_audio_render_filter_reset\n");
#endif

    
    EL_DBG_LOG("el_audio_render_filter_reset\n");
    
    AudioQueueRef *pAudioQ = &myAudioStatus.audioQueue;
    if (pAudioQ) {
        AudioQueueReset(*pAudioQ);
    }
    
#endif
}


EL_STATIC OSStatus StartQueueIfNeeded() {
#if AUDIO_LOG 
    EL_DBG_LOG("AudioLog : StartQueueIfNeeded\n");
#endif

	OSStatus err = noErr;
	if (!myAudioStatus.aqStarted) 
    {		// start the queue if it has not been started already
		err = AudioQueueStart(myAudioStatus.audioQueue, NULL);
		if (err) {
            myAudioStatus.aqFailed = true;
            return err;
        }
        
		myAudioStatus.aqStarted = true;
		EL_DBG_LOG("audio queue started\n");
	}
	return err;
}


EL_STATIC OSStatus MyEnqueueBuffer() {
#if AUDIO_LOG 
    EL_DBG_LOG("AudioLog : MyEnqueueBuffer\n");
#endif

	OSStatus err = noErr;
	myAudioStatus.inuse[myAudioStatus.fillBufferIndex] = true;		// set in use flag
    
	// enqueue buffer
	AudioQueueBufferRef fillBuf = myAudioStatus.audioQueueBuffer[myAudioStatus.fillBufferIndex];
	fillBuf->mAudioDataByteSize = myAudioStatus.bytesFilled;
    err = AudioQueueEnqueueBuffer(myAudioStatus.audioQueue, fillBuf, 0, nil);
	if (err) {
        myAudioStatus.aqFailed = true;
        return err;
    }		
	StartQueueIfNeeded();
	
	return err;
}

EL_STATIC void WaitForFreeBuffer() {
#if AUDIO_LOG 
    EL_DBG_LOG("AudioLog : WaitForFreeBuffer\n");
#endif

	// go to next buffer
	if (++myAudioStatus.fillBufferIndex >= MY_AUDIO_QUEUE_NUM) {
        myAudioStatus.fillBufferIndex = 0;
    }
	myAudioStatus.bytesFilled = 0;		// reset bytes filled
	myAudioStatus.bytesReceived = 0;		// reset bytes received
//  myAudioStatus.sampleBytesRead = 0;   // reset bytes read
    
	// wait until next buffer is not in use
	pthread_mutex_lock(&myAudioStatus.mutex_aq); 
	while (myAudioStatus.inuse[myAudioStatus.fillBufferIndex] && !myAudioStatus.playStop) {
		//EL_DBG_LOG("... WAITING for audio buffer ...\n");
		pthread_cond_wait(&myAudioStatus.cond_aq_buf, &myAudioStatus.mutex_aq);
	}
	pthread_mutex_unlock(&myAudioStatus.mutex_aq);
}


//Find the index of inBuffer
EL_STATIC int MyFindQueueBuffer(AudioQueueBufferRef inBuffer) {
	int i;
#if AUDIO_LOG 
    EL_DBG_LOG("AudioLog : MyFindQueueBuffer\n");
#endif

	for ( i = 0; i < MY_AUDIO_QUEUE_NUM; ++i) {
		if (inBuffer == myAudioStatus.audioQueueBuffer[i]) 
			return i;
	}
	return -1;
}


#pragma mark -
#pragma mark Audio worker thread function

#define GET_SAMPLE_TIME_INTERVAL    50000
#define SLEEP_TIME_INTERVAL         10000

#if ENABLE_AUDIO
EL_STATIC void GetSampleDataFunc(void *pData) {
#if AUDIO_LOG 
    EL_DBG_LOG("AudioLog : GetSampleDataFunc\n");
#endif

    //pthread_mutex_lock(&myAudioStatus.mutex_over);
	while (!myAudioStatus.playStop) {
        NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
        
        unsigned char *pAudioData = NULL;
        long nDataLen = 0;
        
        while (true) {
            //EL_DBG_LOG("myAudioStatus.playStarted:%d, playPause:%d,playStop = %d...\n", myAudioStatus.playStarted, myAudioStatus.playPause
              //     ,myAudioStatus.playStop);
            
            if (myAudioStatus.playStop) break;
            
           // pthread_mutex_lock(&myAudioStatus.mutex_pause);
            if (myAudioStatus.playPause) {
                //EL_DBG_LOG("audio play pause...\n");
               // pthread_mutex_unlock(&myAudioStatus.mutex_pause);
                usleep(200000);
                break;
            } //else {
                //pthread_mutex_unlock(&myAudioStatus.mutex_pause);
            //}
            
            if (!myAudioStatus.playStarted || !audio_get_sample(&pAudioData, (int *)&nDataLen)) {
                //EL_DBG_LOG("audio_get_sample fail\n");
                usleep(50000);
                break;
            }
            
            //EL_DBG_LOG("audio_get_sample ok\n");
            
            OSStatus err = noErr;  
            myAudioStatus.bytesReceived = nDataLen;
            
            // if the space remaining in the buffer is not enough for this packet, then enqueue the buffer.
            //size_t bufSpaceRemaining = kAQBufSize - myAudioStatus.bytesFilled;
            if (myAudioStatus.bytesFilled + nDataLen > kAQBufSize) 
            {    
                err = MyEnqueueBuffer();
                audio_set_sample_first_play_time_cb(el_get_sys_time_ms());
                 //LOG(@"[MyEnqueueBuffer] err = %ld",err);
                WaitForFreeBuffer();
            }            
            //TODO: if nDataLen > kAQBufSize
            // ensure after waiting, if condition above is met
            if(myAudioStatus.bytesFilled + nDataLen > kAQBufSize)
            {
                EL_DBG_LOG("fatal err: audio render after wait,myAudioStatus.bytesFilled %ld + data len %ld exceeds buffer size %ld\n",myAudioStatus.bytesFilled,nDataLen,kAQBufSize);
                audio_consume_sample();
                usleep(SLEEP_TIME_INTERVAL);
                continue;
            }
            
            //EL_DBG_LOG("myAudioStatus.aqFailed = %d,myAudioStatus.aqStarted = %d\n",myAudioStatus.aqFailed,
            //      myAudioStatus.aqStarted );
            if (/*!myAudioStatus.aqFailed &&*/ myAudioStatus.aqStarted) 
            {
                // copy data to the audio queue buffer
                AudioQueueBufferRef fillBuf = myAudioStatus.audioQueueBuffer[myAudioStatus.fillBufferIndex];
                
                //EL_DBG_LOG("memcpy:%p, %d, %p, %d", fillBuf->mAudioData, (int)myAudioStatus.bytesFilled, pAudioData, (int)nDataLen);
                
                memcpy((char*)fillBuf->mAudioData + myAudioStatus.bytesFilled, (const char*)pAudioData, nDataLen);
                
                // keep track of bytes filled and packets filled
                myAudioStatus.bytesFilled += nDataLen;
                audio_consume_sample();
            }
            
            //yield
            usleep(SLEEP_TIME_INTERVAL);
        } //end while(true)
        

        //EL_DBG_LOG("sleep ...\n");
        usleep(GET_SAMPLE_TIME_INTERVAL);
        
        [pool drain];
    } // end while (!myAudioStatus.playStop)
    
    //pthread_mutex_unlock(&myAudioStatus.mutex_over);
    EL_DBG_LOG("audio play thread exit...\n");
}

#endif    
    
EL_STATIC ELAudioRenderFilter_t* el_player_get_audio_render_filter(EL_VOID) {
    ELAudioRenderFilter_t *pAudioRenderfilter = &audioRenderfilter;

	pAudioRenderfilter->filter_init        = el_audio_render_filter_init;
	pAudioRenderfilter->filter_unint       = el_audio_render_filter_uninit;
	pAudioRenderfilter->filter_set_param   = el_audio_render_filter_set_param;
	pAudioRenderfilter->filter_get_param   = el_audio_render_filter_get_param;
	pAudioRenderfilter->filter_set_volume  = el_audio_render_filter_set_volume;
	pAudioRenderfilter->filter_stop        = el_audio_render_filter_stop;
	pAudioRenderfilter->filter_pause       = el_audio_render_filter_pause;
	pAudioRenderfilter->filter_resume      = el_audio_render_filter_resume;
    pAudioRenderfilter->filter_reset       = el_audio_render_filter_reset;

    return pAudioRenderfilter;
}

