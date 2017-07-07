//
//  ViewController.m
//  SpacePruner
//
//  Created by Derek Ledbetter on 2017-07-06.
//  Copyright © 2017 Derek Ledbetter. All rights reserved.
//

#import "ViewController.h"

#include "despacebenchmark.h"

@interface ViewController () {
    BOOL _currentlyRunning; // TODO: change to property
}

@property (nonatomic, weak) IBOutlet UIButton *button;
@property (nonatomic, weak) IBOutlet UILabel *label;

@end

@implementation ViewController

- (instancetype)initWithNibName:(NSString *)nibNameOrNil bundle:(NSBundle *)nibBundleOrNil {
    self = [super initWithNibName:nibNameOrNil bundle:nibBundleOrNil];
    [self setUp];
    return self;
}

- (nullable instancetype)initWithCoder:(NSCoder *)aDecoder {
    self = [super initWithCoder:aDecoder];
    [self setUp];
    return self;
}

- (void)setUp {
    _currentlyRunning = NO;
}

- (void)viewDidLoad {
    [super viewDidLoad];
    [self updateRunButton];
}

- (void)updateRunButton {
    self.button.enabled = !_currentlyRunning;
}

- (void)didReceiveMemoryWarning {
    [super didReceiveMemoryWarning];
    // Dispose of any resources that can be recreated.
}

- (IBAction)runTest:(id)sender {
    // TODO
    if (_currentlyRunning) {
        return;
    }

    _currentlyRunning = YES;
    [self updateRunButton];
    __weak ViewController *weakSelf = self;
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^(void) {
        // TODO: print results to screen.
        despace_benchmark();
        NSString* message = @"Printed benchmark results to log.\n";

        ViewController* self = weakSelf;
        if (self) {
            dispatch_async(dispatch_get_main_queue(), ^(void) {
                self->_currentlyRunning = NO;
                [self updateRunButton];
                self.label.text = message;
            });
        }
    });
}

@end
