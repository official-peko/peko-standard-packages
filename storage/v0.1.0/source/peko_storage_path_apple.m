/*
 * peko_storage_path_apple.m
 * Per-app data directory resolver for macOS and iOS.
 * Returns a directory under the user Application Support area, named by the
 * application identity. The directory tree is created when it is absent.
 */

#import <Foundation/Foundation.h>
#include <stdlib.h>
#include <string.h>

/* Copies a C string into a fresh malloc buffer. */
static char *dup_path(const char *s)
{
    if (!s) {
        return NULL;
    }
    size_t n = strlen(s) + 1;
    char *c = (char *)malloc(n);
    if (c) {
        memcpy(c, s, n);
    }
    return c;
}

/* Resolves the per-app data directory and creates it when needed.
 * app_id selects the directory name. An empty app_id falls back to the main
 * bundle identifier, then to a fixed default. Returns a malloc'd path or NULL
 * on failure. */
char *peko_storage_data_dir(const char *app_id)
{
    @autoreleasepool {
        NSFileManager *fm = [NSFileManager defaultManager];
        NSArray<NSURL *> *urls =
            [fm URLsForDirectory:NSApplicationSupportDirectory
                       inDomains:NSUserDomainMask];
        if (urls.count == 0) {
            return NULL;
        }
        NSURL *base = urls[0];

        NSString *ident = nil;
        if (app_id && app_id[0] != '\0') {
            ident = [NSString stringWithUTF8String:app_id];
        }
        if (ident == nil) {
            ident = [[NSBundle mainBundle] bundleIdentifier];
        }
        if (ident == nil) {
            ident = @"peko.app";
        }

        NSURL *dir = [base URLByAppendingPathComponent:ident isDirectory:YES];

        NSError *err = nil;
        BOOL ok = [fm createDirectoryAtURL:dir
              withIntermediateDirectories:YES
                               attributes:nil
                                    error:&err];
        if (!ok) {
            return NULL;
        }

        const char *path = [dir fileSystemRepresentation];
        if (!path) {
            return NULL;
        }
        return dup_path(path);
    }
}
