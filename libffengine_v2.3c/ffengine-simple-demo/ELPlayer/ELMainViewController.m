//
//  ELMainViewController.m
//  ELPlayer
//
//  Created by Steven Jobs on 12-2-25.
//  Copyright (c) 2012年 __MyCompanyName__. All rights reserved.
//

#import "ELMainViewController.h"
#import "ELUtils.h"
#import "ELFileListViewController.h"
#import "InputVideoUrlViewController.h"

@implementation ELMainViewController

@synthesize functionArray;
@synthesize functionLabelTextDictionary;

-(void)dealloc
{
    self.functionArray = nil;
    self.functionLabelTextDictionary = nil;

    [super dealloc];
}

- (void)didReceiveMemoryWarning
{
    // Releases the view if it doesn't have a superview.
    [super didReceiveMemoryWarning];
    
    // Release any cached data, images, etc that aren't in use.
}

#pragma mark - View lifecycle

- (void)viewDidLoad
{
    [super viewDidLoad];

    self.title = @"Demo(xiewei.max@gmail.com)";
    
    self.functionArray = [NSMutableArray arrayWithObjects: FUNC_FILELIST, FUNC_INPUTURL, nil];
    
    self.functionLabelTextDictionary = [NSMutableDictionary dictionaryWithObjectsAndKeys: 
                                        FUNC_FILELIST, FUNC_FILELIST,
                                        FUNC_INPUTURL, FUNC_INPUTURL,
                                        nil];

}

- (BOOL)shouldAutorotate
{
    return NO;
}

- (BOOL)shouldAutorotateToInterfaceOrientation:(UIInterfaceOrientation)interfaceOrientation
{
    // Return YES for supported orientations
    return (interfaceOrientation == UIInterfaceOrientationPortrait);
}

-(NSString *) getFunctionKey: (NSInteger) functionRow
{
    return [self.functionArray objectAtIndex: functionRow];
}

-(NSString *) getFunctionLabel: (NSInteger) functionRow
{
    NSString *functionKey = [self getFunctionKey: functionRow];

    return [self.functionLabelTextDictionary objectForKey: functionKey];
}

#pragma mark - Table view data source

- (NSInteger)numberOfSectionsInTableView:(UITableView *)tableView
{
    return 1;
}

- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section
{
    return [self.functionArray count];
}

- (UITableViewCell *)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath
{
    static NSString *CellIdentifier = @"Cell";

    UITableViewCell *cell = [tableView dequeueReusableCellWithIdentifier:CellIdentifier];
    if (cell == nil) 
    {
        cell = [[[UITableViewCell alloc] initWithStyle:UITableViewCellStyleDefault reuseIdentifier:CellIdentifier] autorelease];
    }

    cell.textLabel.text = [self getFunctionLabel: indexPath.row];

    return cell;
}

#pragma mark - Table view delegate

- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath
{
    NSString *functionKey = [self.functionArray objectAtIndex: indexPath.row];
    
    if ([functionKey isEqualToString: FUNC_FILELIST])
    {
        ELFileListViewController *fileListViewController = [[[ELFileListViewController alloc] init] autorelease];
        fileListViewController.title = [self getFunctionLabel: indexPath.row];

        [self.navigationController pushViewController: fileListViewController animated: YES];
    }
    else if([functionKey isEqualToString: FUNC_INPUTURL])
    {
        InputVideoUrlViewController *inputVideoUrlViewController = [[[InputVideoUrlViewController alloc] initWithNibName:@"InputVideoUrlViewController" bundle:nil] autorelease];
        inputVideoUrlViewController.title = [self getFunctionLabel: indexPath.row];

        [self.navigationController pushViewController: inputVideoUrlViewController animated: YES];
    }
}

@end
