#include <windows.h>
#include <stdint.h>
#include <xinput.h>

struct offscreen_buffer {
  BITMAPINFO info;
  VOID *memory;
  int width;
  int height;
  int pitch;
  int bytesPerPixel;
};

struct window_dimension {
  int width;
  int height; 
};

// Manually load XInput functions
#define X_INPUT_GET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_STATE* pState)
#define X_INPUT_SET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_VIBRATION* pVibration)
typedef X_INPUT_GET_STATE(x_input_get_state);
typedef X_INPUT_SET_STATE(x_input_set_state);
X_INPUT_GET_STATE(XInputGetStateStub) { return 0; }
X_INPUT_SET_STATE(XInputSetStateStub) { return 0; }
static x_input_get_state *XInputGetState_;
static x_input_set_state *XInputSetState_;
#define XInputGetState XInputGetState_
#define XInputSetState XInputSetState_

static void loadXInput() {
  HMODULE lib = LoadLibraryA("xinput1_3.dll");
  if(lib) {
    XInputGetState = (x_input_get_state *) GetProcAddress(lib, "XInputGetState");
    XInputSetState = (x_input_set_state *) GetProcAddress(lib, "XInputSetState");
  }
}

static window_dimension getWindowDimension(HWND Window) {
  window_dimension out;

  RECT clientRect;
  GetClientRect(Window, &clientRect);
  out.width = clientRect.right - clientRect.left;
  out.height = clientRect.bottom - clientRect.top;

  return(out);
}

static bool running;
static offscreen_buffer globalbackbuffer;

static void RenderGradient(offscreen_buffer *buffer, int xoffset, int yoffset) {
  uint8_t *row = (uint8_t *) buffer->memory;
  for(int y = 0; y < buffer->height; ++y) {
      uint32_t *pixel = (uint32_t *) row;
      for(int x = 0; x < buffer->width; ++x) {
        uint8_t blue = (x + xoffset);
        uint8_t green = (y + yoffset);
        uint8_t red = (xoffset + yoffset);

        *pixel++ = (red << 16) | (green << 8) | (blue);
      }
      row += buffer->pitch;
  }
}

static void ResizeDIBSection(offscreen_buffer *buffer, int width, int height) {

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
  buffer->bytesPerPixel = 4;

  int bitmapMemSize = buffer->bytesPerPixel * width * height;
  buffer->memory = VirtualAlloc(
    NULL, 
    bitmapMemSize,
    MEM_COMMIT,
    PAGE_READWRITE
  );

  buffer->pitch = buffer->width * buffer->bytesPerPixel;

  RenderGradient(buffer, 0, 0);
}

static void CopyBufferToWindow(HDC context, int winWidth, int winHeight, offscreen_buffer *buffer, int x, int y, int width, int height) {
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
      window_dimension dim = getWindowDimension(Window);
      CopyBufferToWindow(
        deviceContext, 
        dim.width, dim.height,
        &globalbackbuffer,
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
    loadXInput();

    WNDCLASSEX WindowClass = {};

    ResizeDIBSection(&globalbackbuffer, 1280, 720);

    WindowClass.cbSize = sizeof(WNDCLASSEX);
    WindowClass.style = CS_VREDRAW|CS_HREDRAW;
    WindowClass.lpfnWndProc = MainWindowCallback;
    WindowClass.hInstance = hInstance;
    WindowClass.lpszClassName = "RaikaWindow";

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
      if(WindowHandle) {
        running = true;

        int xoffset = 0;
        int yoffset = 0;

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
          for(DWORD i = 0; i < XUSER_MAX_COUNT; ++i) {
            XINPUT_STATE state;
            if(XInputGetState(i, &state) == ERROR_SUCCESS) {
              // available
              XINPUT_GAMEPAD *pad = &state.Gamepad;
              bool dpadUp = (pad->wButtons & XINPUT_GAMEPAD_DPAD_UP);
              bool dpadDown = (pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN);
              bool dpadLeft = (pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT);
              bool dpadRight = (pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT);
              bool startButton = (pad->wButtons & XINPUT_GAMEPAD_START);
              bool backButton = (pad->wButtons & XINPUT_GAMEPAD_BACK);
              bool leftThumb = (pad->wButtons & XINPUT_GAMEPAD_LEFT_THUMB);
              bool rightThumb = (pad->wButtons & XINPUT_GAMEPAD_RIGHT_THUMB);
              bool leftShoulder = (pad->wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER);
              bool rightShoulder = (pad->wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER);
              bool aButton = (pad->wButtons & XINPUT_GAMEPAD_A);
              bool bButton = (pad->wButtons & XINPUT_GAMEPAD_B);
              bool xButton = (pad->wButtons & XINPUT_GAMEPAD_X);
              bool yButton = (pad->wButtons & XINPUT_GAMEPAD_Y);

              uint8_t lTrigger = pad->bLeftTrigger;
              uint8_t rTrigger = pad->bRightTrigger;

              int16_t lStickX = pad->sThumbLX;
              int16_t lStickY = pad->sThumbLY;
              int16_t rStickX = pad->sThumbRX;
              int16_t rStickY = pad->sThumbRY;

              if(aButton) {
                yoffset += 1;
              }
              if(bButton) {
                yoffset -= 1;
              }
              if(xButton) {
                XINPUT_VIBRATION vibrationData;
                vibrationData.wLeftMotorSpeed = 20000;
                vibrationData.wRightMotorSpeed = 20000;
                XInputSetState(i, &vibrationData);
              }
            } else {
              // not available
            }
          }

          RenderGradient(&globalbackbuffer, xoffset, yoffset);

          HDC deviceContext = GetDC(WindowHandle);
          window_dimension dim = getWindowDimension(WindowHandle);
          CopyBufferToWindow(
            deviceContext, 
            dim.width, dim.height,
            &globalbackbuffer,
            0, 
            0, 
            dim.width,
            dim.height
          );
          ReleaseDC(WindowHandle, deviceContext);

          ++xoffset;
        }
      } else {
        // Handle failed
      }
    } else {
      // TODO: Window Failed to register
    }

    return 0;
}