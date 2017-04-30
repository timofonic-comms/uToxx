#include "main.h"

#include "../macros.h"

#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <audioclient.h>
#include <mmdeviceapi.h>

// REFERENCE_TIME time units per second and per millisecond
#define REFTIMES_PER_SEC 10000000
#define REFTIMES_PER_MILLISEC 10000

#define EXIT_ON_ERROR(hres) \
    if (FAILED(hres)) {     \
        goto Exit;          \
    }

#define SAFE_RELEASE(punk)             \
    if ((punk) != NULL) {              \
        (punk)->lpVtbl->Release(punk); \
        (punk) = NULL;                 \
    }

IAudioClient *       pAudioClient   = NULL;
IAudioCaptureClient *pCaptureClient = NULL;
WAVEFORMATEX *       pwfx           = NULL;

const GUID IID_IAudioCaptureClient_utox = { 0xc8adbd64, 0xe71e, 0x48a0, { 0xa4, 0xde, 0x18, 0x5c, 0x39, 0x5c, 0xd3, 0x17 } };


/* note: only works when loopback is 48khz 2 channel floating*/
void audio_detect(void) {
    HRESULT        hr;
    REFERENCE_TIME hnsRequestedDuration = REFTIMES_PER_SEC;
    // REFERENCE_TIME hnsActualDuration;
    UINT32               bufferFrameCount;
    IMMDeviceEnumerator *pEnumerator       = NULL;
    IMMDevice *          pDevice           = NULL;
    IMMDeviceCollection *pDeviceCollection = NULL;
    // BOOL bDone = FALSE;
    UINT count;
    // HANDLE hEvent = NULL;

    CoInitialize(NULL);

    hr = CoCreateInstance(&CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL, &IID_IMMDeviceEnumerator, (void **)&pEnumerator);
    EXIT_ON_ERROR(hr)

    hr = pEnumerator->lpVtbl->EnumAudioEndpoints(pEnumerator, eAll, DEVICE_STATE_ACTIVE, &pDeviceCollection);
    EXIT_ON_ERROR(hr)

    hr = pDeviceCollection->lpVtbl->GetCount(pDeviceCollection, &count);
    EXIT_ON_ERROR(hr)


    hr = pEnumerator->lpVtbl->GetDefaultAudioEndpoint(pEnumerator, eRender, eConsole, &pDevice);
    EXIT_ON_ERROR(hr)

    hr = pDevice->lpVtbl->Activate(pDevice, &IID_IAudioClient, CLSCTX_ALL, NULL, (void **)&pAudioClient);
    EXIT_ON_ERROR(hr)

    hr = pAudioClient->lpVtbl->GetMixFormat(pAudioClient, &pwfx);
    EXIT_ON_ERROR(hr)


    if (pwfx->nSamplesPerSec != 48000 || pwfx->nChannels != 2 || pwfx->wFormatTag != WAVE_FORMAT_EXTENSIBLE) {
                goto Exit;
    }

    WAVEFORMATEXTENSIBLE *wfx = (void *)pwfx;
    if (memcmp(&KSDATAFORMAT_SUBTYPE_IEEE_FLOAT, &wfx->SubFormat, sizeof(wfx->SubFormat)) != 0) {
        goto Exit;
    }

    hr = pAudioClient->lpVtbl->Initialize(pAudioClient, AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK,
                                          hnsRequestedDuration, 0, pwfx, NULL);
    EXIT_ON_ERROR(hr)

    // Get the size of the allocated buffer.
    hr = pAudioClient->lpVtbl->GetBufferSize(pAudioClient, &bufferFrameCount);
    EXIT_ON_ERROR(hr)

    hr = pAudioClient->lpVtbl->GetService(pAudioClient, &IID_IAudioCaptureClient_utox, (void **)&pCaptureClient);
    EXIT_ON_ERROR(hr)

    return;

Exit:
    CoTaskMemFree(pwfx);
    SAFE_RELEASE(pEnumerator)
    SAFE_RELEASE(pDevice)
    SAFE_RELEASE(pAudioClient)
    SAFE_RELEASE(pCaptureClient)

    }

bool audio_init(void *UNUSED(handle)) {
    return SUCCEEDED(pAudioClient->lpVtbl->Start(pAudioClient));
}

bool audio_close(void *UNUSED(handle)) {
    return SUCCEEDED(pAudioClient->lpVtbl->Stop(pAudioClient));
}

static void *convertsamples(int16_t *dest, float *src, uint16_t samples) {
    if (!src) {
        memset(dest, 0, samples * 2);
        return NULL;
    }

    for (uint16_t i = 0; i != samples; i++) {
        float x = *src++;
        const float y = *src++;

        x = (x + y) * INT16_MAX / 2.0;

        if (x > INT16_MAX) {
            x = INT16_MAX;
        } else if (x < INT16_MIN) {
            x = INT16_MIN;
        }
        int16_t v = lrintf(x);
        *dest++   = v; // x;
    }

    return src;
}

bool audio_frame(int16_t *buffer) {
    UINT32 numFramesAvailable;
    UINT32 packetLength = 0;
    BYTE * pData;
    DWORD  flags;

    pCaptureClient->lpVtbl->GetNextPacketSize(pCaptureClient, &packetLength);


    while (packetLength != 0) {
        // Get the available data in the shared buffer.
        pCaptureClient->lpVtbl->GetBuffer(pCaptureClient, &pData, &numFramesAvailable, &flags, NULL, NULL);

        if (flags & AUDCLNT_BUFFERFLAGS_SILENT) {
            pData = NULL; // Tell CopyData to write silence.
        }

        static bool frame = true;

        convertsamples(&buffer[frame ? 0 : 480], (void *)pData, 480);

        frame = !frame;

        pCaptureClient->lpVtbl->ReleaseBuffer(pCaptureClient, numFramesAvailable);

        if (frame) {
            return true;
        }

        pCaptureClient->lpVtbl->GetNextPacketSize(pCaptureClient, &packetLength);
    }

    return false;
}
