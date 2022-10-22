#include <windows.h>
#include <stdint.h>

static bool running;
static BITMAPINFO bitmapInfo;
static VOID *bitmapMemory;
static int bitmapWidth;
static int bitmapHeight;
static int bytesPerPixel = 4;

static void RenderGradient(int xoffset, int yoffset) {
  int pitch = bitmapWidth * bytesPerPixel;
  uint8_t *row = (uint8_t *) bitmapMemory;
  for(int y = 0; y < bitmapHeight; ++y) {
      uint32_t *pixel = (uint32_t *) row;
      for(int x = 0; x < bitmapWidth; ++x) {
        uint8_t blue = (x + xoffset);
        uint8_t green = (y + yoffset);
        uint8_t red = (xoffset + yoffset);

        *pixel++ = (red << 16) | (green << 8) | (blue);
      }
      row += pitch;
  }
}

static void ResizeDIBSection(int width, int height) {

  if(bitmapMemory) {
    VirtualFree(bitmapMemory, NULL, MEM_RELEASE);
  }

  bitmapInfo.bmiHeader.biSize = sizeof(bitmapInfo.bmiHeader);
  bitmapInfo.bmiHeader.biWidth = width;
  bitmapInfo.bmiHeader.biHeight = height;
  bitmapInfo.bmiHeader.biPlanes = 1;
  bitmapInfo.bmiHeader.biBitCount = 32;
  bitmapInfo.bmiHeader.biCompression = BI_RGB;

  int bitmapMemSize = bytesPerPixel * width * height;
  bitmapMemory = VirtualAlloc(
    NULL, 
    bitmapMemSize,
    MEM_COMMIT,
    PAGE_READWRITE
  );

  bitmapWidth = width;
  bitmapHeight = height;

  RenderGradient(0, 0);
}

static void UpdateDrawWindow(HDC context, RECT *windowRect, int x, int y, int width, int height) {
  StretchDIBits(
    context,
    0, 0, bitmapWidth, bitmapHeight,
    0, 0, windowRect->right - windowRect->left, windowRect->bottom - windowRect->top,
    bitmapMemory, &bitmapInfo,
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
      RECT clientRect;
      GetClientRect(Window, &clientRect);
      ResizeDIBSection(
        clientRect.right - clientRect.left, 
        clientRect.bottom - clientRect.top
      );
      OutputDebugString("WM_SIZE\n");
    } break;
    case WM_DESTROY:
    {
      running = false;
      OutputDebugString("WM_DESTROY\n");
    } break;
    case WM_CLOSE:
    {
      running = false;
      OutputDebugString("WM_CLOSE\n");
    } break;
    case WM_ACTIVATEAPP:
    {
      OutputDebugString("WM_ACTIVATEAPP\n");
    } break;
    case WM_PAINT:
    {
      PAINTSTRUCT paint;
      HDC deviceContext = BeginPaint(Window, &paint);
      RECT clientRect;
      GetClientRect(Window, &clientRect);
      UpdateDrawWindow(
        deviceContext, 
        &clientRect,
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
    WNDCLASSEX WindowClass = {};
    WindowClass.cbSize = sizeof(WNDCLASSEX);
    WindowClass.style = CS_OWNDC|CS_VREDRAW|CS_HREDRAW;
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
        MSG message;
        BOOL result;

        running = true;

        int xoffset = 0;
        int yoffset = 0;

        while(running) {
          while(PeekMessage(&message, 0, 0, 0, PM_REMOVE)) {
            if (message.message == WM_QUIT) {
              running = false;
            } else {
              TranslateMessage(&message);
              DispatchMessage(&message);
            }
          }

          RenderGradient(xoffset, yoffset);

          HDC deviceContext = GetDC(WindowHandle);
          RECT clientRect;
          GetClientRect(WindowHandle, &clientRect);
          UpdateDrawWindow(
            deviceContext, 
            &clientRect,
            0, 
            0, 
            clientRect.right - clientRect.left,
            clientRect.bottom - clientRect.top
          );
          ReleaseDC(WindowHandle, deviceContext);

          ++xoffset;
          ++yoffset;
        }
      } else {
        // Handle failed
      }
    } else {
      // TODO: Window Failed to register
    }

    return 0;
}