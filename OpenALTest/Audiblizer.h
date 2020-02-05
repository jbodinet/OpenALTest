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
#include <OpenAL/al.h>
#include <OpenAL/alc.h>

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
    
    bool Initialize(BufferCompletionListener *listener = nullptr);
    
    typedef std::vector<AudioChunk> AudioChunkVector;
    bool QueueAudio(const AudioChunkVector &audioChunks);
    
    bool Stop();
    
private:
    std::mutex mutex;
    
    ALCdevice  *device;
    ALCcontext *context;
    ALuint      source;
    ALuint      *processedBuffers;
    ALuint      processBuffersCount;
    
    BufferCompletionListener *bufferCompletionListener;
    
    typedef ALuint AudioBufferMapKey; // Buffer ID
    typedef void* AudioBufferMapValue; // Buffer Data
    typedef std::map<AudioBufferMapKey, AudioBufferMapValue> AudioBufferMap;
    typedef std::pair<AudioBufferMapKey, AudioBufferMapValue> AudioBufferMapPair;
    typedef AudioBufferMap::iterator AudioBufferMapIterator;
    typedef std::pair<AudioBufferMapIterator, bool> AudioBufferMapInsertionPair;
    
    AudioBufferMap audioBufferMap;
    
    bool initialized;
    
    static ALenum OpenALAudioFormat(AudioFormat audioFormat);
    
    // todo - ProcessUnqueueableBuffers should run in a private thread that is owned by this Audiblizer!!!
    bool ProcessUnqueueableBuffers();
};

#endif /* Audiblizer_h */
