#include	<iostream>
#include	<cstdint>
#include	"application.h"
#include	"system/renderer.h"
#include    "system/DebugUI.h"
#include    "system/CDirectInput.h"
#include	"system/scenemanager.h"
#include	"fpscontrol.h"
#include	"system/inputmanager.h"

#include <array>
#include <filesystem>
#include <string_view>
#include <shellapi.h>
#pragma comment(lib, "shell32.lib")

namespace {
void SetResourceDirectory() {
    std::array<wchar_t, 32768> executable{};
    GetModuleFileNameW(nullptr, executable.data(), static_cast<DWORD>(executable.size()));
    for (auto candidate : {std::filesystem::current_path(), std::filesystem::path(executable.data()).parent_path()}) {
        for (int level = 0; level < 8; ++level) {
            std::error_code error;
            if (std::filesystem::exists(candidate / "shader/common.hlsl", error)) {
                std::filesystem::current_path(candidate, error);
                return;
            }
            const auto parent = candidate.parent_path();
            if (parent == candidate || parent.empty()) break;
            candidate = parent;
        }
    }
}

bool EditorRequested() {
    int count = 0;
    LPWSTR* arguments = CommandLineToArgvW(GetCommandLineW(), &count);
    bool editor = false;
    if (arguments) {
        for (int i = 1; i < count; ++i) editor |= std::wstring_view(arguments[i]) == L"--editor";
        LocalFree(arguments);
    }
    return editor;
}
}

void gameinit()
{
    SetResourceDirectory();
	// レンダラの初期化    
	Renderer::Init();

	// DirectInputの初期化
	CDirectInput::GetInstance().Init(Application::GetHInstance(), 
		Application::GetWindow(),
		Application::GetWidth(),
		Application::GetHeight());

	// デバッグUIの初期化
	DebugUI::Init(Renderer::GetDevice(), Renderer::GetDeviceContext());

	// シーンマネージャの初期化
	SceneManager::Init();

	//　シーン選択
	SceneManager::SetCurrentScene(EditorRequested() ? "MotionEditorScene" : "Game3DScene");

}

void gameupdate(uint64_t deltatime)
{

	CInputManager::GetInstance().Update();

		// シーンマネージャの更新
	SceneManager::Update(deltatime);
}

void gamedraw(uint64_t deltatime) 
{
	// レンダリング前処理
	Renderer::Begin();

	// シーンマネージャの描画
	SceneManager::Draw(deltatime);

	// デバッグUIの描画
	DebugUI::Render();

	// レンダリング後処理
	Renderer::End();
}

void gamedispose() 
{
	// デバッグUIの終了処理
	DebugUI::DisposeUI();

	// シーンマネージャの終了処理
	SceneManager::Dispose();

	// レンダラの終了処理
	Renderer::Dispose();

}

void gameloop()
{
	// 60FPSで動かす
	static FPS fpsrate(65);

	// 前フレームからの経過時間を取得
	uint64_t delta_time = fpsrate.BeginFrame();

//	std::cout << delta_time << std::endl;

	// 更新処理
	gameupdate(delta_time);

	// 描画処理
	gamedraw(delta_time);

	// 60FPSになるように待機
	fpsrate.EndFrame();
}