//
//  ELFileListViewController.m
//  ELPlayer
//
//  Created by Steven Jobs on 12-2-25.
//  Copyright (c) 2012年 __MyCompanyName__. All rights reserved.
//

#import "ELFileListViewController.h"
#import "ELUtils.h"
#import "ELGlobal.h"
#import "ELPlayerViewController.h"

@implementation ELFileListViewController

@synthesize fileListArray = _fileListArray;

-(void)dealloc
{
    self.fileListArray = nil;

    [super dealloc];
}

#pragma mark - View lifecycle

- (void)viewDidLoad
{
//    NSArray *extensionArray = [NSArray arrayWithObjects: SUPPORTED_FILE_EXTENSIONS, nil];
    NSFileManager *fileManager = [[[NSFileManager alloc] init] autorelease];
    
    // 1. 获取文件列表
//    NSArray *levelList = [[fileManager contentsOfDirectoryAtPath: [ELUtils documentsPath] error:nil] pathsMatchingExtensions: extensionArray];// 这里只是Documents目录的

    NSArray *levelList = [fileManager contentsOfDirectoryAtPath: [ELUtils documentsPath] error:nil];// 这里只是Documents目录的

    
    self.fileListArray = [NSMutableArray arrayWithArray: levelList];

    [super viewDidLoad];
}

#pragma mark - Table view data source

- (NSInteger)numberOfSectionsInTableView:(UITableView *)tableView
{
    return 1;
}

- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section
{
    return [self.fileListArray count];
}

- (UITableViewCell *)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath
{
    static NSString *CellIdentifier = @"Cell";
    
    UITableViewCell *cell = [tableView dequeueReusableCellWithIdentifier:CellIdentifier];
    if (cell == nil) 
    {
        cell = [[[UITableViewCell alloc] initWithStyle:UITableViewCellStyleDefault reuseIdentifier:CellIdentifier] autorelease];
    }
    
    cell.textLabel.text = [self.fileListArray objectAtIndex: indexPath.row];
    
    return cell;
}

#pragma mark - Table view delegate

- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath
{
    ELPlayerViewController *playerViewController = [[[ELPlayerViewController alloc] initWithNibName: @"ELPlayerViewController" bundle: nil] autorelease];
    
    NSString *fileName = [self.fileListArray objectAtIndex: indexPath.row];
    
    playerViewController.titleName = fileName;
    fileName = [[ELUtils documentsPath] stringByAppendingPathComponent: fileName];
    
    NSLog(@"Playing: %@", fileName);
    playerViewController.videoUrl = fileName;

    [self.navigationController pushViewController: playerViewController animated:YES];
}

@end
