#include <windows.h>

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
      OutputDebugString("WM_SIZE\n");
    } break;
    case WM_DESTROY:
    {
      OutputDebugString("WM_DESTROY\n");
    } break;
    case WM_CLOSE:
    {
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
      PatBlt(
        deviceContext, 
        paint.rcPaint.left, 
        paint.rcPaint.top, 
        paint.rcPaint.right - paint.rcPaint.left,
        paint.rcPaint.bottom - paint.rcPaint.top,
        BLACKNESS
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
    MessageBoxA(0, "Hello World!", "raika", MB_OK|MB_ICONINFORMATION);

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

        while((result = GetMessage(&message,0,0,0)) != 0) {
          if (result == -1) {
            // Error
          } else {
            TranslateMessage(&message);
            DispatchMessage(&message);
          }
        }
      } else {
        // Handle failed
      }
    } else {
      // TODO: Window Failed to register
    }

    return 0;
}