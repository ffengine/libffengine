//
//  ELPlayerViewController.h
//  ELPlayer
//
//  Created by Steven Jobs on 12-2-25.
//  Copyright (c) 2012年 __MyCompanyName__. All rights reserved.
//

#import <UIKit/UIKit.h>
#import "FFEngine/FFEngine.h"


@interface ELPlayerViewController : UIViewController <ELPlayerMessageProtocol>
{
    size_t _videoDuration;
    int _videoPostion;
}

////! 
@property (nonatomic, retain) IBOutlet UIView *protraitNavView;
@property (nonatomic, retain) IBOutlet UIView *protraitBottomView;

@property (nonatomic, retain) NSString *titleName;
@property (nonatomic, retain) IBOutlet UILabel *titleForP;

@property (nonatomic, retain) IBOutlet UILabel *pEstablishedTimeLabel;

@property (nonatomic, retain) IBOutlet UILabel *pLeftTimeLabel;

@property (nonatomic, retain) IBOutlet UISlider *progressPSlider;

@property (nonatomic, retain) NSString *videoUrl;

@property (nonatomic, retain) IBOutlet UIView *playerView;

@end
