// @import UIKit;
// @import Webkit;
// #import <UIKit/UIKit.h>
// #import <Cocoa/Cocoa.h>
#import <Webkit/WebKit.h>

@interface ViewController : UIViewController
@property(strong,nonatomic) WKWebView *webView;
@property (strong, nonatomic) NSString *productURL;
@end

@implementation ViewController

- (void)viewDidLoad {
    [super viewDidLoad];
    NSURL *url = [NSURL URLWithString:self.productURL];
    NSURLRequest *request = [NSURLRequest requestWithURL:url];

    _webView = [[WKWebView alloc] initWithFrame:self.view.frame];
    [_webView loadRequest:request];
    _webView.frame = CGRectMake(self.view.frame.origin.x,self.view.frame.origin.y, self.view.frame.size.width, self.view.frame.size.height);
    self.view = _webView;
}

@end

@interface AppDelegate : UIResponder <UIApplicationDelegate>
@property(strong, nonatomic) UIWindow *window;
@end

NSString* mainURL;

@implementation AppDelegate

- (BOOL)application:(UIApplication *)application
    didFinishLaunchingWithOptions:(NSDictionary *)launchOptions {
  self.window = [[UIWindow alloc] initWithFrame:[[UIScreen mainScreen] bounds]];
  ViewController *view_controller = [[ViewController alloc] init];
    view_controller.productURL = mainURL;
  UINavigationController *navigation_controller =
      [[UINavigationController alloc]
          initWithRootViewController:view_controller];
  navigation_controller.navigationBar.translucent = YES;
  [self.window setRootViewController:navigation_controller];
  [self.window makeKeyAndVisible];
  return YES;
}

@end

int createWebview(char* url) {
    mainURL = [NSString stringWithUTF8String:url];
    char *argv[0];
  @autoreleasepool {
    return UIApplicationMain(0, argv, nil,
                             NSStringFromClass([AppDelegate class]));
  }
}