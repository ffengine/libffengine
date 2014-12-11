//
//  MyParmeterClass.h
//  EL
//
//  Created by apple apple on 11-10-24.
//  Copyright 2011年 __MyCompanyName__. All rights reserved.
//


//#ifndef _MY_PARMETER_CLASS_H_
//#define _MY_PARMETER_CLASS_H_

#import <Foundation/Foundation.h>

// 解码后，输出帧，是在子线程里面发消息，让主线程去更新UI
@interface ELMessageObject : NSObject 
{

}

@property (nonatomic, assign) int message;
@property (nonatomic, assign) int arg1;
@property (nonatomic, assign) int arg2;

-(id) initWithMessage:(int)parm1 arg1:(int)parm2 arg2:(int)parm3;

@end
