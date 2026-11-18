#pragma once
#include <sys/types.h>
#include <cstddef>
struct AAssetManager;
struct AAsset;
enum { AASSET_MODE_UNKNOWN = 0, AASSET_MODE_RANDOM = 1, AASSET_MODE_STREAMING = 2, AASSET_MODE_BUFFER = 3 };
AAsset *AAssetManager_open(AAssetManager *mgr, const char *filename, int mode);
off_t AAsset_getLength(AAsset *asset);
int AAsset_read(AAsset *asset, void *buf, size_t count);
void AAsset_close(AAsset *asset);
