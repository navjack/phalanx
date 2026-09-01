#import <AppKit/AppKit.h>
#import <GameController/GameController.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#include "input.h"
#include "platform_macos.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static GCController *s_gamepad;
static id s_gamepad_connect_observer;
static id s_gamepad_disconnect_observer;

int MacPrepareSupportDirectory(char *path, size_t path_size) {
  if (!path || path_size == 0)
    return 0;

  @autoreleasepool {
    NSFileManager *files = NSFileManager.defaultManager;
    NSError *error = nil;
    NSURL *base = [files URLForDirectory:NSApplicationSupportDirectory
                                inDomain:NSUserDomainMask
                       appropriateForURL:nil
                                  create:YES
                                   error:&error];
    if (!base)
      return 0;

    NSURL *directory = [base URLByAppendingPathComponent:@"Phalanx Recompiled"
                                             isDirectory:YES];
    if (![files createDirectoryAtURL:directory
          withIntermediateDirectories:YES
                           attributes:nil
                                error:&error])
      return 0;

    const char *directory_path = directory.fileSystemRepresentation;
    if (!directory_path || strlen(directory_path) >= path_size)
      return 0;
    memcpy(path, directory_path, strlen(directory_path) + 1);
    return 1;
  }
}

static GCController *FirstExtendedGamepad(void) {
  for (GCController *controller in GCController.controllers) {
    if (controller.extendedGamepad)
      return controller;
  }
  return nil;
}

static int GamepadTraceEnabled(void) {
  const char *value = getenv("PHALANX_INPUT_TRACE");
  return value && value[0] && strcmp(value, "0") != 0;
}

static void TraceGamepad(const char *event, GCController *controller) {
  if (!GamepadTraceEnabled())
    return;
  const char *name = controller.vendorName.UTF8String;
  fprintf(stderr, "Phalanx gamepad: %s%s%s\n", event,
          name ? " " : "", name ? name : "");
}

static void SelectGamepad(GCController *controller) {
  if (controller == s_gamepad || !controller.extendedGamepad)
    return;
  s_gamepad = controller;
  s_gamepad.playerIndex = GCControllerPlayerIndex1;
  TraceGamepad("using", s_gamepad);
}

void MacGamepadInitialize(void) {
  if (s_gamepad_connect_observer)
    return;

  GCController.shouldMonitorBackgroundEvents = NO;
  SelectGamepad(FirstExtendedGamepad());

  NSNotificationCenter *center = NSNotificationCenter.defaultCenter;
  NSOperationQueue *main_queue = NSOperationQueue.mainQueue;
  s_gamepad_connect_observer = [center
      addObserverForName:GCControllerDidConnectNotification
                  object:nil
                   queue:main_queue
              usingBlock:^(NSNotification *notification) {
                GCController *controller = notification.object;
                if (!s_gamepad)
                  SelectGamepad(controller);
              }];
  s_gamepad_disconnect_observer = [center
      addObserverForName:GCControllerDidDisconnectNotification
                  object:nil
                   queue:main_queue
              usingBlock:^(NSNotification *notification) {
                GCController *controller = notification.object;
                if (controller != s_gamepad)
                  return;
                TraceGamepad("disconnected", controller);
                s_gamepad = nil;
                SelectGamepad(FirstExtendedGamepad());
              }];
}

void MacGamepadShutdown(void) {
  NSNotificationCenter *center = NSNotificationCenter.defaultCenter;
  if (s_gamepad_connect_observer)
    [center removeObserver:s_gamepad_connect_observer];
  if (s_gamepad_disconnect_observer)
    [center removeObserver:s_gamepad_disconnect_observer];
  s_gamepad_connect_observer = nil;
  s_gamepad_disconnect_observer = nil;
  s_gamepad = nil;
}

uint16_t MacGamepadReadButtons(void) {
  GCController *current = GCController.current;
  if (current.extendedGamepad)
    SelectGamepad(current);

  GCExtendedGamepad *gamepad = s_gamepad.extendedGamepad;
  if (!gamepad)
    return 0;

  uint16_t buttons = 0;
  if (gamepad.rightShoulder.isPressed) buttons |= PHALANX_INPUT_R;
  if (gamepad.leftShoulder.isPressed)  buttons |= PHALANX_INPUT_L;

  /* Apple's extended profile names face buttons by their physical diamond:
   * Y / X-B / A. Map by position to the equivalent SNES diamond. */
  if (gamepad.buttonY.isPressed) buttons |= PHALANX_INPUT_X;
  if (gamepad.buttonB.isPressed) buttons |= PHALANX_INPUT_A;
  if (gamepad.buttonX.isPressed) buttons |= PHALANX_INPUT_Y;
  if (gamepad.buttonA.isPressed) buttons |= PHALANX_INPUT_B;

  if (gamepad.buttonMenu.isPressed)    buttons |= PHALANX_INPUT_START;
  if (gamepad.buttonOptions.isPressed) buttons |= PHALANX_INPUT_SELECT;

  GCControllerDirectionPad *dpad = gamepad.dpad;
  if (dpad.right.isPressed) buttons |= PHALANX_INPUT_RIGHT;
  if (dpad.left.isPressed)  buttons |= PHALANX_INPUT_LEFT;
  if (dpad.down.isPressed)  buttons |= PHALANX_INPUT_DOWN;
  if (dpad.up.isPressed)    buttons |= PHALANX_INPUT_UP;

  const float analog_threshold = 0.35f;
  const float x = gamepad.leftThumbstick.xAxis.value;
  const float y = gamepad.leftThumbstick.yAxis.value;
  if (x >  analog_threshold) buttons |= PHALANX_INPUT_RIGHT;
  if (x < -analog_threshold) buttons |= PHALANX_INPUT_LEFT;
  if (y < -analog_threshold) buttons |= PHALANX_INPUT_DOWN;
  if (y >  analog_threshold) buttons |= PHALANX_INPUT_UP;

  return buttons;
}

int MacChoosePhalanxRom(char *path, size_t path_size) {
  if (!path || path_size == 0)
    return 0;

  @autoreleasepool {
    [NSApplication sharedApplication];
    NSOpenPanel *panel = [NSOpenPanel openPanel];
    panel.title = @"Choose your Phalanx (USA) ROM";
    panel.message = @"Select the clean, unheadered Phalanx (USA) .sfc file.";
    panel.prompt = @"Open ROM";
    panel.canChooseFiles = YES;
    panel.canChooseDirectories = NO;
    panel.allowsMultipleSelection = NO;
    panel.allowedContentTypes = @[
      [UTType typeWithFilenameExtension:@"sfc"],
      [UTType typeWithFilenameExtension:@"smc"],
    ];

    if ([panel runModal] != NSModalResponseOK)
      return 0;

    const char *selected = panel.URL.fileSystemRepresentation;
    if (!selected || strlen(selected) >= path_size)
      return 0;
    memcpy(path, selected, strlen(selected) + 1);
    return 1;
  }
}

void MacShowError(const char *title, const char *message) {
  @autoreleasepool {
    [NSApplication sharedApplication];
    NSAlert *alert = [[NSAlert alloc] init];
    alert.alertStyle = NSAlertStyleCritical;
    alert.messageText = title ? [NSString stringWithUTF8String:title]
                              : @"Phalanx";
    alert.informativeText = message ? [NSString stringWithUTF8String:message]
                                    : @"Unknown error";
    [alert addButtonWithTitle:@"OK"];
    [alert runModal];
  }
}
