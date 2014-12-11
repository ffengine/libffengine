//
//  ELMainViewController.h
//  ELPlayer
//
//  Created by Steven Jobs on 12-2-25.
//  Copyright (c) 2012年 __MyCompanyName__. All rights reserved.
//

#import <UIKit/UIKit.h>

#define FUNC_FILELIST @"Local iTunes files" // @"本地文件列表"
#define FUNC_INPUTURL @"input url" //  @"输入URL"

@interface ELMainViewController : UITableViewController

@property (nonatomic, retain) NSMutableArray *functionArray;
@property (nonatomic, retain) NSMutableDictionary *functionLabelTextDictionary;

@end
