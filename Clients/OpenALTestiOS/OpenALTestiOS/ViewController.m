//
//  ViewController.m
//  OpenALTestiOS
//
//  Created by Joshua Bodinet on 2/26/20.
//  Copyright © 2020 JoshuaBodinet. All rights reserved.
//

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
    [self pickMovie];
}

#pragma mark - Player Utils
-(void) pickMovie {
    dispatch_async(dispatch_get_main_queue(), ^{
        if([PHPhotoLibrary authorizationStatus] != PHAuthorizationStatusAuthorized)
            return;
        
        [self performSegueWithIdentifier:@"showImagePickerSansCopy" sender:self];
    });
}

-(void) loadMovieIntoAudiblizerTestHarness:(NSURL*)movieURL {
    if(audiblizer != nullptr)
    {
        uint32_t audioSampleRate = 48000;
        
        if(!audiblizer->LoadAudio([[movieURL path] cStringUsingEncoding:NSUTF8StringEncoding] , audioSampleRate))
        {
            NSLog(@"Audiblizer failed to load audio from file at path:%@", [movieURL path]);
        }
    }
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


@end
