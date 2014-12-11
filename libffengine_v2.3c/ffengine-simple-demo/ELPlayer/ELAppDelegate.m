//
//  ELAppDelegate.m
//  ELPlayer
//
//  Created by Steven Jobs on 12-2-25.
//  Copyright (c) 2012年 __MyCompanyName__. All rights reserved.
//

#import <AVFoundation/AVFoundation.h>

#import "ELAppDelegate.h"

#import "ELMainViewController.h"
#import "ELUtils.h"
#import <FFEngine/FFEngine.h>

@implementation ELAppDelegate

@synthesize window = _window;
@synthesize mainViewController = _mainViewController;

- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions
{
    self.window = [[[UIWindow alloc] initWithFrame:[[UIScreen mainScreen] bounds]] autorelease];

    NSLog(@"Bundle Path = %@", [NSBundle mainBundle].bundlePath);
    NSLog(@"Bundle ID = %@", [[NSBundle mainBundle] bundleIdentifier]);

    ELMainViewController *mainViewController = [[[ELMainViewController alloc] initWithNibName:@"ELMainViewController" bundle:nil] autorelease];
    UINavigationController *navigationController = [[[UINavigationController alloc] initWithRootViewController: mainViewController] autorelease];

    self.window.rootViewController = navigationController;

    [self.window makeKeyAndVisible];

    // FFEngine: register engine!
    registerLib("ffsdk.com");

    return YES;
}

@end
