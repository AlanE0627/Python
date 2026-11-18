// Silent BASS stub for desktop test builds (audio is verified on device only).
#include "bass.h"
extern "C" {
BOOL BASSDEF(BASS_ChannelPlay)(DWORD, BOOL) { return TRUE; }
BOOL BASSDEF(BASS_ChannelStop)(DWORD) { return TRUE; }
int BASSDEF(BASS_ErrorGetCode)(void) { return 0; }
BOOL BASSDEF(BASS_Free)(void) { return TRUE; }
BOOL BASSDEF(BASS_Init)(int, DWORD, DWORD, void *, const void *) { return TRUE; }
DWORD BASSDEF(BASS_SampleGetChannel)(HSAMPLE, DWORD) { return 1; }
HSAMPLE BASSDEF(BASS_SampleLoad)(BOOL, const void *, QWORD, DWORD, DWORD, DWORD) { return 1; }
BOOL BASSDEF(BASS_SetConfig)(DWORD, DWORD) { return TRUE; }
HSTREAM BASSDEF(BASS_StreamCreateFile)(BOOL, const void *, QWORD, QWORD, DWORD) { return 1; }
BOOL BASSDEF(BASS_StreamFree)(HSTREAM) { return TRUE; }
}
