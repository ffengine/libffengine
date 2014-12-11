//
//  InputVideoUrlViewController.m
//  ELPlayer
//
//  Created by Steven Jobs on 12-2-25.
//  Copyright (c) 2012年 __MyCompanyName__. All rights reserved.
//

#import "InputVideoUrlViewController.h"
#import "ELPlayerViewController.h"

@implementation InputVideoUrlViewController
@synthesize searchBar = _searchBar;
@synthesize urlHistory = _urlHistory;

#pragma mark - View lifecycle

- (void)viewDidLoad
{
    [super viewDidLoad];

    NSLog(@"self.searchBar = %@", self.searchBar);
    
    self.tableView.tableHeaderView = self.searchBar;

    self.urlHistory = [NSMutableArray arrayWithCapacity: 20];

    [_urlHistory addObject: @"rtsp://mafreebox.freebox.fr/fbxtv_pub/stream?namespace=1&service=372"];
    
    [_urlHistory addObject: @"rtmp://rtmp.sctv.com/SRT_Live/KBTV_800"];
    [_urlHistory addObject: @"rtmp://media.csjmpd.com/live/live1"];

    [_urlHistory addObject: @"http://192.168.1.100/mt.avi"];

    [_urlHistory addObject: @"http://v1.cztv.com/channels/101/500.flv"];    // 可以播放
    [_urlHistory addObject: @"http://61.55.169.19:8080/live/38"];
}

#pragma mark - Table view data source

- (NSInteger)numberOfSectionsInTableView:(UITableView *)tableView
{
    return 1;
}

- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section
{
    return [self.urlHistory count];
}

- (UITableViewCell *)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath
{
    static NSString *CellIdentifier = @"Cell";
    
    UITableViewCell *cell = [tableView dequeueReusableCellWithIdentifier:CellIdentifier];
    if (cell == nil) {
        cell = [[[UITableViewCell alloc] initWithStyle:UITableViewCellStyleDefault reuseIdentifier:CellIdentifier] autorelease];
    }
    
    cell.textLabel.text = [self.urlHistory objectAtIndex: indexPath.row];

    return cell;
}

#pragma mark - Table view delegate

- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath
{
    self.searchBar.text = [self.urlHistory objectAtIndex: indexPath.row];
    
    [self.searchBar becomeFirstResponder];
}

#pragma mark - searchbar delegate

- (void)searchBarCancelButtonClicked:(UISearchBar *) searchBar
{
    [searchBar resignFirstResponder];
}

- (void)searchBarSearchButtonClicked:(UISearchBar *)searchBar
{
    [searchBar resignFirstResponder];
    
    if (0 >= [searchBar.text length]) 
    {
        return;
    }

    if (![self.urlHistory containsObject: searchBar.text]) 
    {
        [self.urlHistory addObject: searchBar.text];
    }

    [self.tableView reloadData];

    ELPlayerViewController *playerViewController = [[[ELPlayerViewController alloc] initWithNibName: @"ELPlayerViewController" bundle: nil] autorelease];
    playerViewController.videoUrl = searchBar.text;
    [self.navigationController pushViewController:playerViewController animated:NO];
}

@end
