//
//  ViewController.m
//  SpacePruner
//
//  Created by Derek Ledbetter on 2017-07-06.
//

#import "ViewController.h"

#include "despacebenchmark.h"
#include <stdio.h>

@interface ViewController () {
  BOOL _currentlyRunning; // TODO: change to property
  NSMutableString *_logText;
}

@property (nonatomic, weak) IBOutlet UIButton *button;
@property (nonatomic, weak) IBOutlet UILabel *label;
@end


@interface ViewController (Private)
- (void)addToLogFromBytes:(const char*)bytes length:(int)length;
@end

int streamWriteFunction(void *cookie, const char *buf, int nbyte) {
  __weak ViewController* weakController = (__bridge __weak ViewController*)cookie;
  ViewController* controller = weakController;
  if (controller) {
    [controller addToLogFromBytes:buf length:nbyte];
  }
  return nbyte;
}

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
  _logText = [NSMutableString string];
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
}

- (IBAction)runTest:(id)sender {
  if (_currentlyRunning) {
    return;
  }

  _currentlyRunning = YES;
  [self updateRunButton];
  [_logText deleteCharactersInRange:(NSRange){ 0, _logText.length } ];

  __weak ViewController *weakSelf = self;
  dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^(void) {
    void* cookie = (__bridge void*)weakSelf;
    FILE* stream = fwopen(cookie, streamWriteFunction);
    despace_benchmark(stream);
    fclose(stream);

    ViewController* self = weakSelf;
    if (self) {
      dispatch_async(dispatch_get_main_queue(), ^(void) {
        self->_currentlyRunning = NO;
        [self updateRunButton];
      });
    }
  });
}

- (void)addToLogFromBytes:(const char*)bytes length:(int)length {
  NSString* newString = [[NSString alloc] initWithBytes:bytes length:length encoding:NSASCIIStringEncoding];
  dispatch_async(dispatch_get_main_queue(), ^{
    [_logText appendString:newString];
    self.label.text = _logText;
  });
}

@end
