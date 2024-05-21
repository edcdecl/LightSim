#include "renderui.hpp"
#include "rendercontroller.hpp"

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
    SetProcessDPIAware();
    HWND hRenderUIWnd;
    auto hThreadRenderUI = RenderUI::Create(hInstance, hRenderUIWnd);

    HWND hRenderControllerWnd;
    auto hThreadRenderController = RenderController::Create(hInstance, hRenderControllerWnd);

    RenderUI::AttachController(hRenderUIWnd, hRenderControllerWnd);

    HANDLE handles[2] = {hThreadRenderUI, hThreadRenderController};
    auto wfmor = WaitForMultipleObjects(2, handles, FALSE, INFINITE);
    if (wfmor == WAIT_OBJECT_0) {
        PostMessageW(hRenderControllerWnd, WM_DESTROY, 0, 0);
        WaitForSingleObject(hThreadRenderController, INFINITE);
    } else if (wfmor == WAIT_OBJECT_0 + 1) {
        PostMessageW(hRenderUIWnd, WM_DESTROY, 0, 0);
        WaitForSingleObject(hThreadRenderUI, INFINITE);
    }
    CloseHandle(hThreadRenderUI);
    CloseHandle(hThreadRenderController);
    return 0;
}
