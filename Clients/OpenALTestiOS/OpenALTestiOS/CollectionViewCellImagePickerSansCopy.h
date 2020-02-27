//
//  CollectionViewCellImagePickerSansCopy.h
//  OpenALTestiOS
//
//  Created by Joshua Bodinet on 2/26/20.
//  Copyright © 2020 JoshuaBodinet. All rights reserved.
//

#import <UIKit/UIKit.h>

NS_ASSUME_NONNULL_BEGIN

@interface CollectionViewCellImagePickerSansCopy : UICollectionViewCell

@property (strong, nonatomic) IBOutlet UIImageView *imageView;

@property (nonatomic)  NSInteger pickedFileIndex;
@property (strong, nonatomic) NSString *pickedFileAbsoluteURLString;

@end

NS_ASSUME_NONNULL_END
