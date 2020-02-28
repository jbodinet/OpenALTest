//
//  ViewController.h
//  OpenALTestiOS
//
//  Created by Joshua Bodinet on 2/26/20.
//  Copyright © 2020 JoshuaBodinet. All rights reserved.
//

#import <UIKit/UIKit.h>
#import <Photos/Photos.h>
#import <MobileCoreServices/MobileCoreServices.h>

@interface ViewController : UIViewController <UIImagePickerControllerDelegate, UINavigationControllerDelegate>

@property (strong, nonatomic) IBOutlet UITextView *textView;
@property (strong, nonatomic) IBOutlet UIButton *pickMovieButton;

@property (nonatomic) BOOL viewControllerHasMadeFirstAppearance;

-(void) pickMovie;
-(BOOL) loadMovieIntoAudiblizerTestHarness:(NSURL*)movieURL;

- (IBAction)hitPickMovieButton:(UIButton *)sender;
- (IBAction)showImagePickerSansCopyUnwind:(UIStoryboardSegue*)unwindSegue;

-(void) clearMovieFilesFromTmpDirSparingURL:(NSURL*)exceptThisURL;


@end

