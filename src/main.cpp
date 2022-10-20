#include <windows.h>

int APIENTRY WinMain(
  HINSTANCE hInstance,
  HINSTANCE hPrevInstance,
  LPSTR     lpCmdLine,
  int       nShowCmd
) {
    MessageBoxA(0, "Hello World!", "raika", MB_OK|MB_ICONINFORMATION);

    return 0;
}