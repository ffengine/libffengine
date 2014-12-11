//
//  ELShareContainer.h
//  ELMediaLib_New
//
//  Created by Wei Xie on 12-6-25.
//  Copyright (c) 2012年 __MyCompanyName__. All rights reserved.
//

#import <Foundation/Foundation.h>

@interface ELShareContainer : NSObject

@property (nonatomic, readwrite) BOOL isRegisterOK;

+(ELShareContainer *) defaultShareConatiner;
+(void) destoryDefaultShareContainer;

@end
