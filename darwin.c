#import <Foundation/NSArray.h>
#import <Foundation/Foundation.h>
#import <SystemConfiguration/SCPreferences.h>
#import <SystemConfiguration/SCNetworkConfiguration.h>
#include <sys/syslimits.h>
#include <sys/stat.h>
#include <mach-o/dyld.h>
#include "common.h"

int elevate(char *path, char *prompt, char *iconPath)
{
  AuthorizationEnvironment authEnv;
  AuthorizationItem kAuthEnv[2];
  authEnv.items = kAuthEnv;
  authEnv.count = 0;

  if (prompt != NULL) {
    kAuthEnv[authEnv.count].name = kAuthorizationEnvironmentPrompt;
    kAuthEnv[authEnv.count].valueLength = strlen(prompt);
    kAuthEnv[authEnv.count].value = prompt;
    kAuthEnv[authEnv.count].flags = 0;
    authEnv.count++;
  }
  if (iconPath != NULL) {
    kAuthEnv[authEnv.count].name = kAuthorizationEnvironmentIcon;
    kAuthEnv[authEnv.count].valueLength = strlen(iconPath);
    kAuthEnv[authEnv.count].value = iconPath;
    kAuthEnv[authEnv.count].flags = 0;
    authEnv.count++;
  }

  AuthorizationItem authItems[1];
  authItems[0].name = kAuthorizationRightExecute;
  authItems[0].valueLength = 0;
  authItems[0].value = NULL;
  authItems[0].flags = 0;

  AuthorizationRights authRights;
  authRights.count = sizeof(authItems) / sizeof(authItems[0]);
  authRights.items = authItems;

  AuthorizationFlags authFlags;
  authFlags = kAuthorizationFlagDefaults | kAuthorizationFlagInteractionAllowed | kAuthorizationFlagExtendRights;

  AuthorizationRef authRef;
  OSStatus status = AuthorizationCreate(&authRights, &authEnv, authFlags, &authRef);
  if(status != errAuthorizationSuccess) {
    NSLog(@"Error create authorization");
    return NO_PERMISSION;
  }

  FILE *pipe = NULL;
  char* argv[] = { "setuid", NULL };
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
  status = AuthorizationExecuteWithPrivileges(authRef, path, kAuthorizationFlagDefaults, argv, &pipe);
#pragma GCC diagnostic warning "-Wdeprecated-declarations"
  if(status != errAuthorizationSuccess) {
    NSLog(@"Error run %s with privileges: %d", path, status);
  } else {
    char readBuffer[256];
    for(;;) {
      ssize_t len = read(fileno(pipe), readBuffer, sizeof(readBuffer));
      if (len <= 0) { break; }
      write(STDERR_FILENO, readBuffer, len);
    }
    fclose(pipe);
  }

  AuthorizationFree(authRef, kAuthorizationFlagDestroyRights);
  return status == errAuthorizationSuccess ? 0 : -1;
}

int setUid()
{
  AuthorizationRef authRef;
  OSStatus result;
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
  result = AuthorizationCopyPrivilegedReference(&authRef, kAuthorizationFlagDefaults);
#pragma GCC diagnostic warning "-Wdeprecated-declarations"
  if (result != errAuthorizationSuccess) {
    // AuthorizationExecuteWithPrivileges in elevate() can only read stdout,
    //so we print all errors to stdout, same below.
    puts("Not running as root");
    return NO_PERMISSION;
  }
  char exeFullPath [PATH_MAX];
  uint32_t size = PATH_MAX;
  if (_NSGetExecutablePath(exeFullPath, &size) != 0)
  {
    printf("Path longer than %d, should not occur!!!!!", size);
    return SYSCALL_FAILED;
  }
  if (chown(exeFullPath, 0, 0) != 0) // root:wheel
  {
    puts("Error chown");
    return NO_PERMISSION;
  }
  if (chmod(exeFullPath, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH | S_ISUID) != 0)
  {
    puts("Error chmod");
    return NO_PERMISSION;
  }
  return RET_NO_ERROR;
}

int togglePac(bool turnOn, const char* pacUrl)
{
  NSString* nsPacUrl = [[NSString alloc] initWithCString: pacUrl encoding:NSUTF8StringEncoding];
  int ret = RET_NO_ERROR;
  Boolean success;

  SCNetworkSetRef networkSetRef;
  CFArrayRef networkServicesArrayRef;
  SCNetworkServiceRef networkServiceRef;
  SCNetworkProtocolRef proxyProtocolRef;
  NSDictionary *oldPreferences;
  NSMutableDictionary *newPreferences;
  NSString *wantedHost;


  // Get System Preferences Lock
  SCPreferencesRef prefsRef = SCPreferencesCreate(NULL, CFSTR("org.getlantern.lantern"), NULL);

  if(prefsRef==NULL) {
    NSLog(@"Fail to obtain Preferences Ref");
    ret = NO_PERMISSION;
    goto freePrefsRef;
  }

  success = SCPreferencesLock(prefsRef, true);
  if (!success) {
    NSLog(@"Fail to obtain PreferencesLock");
    ret = NO_PERMISSION;
    goto freePrefsRef;
  }

  // Get available network services
  networkSetRef = SCNetworkSetCopyCurrent(prefsRef);
  if(networkSetRef == NULL) {
    NSLog(@"Fail to get available network services");
    ret = SYSCALL_FAILED;
    goto freeNetworkSetRef;
  }

  //Look up interface entry
  networkServicesArrayRef = SCNetworkSetCopyServices(networkSetRef);
  networkServiceRef = NULL;
  for (long i = 0; i < CFArrayGetCount(networkServicesArrayRef); i++) {
    networkServiceRef = CFArrayGetValueAtIndex(networkServicesArrayRef, i);

    // Get proxy protocol
    proxyProtocolRef = SCNetworkServiceCopyProtocol(networkServiceRef, kSCNetworkProtocolTypeProxies);
    if(proxyProtocolRef == NULL) {
      NSLog(@"Couldn't acquire copy of proxyProtocol");
      ret = SYSCALL_FAILED;
      goto freeProxyProtocolRef;
    }

    oldPreferences = (__bridge NSDictionary*)SCNetworkProtocolGetConfiguration(proxyProtocolRef);
    newPreferences = [NSMutableDictionary dictionaryWithDictionary: oldPreferences];
    wantedHost = @"localhost";

    if(turnOn == true) {
      [newPreferences setValue: wantedHost forKey:(NSString*)kSCPropNetProxiesHTTPProxy];
      [newPreferences setValue:[NSNumber numberWithInt:1] forKey:(NSString*)kSCPropNetProxiesProxyAutoConfigEnable];
      [newPreferences setValue:nsPacUrl forKey:(NSString*)kSCPropNetProxiesProxyAutoConfigURLString];
    } else {
      [newPreferences setValue:[NSNumber numberWithInt:0] forKey:(NSString*)kSCPropNetProxiesProxyAutoConfigEnable];
    }

    success = SCNetworkProtocolSetConfiguration(proxyProtocolRef, (__bridge CFDictionaryRef)newPreferences);
    if(!success) {
      NSLog(@"Failed to set Protocol Configuration");
      ret = SYSCALL_FAILED;
      goto freeProxyProtocolRef;
    }

freeProxyProtocolRef:
    CFRelease(proxyProtocolRef);
  }

  success = SCPreferencesCommitChanges(prefsRef);
  if(!success) {
    NSLog(@"Failed to Commit Changes");
    ret = SYSCALL_FAILED;
    goto freeNetworkServicesArrayRef;
  }

  success = SCPreferencesApplyChanges(prefsRef);
  if(!success) {
    NSLog(@"Failed to Apply Changes");
    ret = SYSCALL_FAILED;
    goto freeNetworkServicesArrayRef;
  }
  success = true;

  //Free Resources
freeNetworkServicesArrayRef:
  CFRelease(networkServicesArrayRef);
freeNetworkSetRef:
  CFRelease(networkSetRef);
freePrefsRef:
  SCPreferencesUnlock(prefsRef);
  CFRelease(prefsRef);

  return ret;
}
