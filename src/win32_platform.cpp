#include "raika.cpp"

#include <windows.h>
#include <stdint.h>
#include <xinput.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <comdef.h>

#define SAMPLES_PER_SECOND 48000
#define FPS 30
#define CONTROLLER_MAX 4

// COM result check
#define RETURN_IF_FAILED(com_call) {HRESULT hr; if(FAILED(hr = (com_call))) { _com_error err(hr); OutputDebugString(err.ErrorMessage()); return hr; }}


struct win32_graphics_buffer {
  BITMAPINFO info;
  VOID *memory;
  int width;
  int height;
  int pitch;
};

struct win32_window_dimension {
  int width;
  int height; 
};

struct win32_audio_client {
  UINT32 bufferSize;
  UINT32 sampleBytes;
  int bitDepth; 
  int channels;
  IAudioClient *audioClient;
  IAudioRenderClient *renderClient;
  IAudioClock *audioClock;
  ISimpleAudioVolume *audioVolume;
};

// globals
static bool running;
static win32_graphics_buffer globalGraphicsBuffer;
static win32_audio_client globalAudioClient; 

// Manually load XInput functions
#define X_INPUT_GET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_STATE* pState)
#define X_INPUT_SET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_VIBRATION* pVibration)
typedef X_INPUT_GET_STATE(x_input_get_state);
typedef X_INPUT_SET_STATE(x_input_set_state);
X_INPUT_GET_STATE(XInputGetStateStub) { return ERROR_NOT_SUPPORTED; }
X_INPUT_SET_STATE(XInputSetStateStub) { return ERROR_NOT_SUPPORTED; }
static x_input_get_state *XInputGetState_;
static x_input_set_state *XInputSetState_;
#define XInputGetState XInputGetState_
#define XInputSetState XInputSetState_

static void LoadXInput() {
  HMODULE lib = LoadLibraryA("xinput1_4.dll");
  if(!lib) {
    lib = LoadLibraryA("xinput1_3.dll");
  }
  if(lib) {
    XInputGetState = (x_input_get_state *) GetProcAddress(lib, "XInputGetState");
    XInputSetState = (x_input_set_state *) GetProcAddress(lib, "XInputSetState");
  }
}

static HRESULT InitAudio(
  REFERENCE_TIME bufferDuration, 
  int32_t samplesPerSec
) {
  IMMDeviceEnumerator *deviceEnumerator = {};
  IMMDevice *mmDevice;
  globalAudioClient = {};

  // Obtain Device Enumerator
  RETURN_IF_FAILED(CoCreateInstance(
    __uuidof(MMDeviceEnumerator), NULL,
    CLSCTX_ALL, IID_PPV_ARGS(&deviceEnumerator)
  ));
  // Obtain Device
  // Just use the default one:
  RETURN_IF_FAILED(deviceEnumerator->GetDefaultAudioEndpoint(
    eRender, eConsole, &mmDevice
  ));
  // Obtain Audio Client
  RETURN_IF_FAILED(mmDevice->Activate(
    __uuidof(IAudioClient),
    CLSCTX_ALL,
    NULL,
    (void **) &(globalAudioClient.audioClient)
  ));
  // Initialize Audio Client
  // First we need a Wave Format
  WAVEFORMATEX waveFormatPrimary = {};
  waveFormatPrimary.wFormatTag = WAVE_FORMAT_PCM;
  waveFormatPrimary.nChannels = 2;
  waveFormatPrimary.nSamplesPerSec = samplesPerSec;
  waveFormatPrimary.wBitsPerSample = 16;
  waveFormatPrimary.nBlockAlign = (waveFormatPrimary.nChannels * waveFormatPrimary.wBitsPerSample) / 8;
  waveFormatPrimary.nAvgBytesPerSec = waveFormatPrimary.nSamplesPerSec * waveFormatPrimary.nBlockAlign;
  waveFormatPrimary.cbSize = 0;
  // See if this format is valid
  WAVEFORMATEX *waveFormatClosest = NULL;
  // RETURN_IF_FAILED(globalAudioClient.audioClient->IsFormatSupported(
  //   AUDCLNT_SHAREMODE_SHARED,
  //   &waveFormatPrimary,
  //   &waveFormatClosest
  // ));
  if(waveFormatClosest == NULL) {
    waveFormatClosest = &waveFormatPrimary;
  }
  // Throw that into the client
  RETURN_IF_FAILED(globalAudioClient.audioClient->Initialize(
    AUDCLNT_SHAREMODE_SHARED,
    AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM, bufferDuration, 0,
    waveFormatClosest, NULL
  ));
  // Obtain Render Client
  RETURN_IF_FAILED(globalAudioClient.audioClient->GetService(
    __uuidof(IAudioRenderClient), (void **) &globalAudioClient.renderClient
  ));
  // Obtain Audio Clock
  RETURN_IF_FAILED(globalAudioClient.audioClient->GetService(
    __uuidof(IAudioClock), (void **) &globalAudioClient.audioClock
  ));
  // Write true buffer size
  RETURN_IF_FAILED(globalAudioClient.audioClient->GetBufferSize(
    &globalAudioClient.bufferSize
  ));
  // Write silence data to buffer
  BYTE *data;
  RETURN_IF_FAILED(globalAudioClient.renderClient->GetBuffer(
    globalAudioClient.bufferSize, &data
  ));
  // We don't actually have to write anything, just use the flag
  RETURN_IF_FAILED(globalAudioClient.renderClient->ReleaseBuffer(
    globalAudioClient.bufferSize, AUDCLNT_BUFFERFLAGS_SILENT
  ));
  // Update Bytes per Sample (reuse waveformat)
  globalAudioClient.sampleBytes = (waveFormatClosest->nChannels * waveFormatClosest->wBitsPerSample) / 8;
  globalAudioClient.bitDepth = waveFormatClosest->wBitsPerSample;
  globalAudioClient.channels = waveFormatClosest->nChannels;
  // Start the stream!
  RETURN_IF_FAILED(globalAudioClient.audioClient->Start());
  // Print volume
  RETURN_IF_FAILED(globalAudioClient.audioClient->GetService(
    __uuidof(ISimpleAudioVolume), (void **) &globalAudioClient.audioVolume
  ));

  return EXIT_SUCCESS;
}

static HRESULT MakeAudioBuffer(sound_buffer *buffer) {
  UINT32 currentlyWrittenFrames;
  RETURN_IF_FAILED(
    globalAudioClient.audioClient->GetCurrentPadding(&currentlyWrittenFrames)
  );
  buffer->samplesRequested = globalAudioClient.bufferSize - currentlyWrittenFrames;
  buffer->samplesPerSecond = SAMPLES_PER_SECOND;
  buffer->bytesPerSample = globalAudioClient.bitDepth / 8;
  buffer->channels = globalAudioClient.channels;
  // buffer->samplesRequested = buffer->samplesPerSecond / FPS;
  RETURN_IF_FAILED(
    globalAudioClient.renderClient->GetBuffer(buffer->samplesRequested, (BYTE **) &(buffer->memory))
  );
  return EXIT_SUCCESS;
}

static win32_window_dimension GetWindowDimension(
  HWND Window
) {
  win32_window_dimension out;

  RECT clientRect;
  GetClientRect(Window, &clientRect);
  out.width = clientRect.right - clientRect.left;
  out.height = clientRect.bottom - clientRect.top;

  return(out);
}

static void ResizeDIBSection(
  win32_graphics_buffer *buffer, 
  int width, 
  int height
) {

  if(buffer->memory) {
    VirtualFree(buffer->memory, NULL, MEM_RELEASE);
  }

  buffer->info.bmiHeader.biSize = sizeof(buffer->info.bmiHeader);
  buffer->info.bmiHeader.biWidth = width;
  buffer->info.bmiHeader.biHeight = height;
  buffer->info.bmiHeader.biPlanes = 1;
  buffer->info.bmiHeader.biBitCount = 32;
  buffer->info.bmiHeader.biCompression = BI_RGB;

  buffer->width = width;
  buffer->height = height;

  int bytesPerPixel = 4;
  int bitmapMemSize = bytesPerPixel * width * height;
  buffer->memory = VirtualAlloc(
    NULL, 
    bitmapMemSize,
    MEM_COMMIT,
    PAGE_READWRITE
  );

  buffer->pitch = buffer->width * bytesPerPixel;
}

static void CopyBufferToWindow(
  HDC context, 
  int winWidth, 
  int winHeight, 
  win32_graphics_buffer *buffer, 
  int x, 
  int y, 
  int width, 
  int height
) {
  StretchDIBits(
    context,
    0, 0, winWidth, winHeight,
    0, 0, buffer->width, buffer->height,
    buffer->memory, &(buffer->info),
    DIB_RGB_COLORS,
    SRCCOPY
  );
}

LRESULT MainWindowCallback(
  HWND Window,
  UINT Message,
  WPARAM wParam,
  LPARAM lParam
) {
  LRESULT result = 0;

  switch(Message) {
    case WM_SIZE:
    {
      
    } break;
    case WM_DESTROY:
    {
      running = false;
    } break;
    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP:
    case WM_KEYDOWN:
    case WM_KEYUP:
    {
      uint32_t keycode = wParam;
      bool wasDown = ((lParam & (1 << 30)) != 0);
      bool isDown = ((lParam & (1 << 31)) == 0);
      // Only check if an interesting change has been made
      if(wasDown != isDown) {
        switch(keycode) {
          case 'W':
          OutputDebugString("W\n");
          break;
          case 'A':
          OutputDebugString("A\n");
          break;
          case 'S':
          OutputDebugString("S\n");
          break;
          case 'D':
          OutputDebugString("D\n");
          break;
        }
        
        bool altPressed = ((lParam & (1 << 29)) != 0);
        if(altPressed && keycode == VK_F4) {
          running = false;
        }
      }
    } break;
    case WM_CLOSE:
    {
      running = false;
    } break;
    case WM_ACTIVATEAPP:
    {
    } break;
    case WM_PAINT:
    {
      PAINTSTRUCT paint;
      HDC deviceContext = BeginPaint(Window, &paint);
      win32_window_dimension dim = GetWindowDimension(Window);
      CopyBufferToWindow(
        deviceContext, 
        dim.width, dim.height,
        &globalGraphicsBuffer,
        paint.rcPaint.left, 
        paint.rcPaint.top, 
        paint.rcPaint.right - paint.rcPaint.left,
        paint.rcPaint.bottom - paint.rcPaint.top
      );
      EndPaint(Window, &paint);
    } break;
    default:
    {
      result = DefWindowProc(Window, Message, wParam, lParam);
    } break;
  }

  return result;
}

int APIENTRY WinMain(
  HINSTANCE hInstance,
  HINSTANCE hPrevInstance,
  LPSTR     lpCmdLine,
  int       nShowCmd
) {
    // dynamically load some stuff
    LoadXInput();

    WNDCLASSEX WindowClass = {};

    ResizeDIBSection(&globalGraphicsBuffer, 1280, 720);

    WindowClass.cbSize = sizeof(WNDCLASSEX);
    WindowClass.style = CS_VREDRAW|CS_HREDRAW;
    WindowClass.lpfnWndProc = MainWindowCallback;
    WindowClass.hInstance = hInstance;
    WindowClass.lpszClassName = "RaikaWindow";

    LARGE_INTEGER perfFrequency;
    QueryPerformanceFrequency(&perfFrequency);

    if(RegisterClassEx(&WindowClass)) {
      HWND WindowHandle = CreateWindowEx(
        0,
        WindowClass.lpszClassName,
        "Raika",
        WS_OVERLAPPEDWINDOW|WS_VISIBLE,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        0,
        0,
        hInstance,
        0
      );

      // Load audio
      RETURN_IF_FAILED(InitAudio(1000000, SAMPLES_PER_SECOND)); // 1s buffer

      if(WindowHandle) {
        running = true;

        LARGE_INTEGER beginCounter, endCounter;
        uint64_t beginTimestamp, endTimestamp;
        QueryPerformanceCounter(&beginCounter);
        beginTimestamp = __rdtsc();

        while(running) { // Running loop
          // Deal with messages

          MSG message;
          
          while(PeekMessage(&message, 0, 0, 0, PM_REMOVE)) {
            if (message.message == WM_QUIT) {
              running = false;
            } else {
              TranslateMessage(&message);
              DispatchMessage(&message);
            }
          }

          // Input
          game_input gameInput = {};

          int maxControllers = XUSER_MAX_COUNT;
          if(maxControllers > ArrayCount(gameInput.controllers)) {
            maxControllers = ArrayCount(gameInput.controllers);
          }

          for(DWORD i = 0; i < maxControllers; ++i) {
            XINPUT_STATE state;
            if(XInputGetState(i, &state) == ERROR_SUCCESS) {
              // available
              XINPUT_GAMEPAD *pad = &state.Gamepad;\
              gameInput.controllers[i].buttons[0] = (pad->wButtons & XINPUT_GAMEPAD_A);
              gameInput.controllers[i].buttons[1] = (pad->wButtons & XINPUT_GAMEPAD_B);
              gameInput.controllers[i].buttons[2] = (pad->wButtons & XINPUT_GAMEPAD_X);
              gameInput.controllers[i].buttons[3] = (pad->wButtons & XINPUT_GAMEPAD_Y);
              gameInput.controllers[i].dpad[0] = (pad->wButtons & XINPUT_GAMEPAD_DPAD_UP);
              gameInput.controllers[i].dpad[1] = (pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN);
              gameInput.controllers[i].dpad[2] = (pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT);
              gameInput.controllers[i].dpad[3] = (pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT);
              gameInput.controllers[i].buttons[4] = (pad->wButtons & XINPUT_GAMEPAD_START);
              gameInput.controllers[i].buttons[5] = (pad->wButtons & XINPUT_GAMEPAD_BACK);
              gameInput.controllers[i].buttons[6] = (pad->wButtons & XINPUT_GAMEPAD_LEFT_THUMB);
              gameInput.controllers[i].buttons[7] = (pad->wButtons & XINPUT_GAMEPAD_RIGHT_THUMB);
              gameInput.controllers[i].buttons[8] = (pad->wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER);
              gameInput.controllers[i].buttons[9] = (pad->wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER);

              gameInput.controllers[i].buttons[10] = pad->bLeftTrigger != 0;
              gameInput.controllers[i].buttons[11] = pad->bRightTrigger != 0;

              gameInput.controllers[i].lStickX = pad->sThumbLX < 0 ? (float) pad->sThumbLX / 32768.0f : (float) pad->sThumbLX / 32767.0f;
              gameInput.controllers[i].lStickY = pad->sThumbLY < 0 ? (float) pad->sThumbLY / 32768.0f : (float) pad->sThumbLY / 32767.0f;
              gameInput.controllers[i].rStickX = pad->sThumbRX < 0 ? (float) pad->sThumbRX / 32768.0f : (float) pad->sThumbRX / 32767.0f;
              gameInput.controllers[i].rStickY = pad->sThumbRY < 0 ? (float) pad->sThumbRY / 32768.0f : (float) pad->sThumbRY / 32767.0f;

            } else {
              // not available
            }
          }

          graphics_buffer graphicsBuffer = {};
          graphicsBuffer.memory = globalGraphicsBuffer.memory;
          graphicsBuffer.width = globalGraphicsBuffer.width;
          graphicsBuffer.height = globalGraphicsBuffer.height;
          graphicsBuffer.pitch = globalGraphicsBuffer.pitch;
          
          sound_buffer soundBuffer = {};
          MakeAudioBuffer(&soundBuffer);

          GameUpdateAndRender(&graphicsBuffer, &soundBuffer, &gameInput);
          globalAudioClient.renderClient->ReleaseBuffer(soundBuffer.samplesRequested, 0);

          HDC deviceContext = GetDC(WindowHandle);
          win32_window_dimension dim = GetWindowDimension(WindowHandle);
          CopyBufferToWindow(
            deviceContext, 
            dim.width, dim.height,
            &globalGraphicsBuffer,
            0, 
            0, 
            dim.width,
            dim.height
          );
          ReleaseDC(WindowHandle, deviceContext);

          QueryPerformanceCounter(&endCounter);
          uint64_t counterElapsed = endCounter.QuadPart - beginCounter.QuadPart;
          uint32_t timeElapsed = (counterElapsed * 1000) / perfFrequency.QuadPart; // in ms
          endTimestamp = __rdtsc();
          uint64_t timestampElapsed = endTimestamp - beginTimestamp;
          char Buffer[256];
          wsprintf(Buffer, "ms/frame: %dms, %d\n", timeElapsed, timestampElapsed);
          OutputDebugString(Buffer);
          beginCounter = endCounter;
          beginTimestamp = endTimestamp;
        }
      } else {
        // Handle failed
      }
    } else {
      // TODO: Window Failed to register
    }

    return 0;
}