#include	<iostream>
#include	<cstdint>
#include	"application.h"
#include	"system/renderer.h"
#include    "system/DebugUI.h"
#include    "system/CDirectInput.h"
#include	"system/scenemanager.h"
#include	"fpscontrol.h"
#include	"system/inputmanager.h"

void gameinit() 
{
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
	SceneManager::SetCurrentScene("Game3DScene");

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