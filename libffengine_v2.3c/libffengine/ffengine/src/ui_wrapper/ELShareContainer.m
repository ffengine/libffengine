//
//  ELShareContainer.m
//  ELMediaLib_New
//
//  Created by Wei Xie on 12-6-25.
//  Copyright (c) 2012年 __MyCompanyName__. All rights reserved.
//

#import "ELShareContainer.h"


@implementation ELShareContainer

@synthesize isRegisterOK;

static ELShareContainer *__shareContainer_instance;

+(ELShareContainer *) defaultShareConatiner
{
    @synchronized( [ELShareContainer class] )
    {
        if( !__shareContainer_instance )
        {
            __shareContainer_instance = [[ELShareContainer alloc] init];
        }
        
        return __shareContainer_instance;   
    }
}

+(void) destoryDefaultShareContainer
{
    @synchronized([ELShareContainer class] )
    {
        [__shareContainer_instance release];
        __shareContainer_instance = nil;        
    }
}

@end
