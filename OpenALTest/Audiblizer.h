// ****************************************************************************
// MIT License
//
// Copyright (c) 2019 Joshua E Bodinet
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
// ****************************************************************************
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

#include "HighPrecisionTimer.h"
#include "Event.h"

class Audiblizer : public HighPrecisionTimer::Delegate
{
public:
    enum AudioFormat { AudioFormat_None = 0, AudioFormat_Mono8, AudioFormat_Mono16, AudioFormat_Stereo8, AudioFormat_Stereo16 };
    
    class AudioChunkCompletionListener
    {
    public:
        AudioChunkCompletionListener() {}
        virtual ~AudioChunkCompletionListener() {}
        
        // Listener is responsible for freeing these buffers
        class AudioChunkProperties
        {
        public:
            AudioChunkProperties(void *bufferArg, double durationArg) { buffer = bufferArg; duration = durationArg; }
            void *buffer;
            double duration; // intended duration of this audio chunk in REAL seconds
        };
        
        typedef std::vector<AudioChunkProperties> AudioChunkCompletedVector;
        virtual void AudioChunkCompleted(const AudioChunkCompletedVector &audioChunksCompleted) = 0;
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
    
    void SetBuffersCompletedListener(std::shared_ptr<AudioChunkCompletionListener> listener);
    
    typedef std::vector<AudioChunk> AudioChunkVector;
    bool QueueAudio(const AudioChunkVector &audioChunks);
    uint32_t NumBuffersQueued();
    double   QueuedAudioDurationSeconds();
    
    bool Stop();
    
    // HighPrecisionTimer::Delegate Interface
    // ------------------------------------------------------------------
    virtual void TimerPing();
    virtual double TimerPeriod() { return 0.0001; }
    virtual bool FireOnce() { return false; }
    
    // Static Functions
    // ------------------------------------------------------------------
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
    
    std::shared_ptr<AudioChunkCompletionListener> audioChunkCompletionListener;
    
    class AudioBufferMapValue
    {
    public:
        AudioBufferMapValue() { audioBufferData = nullptr; audioBufferDurationMilliseconds = 0; }
        AudioBufferMapValue(void *data, uint64_t durationMS, double duration) { audioBufferData = data; audioBufferDurationMilliseconds = durationMS; audioBufferDurationSeconds = duration; }
        
        void *audioBufferData;
        uint64_t audioBufferDurationMilliseconds; // duration of this audio buffer in TRUNCATED milliseconds
        double   audioBufferDurationSeconds; // duration of this audio buffer in real secodns
    };
    
    typedef ALuint AudioBufferMapKey; // Buffer ID
    typedef std::map<AudioBufferMapKey, AudioBufferMapValue> AudioBufferMap;
    typedef std::pair<AudioBufferMapKey, AudioBufferMapValue> AudioBufferMapPair;
    typedef AudioBufferMap::iterator AudioBufferMapIterator;
    typedef std::pair<AudioBufferMapIterator, bool> AudioBufferMapInsertionPair;
    
    AudioBufferMap audioBufferMap;
    uint64_t audioBufferMapDurationMilliseconds; // duration of all the audio contained in the audioBufferMap, as measured in milliseconds
    
    bool initialized;
    
    // --- Process unqueueable buffers
    bool ProcessUnqueueableBuffers();
};

#endif /* Audiblizer_h */
