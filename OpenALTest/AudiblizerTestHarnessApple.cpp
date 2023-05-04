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

#include "AudiblizerTestHarnessApple.h"

#include <AudioToolbox/AudioToolbox.h>

bool AudiblizerTestHarnessApple::Load16bitStereoPCMAudioFromFile(const char *filePath, uint32_t sampleRate)
{
    if(!initialized)
    {
        return false;
    }
    
    OSStatus status = noErr;
    ExtAudioFileRef audioFileRef = nullptr;
    CFStringRef audioFilePath = nullptr;
    CFURLRef audioFileURL = nullptr;
    UInt32 numDstFrames = 0;
    SInt64 sourceFileLengthInFrames = 0;
    UInt32 sourceFileLengthInFramesSize = sizeof(sourceFileLengthInFrames);
    
    AudioConverterRef audioConverterRef = nullptr;
    UInt32 audioConverterRefSize = sizeof(AudioConverterRef);
    
    AudioStreamBasicDescription sourceFormat = {0};
    UInt32 sourceFormatSize = sizeof(AudioStreamBasicDescription);
    
    AudioChannelLayout sourceChannelLayout = {0};
    UInt32 sourceChannelLayoutSize = sizeof(AudioChannelLayout);
    
    // set up the AudioStreamBasicDescription of the decode format
    AudioStreamBasicDescription decodeFormat = {0};
    UInt32 decodeFormatSize = sizeof(AudioStreamBasicDescription);
    
    // ditch any existing audio data
    // --------------------------------------------
    FreeAudioSample(audioData);
    audioData = nullptr;
    audioDataPtr = nullptr;
    audioDataSize = 0;
    audioDataTotalNumDatums = 0;
    audioDataTotalNumFrames = 0;
    audioSampleRate = 0;
    audioIsStereo = false;
    audioIsSilence = false;
    audioDurationSeconds = 0.0;
    
    // Configure the AudioToolbox/CoreAudio layer
    // --------------------------------------------
    
    // prepare the decode format
    decodeFormat.mFormatID = kAudioFormatLinearPCM;
    decodeFormat.mChannelsPerFrame = 2;
    decodeFormat.mSampleRate = sampleRate;
    decodeFormat.mFormatFlags = kAudioFormatFlagIsSignedInteger | kAudioFormatFlagsNativeEndian | kAudioFormatFlagIsPacked;
    decodeFormat.mFramesPerPacket = 1; // uncompressed is always 1 packet per frame (is this true?)
    decodeFormat.mBitsPerChannel = 16; // do we want this to be 16bit audio?
    decodeFormat.mBytesPerFrame = 2 * decodeFormat.mChannelsPerFrame; // if 16bit audio, then '2 * sourceFormat.mChannelsPerFrame'
    decodeFormat.mBytesPerPacket = decodeFormat.mBytesPerFrame * decodeFormat.mFramesPerPacket;
    
    // set up the AudioChannelLayout
    AudioChannelLayout decodeChannelLayout = {0};
    UInt32 decodeChannelLayoutSize = sizeof(decodeChannelLayout);
    
    decodeChannelLayout.mChannelLayoutTag = kAudioChannelLayoutTag_Stereo; // kAudioChannelLayoutTag_StereoHeadphones
    decodeChannelLayout.mNumberChannelDescriptions = 1;
    
    // create the output buffer
    AudioBufferList audioBufferList = {0};
    
    audioFilePath = CFStringCreateWithCString(kCFAllocatorDefault, filePath, kCFStringEncodingUTF8);
    if(audioFilePath == nullptr)
    {
        status = -50; // param error
        printf("ERROR -- CFStringCreateWithCString!!!\n");
        goto Exit;
    }
    
    audioFileURL = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, audioFilePath, kCFURLPOSIXPathStyle, false);
    if(audioFileURL == nullptr)
    {
        status = -50; // param error
        printf("ERROR -- CFURLCreateWithFileSystemPath!!!\n");
        goto Exit;
    }
    
    status = ExtAudioFileOpenURL(audioFileURL, &audioFileRef);
    if(status != noErr)
    {
        printf("ERROR -- ExtAudioFileOpenURL!!!\n");
        goto Exit;
    }
    
    // GET Source (aka 'File') Properties
    // --------------------------------------------------------------------------------------------------------------------
    status = ExtAudioFileGetProperty(audioFileRef, kExtAudioFileProperty_FileDataFormat, &sourceFormatSize, &sourceFormat);
    if(status != noErr)
    {
        printf("ERROR -- ExtAudioFileGetProperty FileDataFormat!!!\n");
        goto Exit;
    }
    
    status = ExtAudioFileGetProperty(audioFileRef, kExtAudioFileProperty_FileChannelLayout, &sourceChannelLayoutSize, &sourceChannelLayout);
    if(status != noErr)
    {
        printf("ERROR -- ExtAudioFileGetProperty  FileChannelLayout!!!\n");
        goto Exit;
    }
    
    // Regardless to whether we call this BEFORE OR AFTER we set the ClientDataFormat and the ClientChannelLayout, this
    // call seems to get the file length in frames WITH RESPECT TO THE SOURCE/FILE SAMPLE RATE (the sample rate found by calling
    // ExtAudioFileGetProperty via kExtAudioFileProperty_FileDataFormat) as opposed to the DST/CLIENT SAMPLE RATE (the sample rate
    // supplied to ExtAudioFileSetProperty via kExtAudioFileProperty_ClientDataFormat)
    status = ExtAudioFileGetProperty(audioFileRef, kExtAudioFileProperty_FileLengthFrames, &sourceFileLengthInFramesSize, &sourceFileLengthInFrames);
    if(status != noErr)
    {
        printf("ERROR -- ExtAudioFileGetProperty FileLengthFrames!!!\n");
        goto Exit;
    }
    
    // GET Dst (aka 'Client') Properties
    // --------------------------------------------------------------------------------------------------------------------
    status = ExtAudioFileSetProperty(audioFileRef, kExtAudioFileProperty_ClientDataFormat, decodeFormatSize, &decodeFormat);
    if(status != noErr)
    {
        printf("ERROR -- ExtAudioFileSetProperty ClientDataFormat!!!\n");
        goto Exit;
    }
    
    status = ExtAudioFileSetProperty(audioFileRef, kExtAudioFileProperty_ClientChannelLayout, decodeChannelLayoutSize, &decodeChannelLayout);
    if(status != noErr)
    {
        printf("ERROR -- ExtAudioFileSetProperty ClientDataFormat!!!\n");
        goto Exit;
    }
    
    status = ExtAudioFileGetProperty(audioFileRef, kExtAudioFileProperty_AudioConverter, &audioConverterRefSize, &audioConverterRef);
    if(status != noErr)
    {
        printf("ERROR -- ExtAudioFileGetProperty AudioConverter!!!\n");
        goto Exit;
    }
    
    // Determine the length of the audio in DST sample rate, then create the audio buffer big enough to hold it
    // --------------------------------------------------------------------------------------------------------------------
    numDstFrames = (UInt32) (sourceFileLengthInFrames * (decodeFormat.mSampleRate / (double) sourceFormat.mSampleRate));
    audioBufferList.mNumberBuffers = 1;
    audioBufferList.mBuffers[0].mNumberChannels = 2;
    audioBufferList.mBuffers[0].mDataByteSize = numDstFrames * decodeFormat.mBytesPerFrame;
    audioBufferList.mBuffers[0].mData = malloc(audioBufferList.mBuffers[0].mDataByteSize);
    
    // Read in the decoded audio
    // NOTE: This call can change the value of numDstFrames. Thus, if you want to reuse the buffer held in
    //       audioBufferList.mBuffers[0].mData, then you might want to store its original size (in frames) somewhere else
    //       besides numDstFrames so that you can revisit this value after the call to ExtAudioFileRead()
    // --------------------------------------------------------------------------------------------------------------------
    status = ExtAudioFileRead(audioFileRef, &numDstFrames, &audioBufferList);
    if(status != noErr)
    {
        printf("ERROR -- ExtAudioFileRead!!!\n");
        goto Exit;
    }
    
    // load up the AudiblizerTestHarness properties w/ successful values
    // --------------------------------------------------------------------------------------------------------------------
    audioData = (uint8_t*) audioBufferList.mBuffers[0].mData; // we are here GIVING the audio to the base class pointer!!!
    audioDataPtr = nullptr;
    audioDataSize = audioBufferList.mBuffers[0].mDataByteSize;
    audioDataTotalNumDatums = audioDataSize / sizeof(uint16_t);
    audioDataTotalNumFrames = numDstFrames;
    audioSampleRate = sampleRate;
    audioIsStereo = true;
    audioIsSilence = false;
    audioDurationSeconds = numDstFrames / (double) decodeFormat.mSampleRate;
    
Exit:
    if(audioFilePath != nullptr)
    {
        CFRelease(audioFilePath);
        audioFilePath = nullptr;
    }
    
    if(audioFileURL != nullptr)
    {
        CFRelease(audioFileURL);
        audioFileURL = nullptr;
    }
    
    if(audioFileRef != nullptr)
    {
        ExtAudioFileDispose(audioFileRef);
        audioFileRef = nullptr;
    }
    
    if(status != noErr)
    {
        FreeAudioSample(audioData);
        audioData = nullptr;
        audioDataPtr = nullptr;
        audioDataSize = 0;
        audioDataTotalNumDatums = 0;
        audioDataTotalNumFrames = 0;
        audioSampleRate = 0;
        audioIsStereo = false;
        audioIsSilence = false;
        audioDurationSeconds = 0.0;
    }
    
    return status == noErr ? true : false;
}
