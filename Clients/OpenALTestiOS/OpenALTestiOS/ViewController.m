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

#import "ViewController.h"
#import "ViewControllerImagePickerSansCopy.h"

#include "AudiblizerTestHarnessApple.h"

@interface ViewController () {
    std::shared_ptr<AudiblizerTestHarnessApple> audiblizer;
}

@end

@implementation ViewController

- (void)viewDidLoad {
    [super viewDidLoad];
    
    self.viewControllerHasMadeFirstAppearance = NO;
    
    audiblizer = NULL;
}

-(void)viewDidAppear:(BOOL)animated {
    
    if(!self.viewControllerHasMadeFirstAppearance)
    {
        // Init OpenAL layer
        audiblizer = std::make_shared<AudiblizerTestHarnessApple>();
        if(audiblizer != nullptr)
        {
            if(!audiblizer->Initialize())
            {
                NSLog(@"AudiblizerTestHarness failed to Initialize!!!\n");
            }
        }
        else
        {
            NSLog(@"Could NOT create AudiblizerTestHarness!!!\n");
        }
        
        // force request to access photos library
        [PHPhotoLibrary requestAuthorization:^(PHAuthorizationStatus status) {
            if(status != PHAuthorizationStatusAuthorized)
            {
                NSLog(@"*** Photo Library Access Authorization Not Granted !!! ***");
            }
            else
            {
                
            }
        }];
        
        self.viewControllerHasMadeFirstAppearance = YES;
    }
}

#pragma mark - Button Handlers
- (IBAction)hitPickMovieButton:(UIButton *)sender {
   // [self pickMovie];
    
    NSString *path = [NSString stringWithFormat:@"%@04TwistingByThePool.m4a", NSTemporaryDirectory()];
    NSURL    *url  = [NSURL fileURLWithPath:path];
    if([self loadMovieIntoAudiblizerTestHarness:url])
    {
        AudiblizerTestHarness::VideoSegments videoSegments;
        AudiblizerTestHarness::VideoParameters videoParameters;
        
        videoParameters.sampleDuration = 1001;
        videoParameters.timeScale = 30000;
        videoParameters.numVideoFrames = 30 * 30;
        videoSegments.push_back(videoParameters);
        
        videoParameters.sampleDuration = 1001;
        videoParameters.timeScale = 60000;
        videoParameters.numVideoFrames = 60 * 30;
        videoSegments.push_back(videoParameters);
        
        // potentially adversarial testing params
        double audioPlayrateFactor = 1.0;
        uint32_t audioChunkCacheSize = 1;
        uint32_t numPressureThreads = 0;
        
        audiblizer->StartTest(videoSegments, audioPlayrateFactor, audioChunkCacheSize, numPressureThreads);
    }
}

#pragma mark - Player Utils
-(void) pickMovie {
    dispatch_async(dispatch_get_main_queue(), ^{
        if([PHPhotoLibrary authorizationStatus] != PHAuthorizationStatusAuthorized)
            return;
        
        const BOOL useNativeImagePicker = YES;
        if(useNativeImagePicker)
        {
            // pop open the photo library and show videos
            UIImagePickerController *picker = [[UIImagePickerController alloc] init];
            picker.delegate = self;
            picker.sourceType = UIImagePickerControllerSourceTypePhotoLibrary;
            picker.mediaTypes = [[NSArray alloc] initWithObjects: (NSString *) kUTTypeMovie, nil];
            picker.allowsEditing = NO;
            picker.videoQuality = UIImagePickerControllerQualityTypeHigh;
            [self presentViewController:picker animated:YES completion:nil];
        }
        else
        {
            [self performSegueWithIdentifier:@"showImagePickerSansCopy" sender:self];
        }
    });
}

-(BOOL) loadMovieIntoAudiblizerTestHarness:(NSURL*)movieURL {
    BOOL retVal = YES;
    
    if(audiblizer != nullptr)
    {
        uint32_t audioSampleRate = 48000;
        
        if(!audiblizer->LoadAudio([[movieURL path] cStringUsingEncoding:NSUTF8StringEncoding], audioSampleRate))
        {
            NSLog(@"Audiblizer failed to load audio from file at path:%@", [movieURL path]);
            
            retVal = NO;
            goto Exit;
        }
    }
Exit:
    return retVal;
}

#pragma mark - UIImagePickerControllerDelegate
- (void)imagePickerController:(UIImagePickerController *)picker didFinishPickingMediaWithInfo:(NSDictionary *)info {
    
    if([(NSString*)info[UIImagePickerControllerMediaType] isEqualToString:(NSString*)kUTTypeVideo] ||
       [(NSString*)info[UIImagePickerControllerMediaType] isEqualToString:(NSString*)kUTTypeMovie])
    {
        NSURL *mediaURL = info[UIImagePickerControllerMediaURL];
        
        // ditch all other videos except the one just picked
        // as iOS always copies picked video into app data
        [self clearMovieFilesFromTmpDirSparingURL:mediaURL];
        
        // load the video
        [self loadMovieIntoAudiblizerTestHarness:mediaURL];
    }
    
    [picker dismissViewControllerAnimated:YES completion:nil];
}

- (void)imagePickerControllerDidCancel:(UIImagePickerController *)picker {
    
    [picker dismissViewControllerAnimated:YES completion:nil];
}

#pragma mark - Segue
- (void)prepareForSegue:(UIStoryboardSegue *)segue sender:(id)sender {
    if ([segue.identifier isEqualToString:@"showImagePickerSansCopy"])
    {
        // proper handling of sender object
        // -----------------------------------------------------
        if(sender != self)
        {
            return;
        }
    }
}

- (IBAction)showImagePickerSansCopyUnwind:(UIStoryboardSegue*)unwindSegue {
    NSLog(@"showImagePickerSansCopyUnwind!!!");
    
    if([unwindSegue.sourceViewController isKindOfClass:[ViewControllerImagePickerSansCopy class]])
    {
        ViewControllerImagePickerSansCopy* imagePickerSansCopy = (ViewControllerImagePickerSansCopy*)unwindSegue.sourceViewController;
        
        NSInteger pickedFileIndex = imagePickerSansCopy.pickedFileIndex;
        if(pickedFileIndex >= 0)
        {
            NSString *pickedFileAbsoluteURLString = [NSString stringWithString:imagePickerSansCopy.pickedFileAbsoluteURLString];
            
            NSLog(@"Picked File Idx:%d URL:%@", (int)pickedFileIndex, pickedFileAbsoluteURLString);
            
            // acquire the PHFetchResults and use the pickedFile* info to test for validity and then load the file
            PHFetchResult *results = [PHAsset fetchAssetsWithMediaType:PHAssetMediaTypeVideo options:nil];
            PHAsset *asset = [results objectAtIndex:pickedFileIndex];
            
            dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
            PHVideoRequestOptions *options = [PHVideoRequestOptions new];
            __block AVAsset *resultAsset = nil;
            [[PHImageManager defaultManager] requestAVAssetForVideo:asset options:options resultHandler:^(AVAsset * _Nullable avAsset, AVAudioMix * _Nullable audioMix, NSDictionary * _Nullable info) {
                resultAsset = avAsset;
                dispatch_semaphore_signal(semaphore);
            }];
            
            dispatch_semaphore_wait(semaphore, DISPATCH_TIME_FOREVER);
            
            // only load if the asset is of type AVURLAsset
            if([resultAsset isKindOfClass:[AVURLAsset class]])
            {
                AVURLAsset *urlAsset = (AVURLAsset*)resultAsset;
                
                // ensure that the URL of the urlAsset matches that of the pickedFile, and if so, then
                // load the pickedFile into the player
                if(NSOrderedSame == [[urlAsset.URL absoluteString] compare:pickedFileAbsoluteURLString])
                {
                    [self loadMovieIntoAudiblizerTestHarness:[NSURL URLWithString:pickedFileAbsoluteURLString]];
                }
            }
        }
        else
        {
            // TODO: Handle case where nothing was picked
        }
    }
}

#pragma mark - Utilities
-(void) clearMovieFilesFromTmpDirSparingURL:(NSURL*)exceptThisURL {
    // Create a local file manager instance and grab the URL of the app temp directory
    NSFileManager *localFileManager = [[NSFileManager alloc] init];
    NSURL *directoryToScan = [NSURL fileURLWithPath:NSTemporaryDirectory()];
    
    // create an enumerator
    NSDirectoryEnumerator *dirEnumerator =
    [localFileManager   enumeratorAtURL:directoryToScan
             includingPropertiesForKeys:[NSArray arrayWithObjects:NSURLIsDirectoryKey,nil]
                                options: NSDirectoryEnumerationSkipsHiddenFiles |
     NSDirectoryEnumerationSkipsSubdirectoryDescendants |
     NSDirectoryEnumerationSkipsPackageDescendants
                           errorHandler:nil];
    
    // walk the enumerator, ditching any mp4 files
    NSError *error;
    for (NSURL *theURL in dirEnumerator)
    {
        if(exceptThisURL != nil && [[theURL absoluteString] isEqualToString:[exceptThisURL absoluteString]])
        {
            continue;
        }
        
        // if url is for an .mp4 file, delete the file
        NSString *extension = [theURL pathExtension];
        if([[extension lowercaseString] isEqualToString:@"mp4"] ||
           [[extension lowercaseString] isEqualToString:@"mov"])
        {
            NSLog(@"AppCloseFileDelete:%@", [theURL path]);
            [localFileManager removeItemAtURL:theURL error:&error];
        }
    }
}

@end
