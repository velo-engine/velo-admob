// Google AdMob bridge for iOS (Objective-C).
// Supports Rewarded and Interstitial ads with Google Mobile Ads SDK.
// Operates on the main thread and polls result via atomic C calls.

#pragma once

#if defined(__APPLE__)
#include <TargetConditionals.h>
#if TARGET_OS_IPHONE || TARGET_OS_SIMULATOR

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

@interface VeloAdMobIOSBridge : NSObject
+ (instancetype)sharedInstance;
- (void)initSDKWithRewardedUnit:(NSString *)rewardedUnit interstitialUnit:(NSString *)interstitialUnit;
- (BOOL)isRewardedReady;
- (void)retryRewarded;
- (BOOL)showRewarded;
- (int)pollRewardedResult;
- (BOOL)isInterstitialReady;
- (void)retryInterstitial;
- (BOOL)showInterstitial;
- (int)pollInterstitialResult;
@end

static inline UIViewController* velo_admob_ios_top_view_controller(void) {
    UIWindow *keyWindow = nil;
    if (@available(iOS 13.0, *)) {
        for (UIScene *scene in [UIApplication sharedApplication].connectedScenes) {
            if (scene.activationState == UISceneActivationStateForegroundActive && [scene isKindOfClass:[UIWindowScene class]]) {
                UIWindowScene *windowScene = (UIWindowScene *)scene;
                for (UIWindow *w in windowScene.windows) {
                    if (w.isKeyWindow) {
                        keyWindow = w;
                        break;
                    }
                }
            }
            if (keyWindow) break;
        }
    }
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    if (!keyWindow) {
        keyWindow = [UIApplication sharedApplication].keyWindow;
    }
    if (!keyWindow && [UIApplication sharedApplication].windows.count > 0) {
        keyWindow = [UIApplication sharedApplication].windows.firstObject;
    }
#pragma clang diagnostic pop

    UIViewController *vc = keyWindow ? keyWindow.rootViewController : nil;
    while (vc && vc.presentedViewController) {
        vc = vc.presentedViewController;
    }
    return vc;
}

@implementation VeloAdMobIOSBridge {
    BOOL _rewardedReady;
    BOOL _rewardedLoading;
    int _rewardedResult;
    BOOL _rewardedEarned;
    id _rewardedAd;
    NSString *_rewardedUnitId;

    BOOL _interstitialReady;
    BOOL _interstitialLoading;
    int _interstitialResult;
    id _interstitialAd;
    NSString *_interstitialUnitId;

    BOOL _initialized;
    BOOL _mockMode;
}

+ (instancetype)sharedInstance {
    static VeloAdMobIOSBridge *inst = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        inst = [[VeloAdMobIOSBridge alloc] init];
    });
    return inst;
}

- (instancetype)init {
    self = [super init];
    if (self) {
        _rewardedUnitId = @"";
        _interstitialUnitId = @"";
    }
    return self;
}

- (void)initSDKWithRewardedUnit:(NSString *)rewardedUnit interstitialUnit:(NSString *)interstitialUnit {
    _rewardedUnitId = rewardedUnit ? [rewardedUnit copy] : @"";
    _interstitialUnitId = interstitialUnit ? [interstitialUnit copy] : @"";

    dispatch_async(dispatch_get_main_queue(), ^{
        Class gadMobileAds = NSClassFromString(@"GADMobileAds");
        if (!gadMobileAds) {
            NSLog(@"[VeloAdMob iOS] GADMobileAds framework not linked — running in mock mode");
            self->_mockMode = YES;
            self->_rewardedReady = YES;
            self->_interstitialReady = YES;
            self->_initialized = YES;
            return;
        }

        self->_mockMode = NO;
        self->_initialized = YES;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Warc-performSelector-leaks"
        id sharedAds = [gadMobileAds performSelector:NSSelectorFromString(@"sharedInstance")];
#pragma clang diagnostic pop
        if ([sharedAds respondsToSelector:NSSelectorFromString(@"startWithCompletionHandler:")]) {
            void (^comp)(id) = ^(id status) {
                NSLog(@"[VeloAdMob iOS] GADMobileAds initialized");
                [self loadRewarded];
                [self loadInterstitial];
            };
            NSMethodSignature *sig = [sharedAds methodSignatureForSelector:NSSelectorFromString(@"startWithCompletionHandler:")];
            NSInvocation *inv = [NSInvocation invocationWithMethodSignature:sig];
            [inv setSelector:NSSelectorFromString(@"startWithCompletionHandler:")];
            [inv setTarget:sharedAds];
            [inv setArgument:&comp atIndex:2];
            [inv invoke];
        }
    });
}

- (void)loadRewarded {
    if (_mockMode) {
        _rewardedReady = YES;
        return;
    }
    if (_rewardedLoading || _rewardedReady || _rewardedUnitId.length == 0) return;
    _rewardedLoading = YES;

    Class gadRewarded = NSClassFromString(@"GADRewardedAd");
    Class gadRequest = NSClassFromString(@"GADRequest");
    if (!gadRewarded || !gadRequest) {
        _rewardedLoading = NO;
        return;
    }

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Warc-performSelector-leaks"
    id req = [gadRequest performSelector:NSSelectorFromString(@"request")];
#pragma clang diagnostic pop

    void (^completion)(id, NSError *) = ^(id ad, NSError *err) {
        if (err || !ad) {
            NSLog(@"[VeloAdMob iOS] Rewarded ad load failed: %@", err ? [err localizedDescription] : @"unknown");
            self->_rewardedAd = nil;
            self->_rewardedLoading = NO;
            self->_rewardedReady = NO;
            return;
        }
        NSLog(@"[VeloAdMob iOS] Rewarded ad loaded");
        self->_rewardedAd = ad;
        if ([ad respondsToSelector:NSSelectorFromString(@"setFullScreenContentDelegate:")]) {
            [ad setValue:self forKey:@"fullScreenContentDelegate"];
        }
        self->_rewardedLoading = NO;
        self->_rewardedReady = YES;
    };

    SEL loadSel = NSSelectorFromString(@"loadWithAdUnitID:request:completionHandler:");
    if ([gadRewarded respondsToSelector:loadSel]) {
        NSMethodSignature *sig = [gadRewarded methodSignatureForSelector:loadSel];
        NSInvocation *inv = [NSInvocation invocationWithMethodSignature:sig];
        [inv setSelector:loadSel];
        [inv setTarget:gadRewarded];
        NSString *u = _rewardedUnitId;
        [inv setArgument:&u atIndex:2];
        [inv setArgument:&req atIndex:3];
        [inv setArgument:&completion atIndex:4];
        [inv invoke];
    }
}

- (void)loadInterstitial {
    if (_mockMode) {
        _interstitialReady = YES;
        return;
    }
    if (_interstitialLoading || _interstitialReady || _interstitialUnitId.length == 0) return;
    _interstitialLoading = YES;

    Class gadInterstitial = NSClassFromString(@"GADInterstitialAd");
    Class gadRequest = NSClassFromString(@"GADRequest");
    if (!gadInterstitial || !gadRequest) {
        _interstitialLoading = NO;
        return;
    }

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Warc-performSelector-leaks"
    id req = [gadRequest performSelector:NSSelectorFromString(@"request")];
#pragma clang diagnostic pop

    void (^completion)(id, NSError *) = ^(id ad, NSError *err) {
        if (err || !ad) {
            NSLog(@"[VeloAdMob iOS] Interstitial ad load failed: %@", err ? [err localizedDescription] : @"unknown");
            self->_interstitialAd = nil;
            self->_interstitialLoading = NO;
            self->_interstitialReady = NO;
            return;
        }
        NSLog(@"[VeloAdMob iOS] Interstitial ad loaded");
        self->_interstitialAd = ad;
        if ([ad respondsToSelector:NSSelectorFromString(@"setFullScreenContentDelegate:")]) {
            [ad setValue:self forKey:@"fullScreenContentDelegate"];
        }
        self->_interstitialLoading = NO;
        self->_interstitialReady = YES;
    };

    SEL loadSel = NSSelectorFromString(@"loadWithAdUnitID:request:completionHandler:");
    if ([gadInterstitial respondsToSelector:loadSel]) {
        NSMethodSignature *sig = [gadInterstitial methodSignatureForSelector:loadSel];
        NSInvocation *inv = [NSInvocation invocationWithMethodSignature:sig];
        [inv setSelector:loadSel];
        [inv setTarget:gadInterstitial];
        NSString *u = _interstitialUnitId;
        [inv setArgument:&u atIndex:2];
        [inv setArgument:&req atIndex:3];
        [inv setArgument:&completion atIndex:4];
        [inv invoke];
    }
}

// Delegate callbacks
- (void)adDidDismissFullScreenContent:(id)ad {
    if (ad == _rewardedAd) {
        _rewardedResult = _rewardedEarned ? 1 : 2;
        _rewardedAd = nil;
        [self loadRewarded];
    } else if (ad == _interstitialAd) {
        _interstitialResult = 2;
        _interstitialAd = nil;
        [self loadInterstitial];
    }
}

- (void)ad:(id)ad didFailToPresentFullScreenContentWithError:(NSError *)error {
    NSLog(@"[VeloAdMob iOS] Ad presentation failed: %@", error ? [error localizedDescription] : @"unknown");
    if (ad == _rewardedAd) {
        _rewardedResult = 3;
        _rewardedAd = nil;
        [self loadRewarded];
    } else if (ad == _interstitialAd) {
        _interstitialResult = 3;
        _interstitialAd = nil;
        [self loadInterstitial];
    }
}

- (BOOL)isRewardedReady {
    return _rewardedReady;
}

- (void)retryRewarded {
    dispatch_async(dispatch_get_main_queue(), ^{
        [self loadRewarded];
    });
}

- (BOOL)showRewarded {
    _rewardedResult = 0;
    if (_mockMode) {
        _rewardedResult = 1;
        return YES;
    }
    if (!_rewardedReady || !_rewardedAd) {
        _rewardedResult = 3;
        [self retryRewarded];
        return NO;
    }
    _rewardedReady = NO;
    _rewardedEarned = NO;

    dispatch_async(dispatch_get_main_queue(), ^{
        UIViewController *vc = velo_admob_ios_top_view_controller();
        if (!vc) {
            self->_rewardedResult = 3;
            [self loadRewarded];
            return;
        }

        void (^earnHandler)(void) = ^{
            self->_rewardedEarned = YES;
        };

        SEL presentSel = NSSelectorFromString(@"presentFromRootViewController:userDidEarnRewardHandler:");
        if ([self->_rewardedAd respondsToSelector:presentSel]) {
            NSMethodSignature *sig = [self->_rewardedAd methodSignatureForSelector:presentSel];
            NSInvocation *inv = [NSInvocation invocationWithMethodSignature:sig];
            [inv setSelector:presentSel];
            [inv setTarget:self->_rewardedAd];
            [inv setArgument:&vc atIndex:2];
            [inv setArgument:&earnHandler atIndex:3];
            [inv invoke];
        }
    });
    return YES;
}

- (int)pollRewardedResult {
    int r = _rewardedResult;
    if (r != 0) _rewardedResult = 0;
    return r;
}

- (BOOL)isInterstitialReady {
    return _interstitialReady;
}

- (void)retryInterstitial {
    dispatch_async(dispatch_get_main_queue(), ^{
        [self loadInterstitial];
    });
}

- (BOOL)showInterstitial {
    _interstitialResult = 0;
    if (_mockMode) {
        _interstitialResult = 2;
        return YES;
    }
    if (!_interstitialReady || !_interstitialAd) {
        _interstitialResult = 3;
        [self retryInterstitial];
        return NO;
    }
    _interstitialReady = NO;

    dispatch_async(dispatch_get_main_queue(), ^{
        UIViewController *vc = velo_admob_ios_top_view_controller();
        if (!vc) {
            self->_interstitialResult = 3;
            [self loadInterstitial];
            return;
        }

        SEL presentSel = NSSelectorFromString(@"presentFromRootViewController:");
        if ([self->_interstitialAd respondsToSelector:presentSel]) {
            NSMethodSignature *sig = [self->_interstitialAd methodSignatureForSelector:presentSel];
            NSInvocation *inv = [NSInvocation invocationWithMethodSignature:sig];
            [inv setSelector:presentSel];
            [inv setTarget:self->_interstitialAd];
            [inv setArgument:&vc atIndex:2];
            [inv invoke];
        }
    });
    return YES;
}

- (int)pollInterstitialResult {
    int r = _interstitialResult;
    if (r != 0) _interstitialResult = 0;
    return r;
}

@end

// C interface
static inline void velo_admob_ios_init(const char* rewarded_unit, const char* interstitial_unit) {
    NSString *r = rewarded_unit ? [NSString stringWithUTF8String:rewarded_unit] : @"";
    NSString *i = interstitial_unit ? [NSString stringWithUTF8String:interstitial_unit] : @"";
    [[VeloAdMobIOSBridge sharedInstance] initSDKWithRewardedUnit:r interstitialUnit:i];
}

static inline int velo_admob_ios_is_rewarded_ready(void) {
    return [[VeloAdMobIOSBridge sharedInstance] isRewardedReady] ? 1 : 0;
}

static inline void velo_admob_ios_retry_rewarded(void) {
    [[VeloAdMobIOSBridge sharedInstance] retryRewarded];
}

static inline int velo_admob_ios_show_rewarded(void) {
    return [[VeloAdMobIOSBridge sharedInstance] showRewarded] ? 1 : 0;
}

static inline int velo_admob_ios_poll_rewarded(void) {
    return [[VeloAdMobIOSBridge sharedInstance] pollRewardedResult];
}

static inline int velo_admob_ios_is_interstitial_ready(void) {
    return [[VeloAdMobIOSBridge sharedInstance] isInterstitialReady] ? 1 : 0;
}

static inline void velo_admob_ios_retry_interstitial(void) {
    [[VeloAdMobIOSBridge sharedInstance] retryInterstitial];
}

static inline int velo_admob_ios_show_interstitial(void) {
    return [[VeloAdMobIOSBridge sharedInstance] showInterstitial] ? 1 : 0;
}

static inline int velo_admob_ios_poll_interstitial(void) {
    return [[VeloAdMobIOSBridge sharedInstance] pollInterstitialResult];
}

#endif // TARGET_OS_IPHONE
#endif // __APPLE__
