//
//  Audiblizer.h
//  OpenALTest
//
//  Created by Joshua Bodinet on 2/5/20.
//  Copyright © 2020 JoshuaBodinet. All rights reserved.
//

#ifndef Audiblizer_h
#define Audiblizer_h

#include <iostream>
#include <mutex>
#include <map>
#include <vector>
#include <iterator>
#include <thread>
#include <memory>
#include <OpenAL/al.h>
#include <OpenAL/alc.h>

#include "Event.h"

class Audiblizer
{
public:
    enum AudioFormat { AudioFormat_None = 0, AudioFormat_Mono8, AudioFormat_Mono16, AudioFormat_Stereo8, AudioFormat_Stereo16 };
    
    class BufferCompletionListener
    {
    public:
        BufferCompletionListener() {}
        virtual ~BufferCompletionListener() {}
        
        // Listener is responsible for freeing these buffers
        typedef std::vector<void*> BuffersCompletedVector;
        virtual void BuffersCompleted(const BuffersCompletedVector &buffersCompleted) = 0;
    };
    
    class AudioChunk
    {
    public:
        AudioChunk() : format(AudioFormat_None), sampleRate(0), buffer(nullptr), bufferSize(0) {}
        
        AudioFormat format;
        uint32_t    sampleRate;
        void       *buffer;
        size_t      bufferSize;
    };
    
    Audiblizer();
    ~Audiblizer();
    
    bool Initialize();
    void PrepareForDestruction();
    
    void SetBuffersCompletedListener(std::shared_ptr<BufferCompletionListener> listener);
    
    typedef std::vector<AudioChunk> AudioChunkVector;
    bool QueueAudio(const AudioChunkVector &audioChunks);
    uint32_t NumBuffersQueued();
    double   QueuedAudioDurationSeconds();
    
    bool Stop();
    
    static ALenum OpenALAudioFormat(AudioFormat audioFormat);
    static uint32_t AudioFormatFrameByteLength(AudioFormat audioFormat);
    static uint32_t AudioFormatFrameDatumLength(AudioFormat audioFormat);
    
private:
    std::mutex mutex;
    
    ALCdevice  *device;
    ALCcontext *context;
    ALuint      source;
    ALuint      *processedBuffers;
    ALuint      processBuffersCount;
    
    std::shared_ptr<BufferCompletionListener> bufferCompletionListener;
    
    class AudioBufferMapValue
    {
    public:
        AudioBufferMapValue() { audioBufferData = nullptr; audioBufferDurationMilliseconds = 0; }
        AudioBufferMapValue(void *data, uint64_t durationMS) { audioBufferData = data; audioBufferDurationMilliseconds = durationMS; }
        
        void *audioBufferData;
        uint64_t audioBufferDurationMilliseconds; // duration of this audio buffer
    };
    
    typedef ALuint AudioBufferMapKey; // Buffer ID
    typedef std::map<AudioBufferMapKey, AudioBufferMapValue> AudioBufferMap;
    typedef std::pair<AudioBufferMapKey, AudioBufferMapValue> AudioBufferMapPair;
    typedef AudioBufferMap::iterator AudioBufferMapIterator;
    typedef std::pair<AudioBufferMapIterator, bool> AudioBufferMapInsertionPair;
    
    AudioBufferMap audioBufferMap;
    uint64_t audioBufferMapDurationMilliseconds; // duration of all the audio contained in the audioBufferMap, as measured in milliseconds
    
    bool initialized;
    
    // --- Process unqueueable buffers THREAD ---
    std::thread *processUnqueueableBuffersThread;
    bool         processUnqueueableBuffersThreadRunning;
    Event        processUnqueueableBuffersThreadEvent;
    
    static void  ProcessUnqueueableBuffersThreadProc(Audiblizer *audiblizer);
    bool ProcessUnqueueableBuffers();
};

#endif /* Audiblizer_h */
