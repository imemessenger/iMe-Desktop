// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "base/platform/mac/base_confirm_quit.h"

#include "base/platform/mac/base_utilities_mac.h"

// Thanks Chromium: chrome/browser/ui/cocoa/confirm_quit*

#import <Cocoa/Cocoa.h>
#import <QuartzCore/QuartzCore.h>

namespace {

// How long the user must hold down Cmd+Q to confirm the quit.
constexpr auto kShowDuration = crl::time(1500);

// Duration of the window fade out animation.
constexpr auto kWindowFadeOutDuration = crl::time(200);

// For metrics recording only: How long the user must hold the keys to
// differentitate kDoubleTap from kTapHold.
constexpr auto kDoubleTapTimeDelta = crl::time(320);

// Leeway between the |targetDate| and the current time that will confirm a
// quit.
constexpr auto kTimeDeltaFuzzFactor = crl::time(1000);

} // namespace

@class ConfirmQuitFrameView;

// The ConfirmQuitPanelController manages the black HUD window that tells users
// to "Hold Cmd+Q to Quit".
@interface ConfirmQuitPanelController : NSWindowController<NSWindowDelegate> {
@private
	// The content view of the window that this controller manages.
	ConfirmQuitFrameView* _contentView;  // Weak, owned by the window.
	NSString *_message;
}

// Returns a singleton instance of the Controller. This will create one if it
// does not currently exist.
+ (ConfirmQuitPanelController*)sharedControllerWithMessage:(NSString*)message;

// Runs a modal loop that brings up the panel and handles the logic for if and
// when to terminate. Returns YES if the quit should continue.
- (BOOL)runModalLoopForApplication:(NSApplication*)app;

// Shows the window.
- (void)showWindow:(id)sender;

// If the user did not confirm quit, send this message to give the user
// instructions on how to quit.
- (void)dismissPanel;

@end

// The content view of the window that draws a custom frame.
@interface ConfirmQuitFrameView : NSView {
@private
	NSTextField* _message;  // Weak, owned by the view hierarchy.
}
- (void)setMessageText:(NSString*)text;
@end

@implementation ConfirmQuitFrameView

- (instancetype)initWithFrame:(NSRect)frameRect {
	if ((self = [super initWithFrame:frameRect])) {
		// The frame will be fixed up when |-setMessageText:| is called.
		_message = [[NSTextField alloc] initWithFrame:NSZeroRect];
		[_message setEditable:NO];
		[_message setSelectable:NO];
		[_message setBezeled:NO];
		[_message setDrawsBackground:NO];
		[_message setFont:[NSFont boldSystemFontOfSize:24]];
		[_message setTextColor:[NSColor whiteColor]];
		[self addSubview:_message];
		[_message release];
	}
	return self;
}

- (void)drawRect:(NSRect)dirtyRect {
	const CGFloat kCornerRadius = 5.0;
	NSBezierPath* path = [NSBezierPath
		bezierPathWithRoundedRect:[self bounds]
		xRadius:kCornerRadius
		yRadius:kCornerRadius];

	NSColor* fillColor = [NSColor colorWithCalibratedWhite:0.2 alpha:0.75];
	[fillColor set];
	[path fill];
}

- (void)setMessageText:(NSString*)text {
	const CGFloat kHorizontalPadding = 30;  // In view coordinates.

	// Style the string.
	NSMutableAttributedString *attrString
		= [[NSMutableAttributedString alloc] initWithString:text];
	NSShadow *textShadow = [[NSShadow alloc] init];
	const auto guard = gsl::finally([&] {
		[textShadow release];
		[attrString release];
	});
	[textShadow
		setShadowColor:[NSColor
			colorWithCalibratedWhite:0
			alpha:0.6]];
	[textShadow setShadowOffset:NSMakeSize(0, -1)];
	[textShadow setShadowBlurRadius:1.0];
	[attrString addAttribute:NSShadowAttributeName
					 value:textShadow
					 range:NSMakeRange(0, [text length])];
	[_message setAttributedStringValue:attrString];

	// Fixup the frame of the string.
	[_message sizeToFit];
	NSRect messageFrame = [_message frame];
	NSRect frameInViewSpace
		= [_message convertRect:[[self window] frame] fromView:nil];

	if (NSWidth(messageFrame) > NSWidth(frameInViewSpace)) {
		frameInViewSpace.size.width = NSWidth(messageFrame) + kHorizontalPadding;
	}

	messageFrame.origin.x = NSWidth(frameInViewSpace) / 2 - NSMidX(messageFrame);
	messageFrame.origin.y = NSHeight(frameInViewSpace) / 2 - NSMidY(messageFrame);

	[[self window]
		setFrame:[_message convertRect:frameInViewSpace toView:nil]
		display:YES];
	[_message setFrame:messageFrame];
}

@end

// Animation ///////////////////////////////////////////////////////////////////

// This animation will run through all the windows of the passed-in
// NSApplication and will fade their alpha value to 0.0. When the animation is
// complete, this will release itself.
@interface FadeAllWindowsAnimation : NSAnimation<NSAnimationDelegate> {
@private
	NSApplication* _application;
}
- (instancetype)initWithApplication:(NSApplication*)app
				  animationDuration:(NSTimeInterval)duration;
@end

@implementation FadeAllWindowsAnimation

- (instancetype)initWithApplication:(NSApplication*)app
				  animationDuration:(NSTimeInterval)duration {
	if ((self = [super initWithDuration:duration
					   animationCurve:NSAnimationLinear])) {
		_application = app;
		[self setDelegate:self];
	}
	return self;
}

- (void)setCurrentProgress:(NSAnimationProgress)progress {
	for (NSWindow* window in [_application windows]) {
		[window setAlphaValue:1.0 - progress];
	}
}

- (void)animationDidStop:(NSAnimation*)anim {
	[self autorelease];
}

@end

// Private Interface ///////////////////////////////////////////////////////////

@interface ConfirmQuitPanelController (Private) <CAAnimationDelegate>
- (void)animateFadeOut;
- (NSEvent*)pumpEventQueueForKeyUp:(NSApplication*)app untilDate:(NSDate*)date;
- (void)hideAllWindowsForApplication:(NSApplication*)app
						withDuration:(NSTimeInterval)duration;
- (void)sendAccessibilityAnnouncement;
@end

ConfirmQuitPanelController* g_confirmQuitPanelController = nil;

////////////////////////////////////////////////////////////////////////////////

@implementation ConfirmQuitPanelController

+ (ConfirmQuitPanelController*)sharedControllerWithMessage:(NSString*)message {
	if (!g_confirmQuitPanelController) {
		g_confirmQuitPanelController =
			[[ConfirmQuitPanelController alloc] initWithMessage:message];
	}
	return [[g_confirmQuitPanelController retain] autorelease];
}

- (instancetype)initWithMessage:(NSString*)message {
	const NSRect kWindowFrame = NSMakeRect(0, 0, 350, 70);
	NSWindow *window
		= [[NSWindow alloc]
		   initWithContentRect:kWindowFrame
		   styleMask:NSBorderlessWindowMask
		   backing:NSBackingStoreBuffered
		   defer:NO];
	const auto guard = gsl::finally([&] { [window release]; });

	if ((self = [super initWithWindow:window])) {
		[window setDelegate:self];
		[window setBackgroundColor:[NSColor clearColor]];
		[window setOpaque:NO];
		[window setHasShadow:NO];

		// Create the content view. Take the frame from the existing content view.
		NSRect frame = [[window contentView] frame];
		_contentView = [[ConfirmQuitFrameView alloc] initWithFrame:frame];

		[window setContentView:_contentView];

		// Set the proper string.
		_message = [message retain];
		[_contentView setMessageText:_message];
		[_contentView release];
	}
	return self;
}

- (BOOL)runModalLoopForApplication:(NSApplication*)app {
	ConfirmQuitPanelController *keepAlive = [self retain];
	const auto guard = gsl::finally([&] { [keepAlive release]; });

	// If this is the second of two such attempts to quit within a certain time
	// interval, then just quit.
	// Time of last quit attempt, if any.
	static auto lastQuitAttempt = crl::time();
	const auto timeNow = crl::now();
	if (lastQuitAttempt && (timeNow - lastQuitAttempt) < kTimeDeltaFuzzFactor) {
		// The panel tells users to Hold Cmd+Q. However, we also want to have a
		// double-tap shortcut that allows for a quick quit path. For the users who
		// tap Cmd+Q and then hold it with the window still open, this double-tap
		// logic will run and cause the quit to get committed. If the key
		// combination held down, the system will start sending the Cmd+Q event to
		// the next key application, and so on. This is bad, so instead we hide all
		// the windows (without animation) to look like we've "quit" and then wait
		// for the KeyUp event to commit the quit.
		[self hideAllWindowsForApplication:app withDuration:0];
		NSEvent* nextEvent = [self
			pumpEventQueueForKeyUp:app
			untilDate:[NSDate distantFuture]];
		[app discardEventsMatchingMask:NSAnyEventMask beforeEvent:nextEvent];
		return YES;
	} else {
		lastQuitAttempt = timeNow;
	}

	// Show the info panel that explains what the user must to do confirm quit.
	[self showWindow:self];

	// Explicitly announce the hold-to-quit message. For an ordinary modal dialog
	// VoiceOver would announce it and read its message, but VoiceOver does not do
	// this for windows whose styleMask is NSBorderlessWindowMask, so do it
	// manually here. Without this screenreader users have no way to know why
	// their quit hotkey seems not to work.
	[self sendAccessibilityAnnouncement];

	// Spin a nested run loop until the |targetDate| is reached or a KeyUp event
	// is sent.
	const auto targetDate = crl::now() + kShowDuration;
	BOOL willQuit = NO;
	NSEvent* nextEvent = nil;
	do {
		// Dequeue events until a key up is received. To avoid busy waiting, figure
		// out the amount of time that the thread can sleep before taking further
		// action.
		NSDate* waitDate = [NSDate
			dateWithTimeIntervalSinceNow:(kShowDuration - kTimeDeltaFuzzFactor) / 1000.];
		nextEvent = [self pumpEventQueueForKeyUp:app untilDate:waitDate];

		// Wait for the time expiry to happen. Once past the hold threshold,
		// commit to quitting and hide all the open windows.
		if (!willQuit) {
			const auto now = crl::now();
			const auto difference = (targetDate - now);
			if (difference < kTimeDeltaFuzzFactor) {
				willQuit = YES;

				// At this point, the quit has been confirmed and windows should all
				// fade out to convince the user to release the key combo to finalize
				// the quit.
				[self
					hideAllWindowsForApplication:app
					withDuration:kWindowFadeOutDuration / 1000.];
			}
		}
	} while (!nextEvent);

	// The user has released the key combo. Discard any events (i.e. the
	// repeated KeyDown Cmd+Q).
	[app discardEventsMatchingMask:NSAnyEventMask beforeEvent:nextEvent];

	if (willQuit) {
		// The user held down the combination long enough that quitting should
		// happen.
		return YES;
	} else {
		// Slowly fade the confirm window out in case the user doesn't
		// understand what they have to do to quit.
		[self dismissPanel];
		return NO;
	}

	// Default case: terminate.
	return YES;
}

- (void)windowWillClose:(NSNotification*)notif {
  // Release all animations because CAAnimation retains its delegate (self),
  // which will cause a retain cycle. Break it!
  [[self window] setAnimations:@{}];
  g_confirmQuitPanelController = nil;
  [self autorelease];
}

- (void)showWindow:(id)sender {
	// If a panel that is fading out is going to be reused here, make sure it
	// does not get released when the animation finishes.
	ConfirmQuitPanelController *keepAlive = [self retain];
	const auto guard = gsl::finally([&] { [keepAlive release]; });
	[[self window] setAnimations:@{}];
	[[self window] center];
	[[self window] setAlphaValue:1.0];
	[super showWindow:sender];
}

- (void)dismissPanel {
	[self
		performSelector:@selector(animateFadeOut)
		withObject:nil
		afterDelay:1.0];
}

- (void)animateFadeOut {
	NSWindow* window = [self window];
	CAAnimation *animation
		= [[window animationForKey:@"alphaValue"] copy];
	const auto guard = gsl::finally([&] { [animation release]; });
	[animation setDelegate:self];
	[animation setDuration:0.2];
	NSMutableDictionary* dictionary
		= [NSMutableDictionary dictionaryWithDictionary:[window animations]];
	dictionary[@"alphaValue"] = animation;
	[window setAnimations:dictionary];
	[[window animator] setAlphaValue:0.0];
}

- (void)animationDidStart:(CAAnimation*)theAnimation {
	// CAAnimationDelegate method added on OSX 10.12.
}

- (void)animationDidStop:(CAAnimation*)theAnimation finished:(BOOL)finished {
	[self close];
}

// Runs a nested loop that pumps the event queue until the next KeyUp event.
- (NSEvent*)pumpEventQueueForKeyUp:(NSApplication*)app untilDate:(NSDate*)date {
	return [app
		nextEventMatchingMask:NSKeyUpMask
		untilDate:date
		inMode:NSEventTrackingRunLoopMode
		dequeue:YES];
}

// Iterates through the list of open windows and hides them all.
- (void)hideAllWindowsForApplication:(NSApplication*)app
						withDuration:(NSTimeInterval)duration {
	FadeAllWindowsAnimation* animation =
		[[FadeAllWindowsAnimation alloc] initWithApplication:app
										 animationDuration:duration];
	// Releases itself when the animation stops.
	[animation startAnimation];
}

- (void)sendAccessibilityAnnouncement {
	NSAccessibilityPostNotificationWithUserInfo(
		[NSApp mainWindow], NSAccessibilityAnnouncementRequestedNotification, @{
			NSAccessibilityAnnouncementKey : _message,
			NSAccessibilityPriorityKey : @(NSAccessibilityPriorityHigh),
		});
}

@end

namespace Platform::ConfirmQuit {
namespace {

// This returns the NSMenuItem that quits the application.
[[nodiscard]] NSMenuItem *QuitMenuItem() {
	NSMenu* mainMenu = [NSApp mainMenu];
	// Get the application menu (i.e. Chromium).
	NSMenu* appMenu = [[mainMenu itemAtIndex:0] submenu];
	for (NSMenuItem* item in [appMenu itemArray]) {
		// Find the Quit item.
		if ([item action] == @selector(terminate:)) {
			return item;
		}
	}

	// Default to Cmd+Q.
	NSMenuItem* item = [[[NSMenuItem alloc]
		initWithTitle:@""
		action:@selector(terminate:)
		keyEquivalent:@"q"] autorelease];
	item.keyEquivalentModifierMask = NSCommandKeyMask;
	return item;
}

[[nodiscard]] QString KeyCombinationForMenuItem(NSMenuItem *item) {
	auto result = QString();

	NSUInteger modifiers = item.keyEquivalentModifierMask;
	if (modifiers & NSCommandKeyMask) {
		result.append(QChar(0x2318));
	}
	if (modifiers & NSControlKeyMask) {
		result.append(QChar(0x2303));
	}
	if (modifiers & NSAlternateKeyMask) {
		result.append(QChar(0x2325));
	}
	if (modifiers & NSShiftKeyMask) {
		result.append(QChar(0x21E7));
	}
	result.append(NS2QString([item.keyEquivalent uppercaseString]));

	return result;
}

// This looks at the Main Menu and determines what the user has set as the
// key combination for quit. It then gets the modifiers and builds a string
// to display them.
[[nodiscard]] QString KeyCommandString() {
	return KeyCombinationForMenuItem(QuitMenuItem());
}

} // namespace

bool RunModal(QString text) {
	return [[ConfirmQuitPanelController sharedControllerWithMessage:Q2NSString(text)]
		runModalLoopForApplication:NSApp];
}

QString QuitKeysString() {
	return KeyCommandString();
}

} // namespace Platform::ConfirmQuit
