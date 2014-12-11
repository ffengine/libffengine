//
//  MyParmeterClass.m
//  EL
//
//  Created by apple apple on 11-10-24.
//  Copyright 2011年 __MyCompanyName__. All rights reserved.
//

#import "ELMessageObject.h"


@implementation ELMessageObject

@synthesize message = _message;
@synthesize arg1 = _arg1;
@synthesize arg2 = _arg2;

-(id) initWithMessage: (int) message
                 arg1: (int)arg1
                 arg2: (int)arg2
{
    if (self = [super init])
    {
        self.message = message;
        self.arg1 = arg1;
        self.arg2 = arg2;
    }
    return self;
}

@end
