//
//  commonDef.h
//  ELMediaLib_New
//
//  Created by Wei Xie on 12-6-25.
//  Copyright (c) 2012年 __MyCompanyName__. All rights reserved.
//

#ifndef ELMediaLib_New_commonDef_h
#define ELMediaLib_New_commonDef_h

#define R   0
#define G   1
#define B   2

#ifdef APPSTORE_AD_VERSION_CN
#define DEFAULT_VIEW_FRM            CGRectMake(0, 0, 480, 320)
#else
#define DEFAULT_VIEW_FRM            CGRectMake(0, 20, 320, 260)
#endif
#define kAccelerometerFrequency		60.0 // Hz
#define kAccelerationThreshold      2.0
#define HAVADATA_TIME               50

#endif
