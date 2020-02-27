//
//  ViewControllerImagePickerSansCopy.h
//  OpenALTestiOS
//
//  Created by Joshua Bodinet on 2/26/20.
//  Copyright © 2020 JoshuaBodinet. All rights reserved.
//

#import <UIKit/UIKit.h>

NS_ASSUME_NONNULL_BEGIN

@interface ViewControllerImagePickerSansCopy : UIViewController <UICollectionViewDelegate, UICollectionViewDataSource>

@property (strong, nonatomic) IBOutlet UICollectionView *assetCollectionView;
@property (strong, nonatomic) IBOutlet UIButton *cancelButton;

@property (nonatomic)  NSInteger pickedFileIndex;
@property (strong, nonatomic) NSString *pickedFileAbsoluteURLString;

- (IBAction)cancelButtonHit:(UIButton *)sender;

@end

NS_ASSUME_NONNULL_END
