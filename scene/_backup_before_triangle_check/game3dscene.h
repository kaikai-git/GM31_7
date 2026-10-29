#pragma once

#include <memory>
#include <cstdint>

#include "../system/commontypes.h"
#include "../system/camera.h"
#include "../system/IScene.h"
#include "../system/SceneClassFactory.h"
#include "../system/DirectWrite.h"
#include "../system/C3DShape.h"
#include "../system/collision.h"

/**
 * @brief Game3Dシーン
 */
class Game3DScene : public SceneBase {
public:

	/// @brief コピーコンストラクタは使用不可
	Game3DScene(const Game3DScene&) = delete;

	/// @brief 代入演算子も使用不可
	Game3DScene& operator=(const Game3DScene&) = delete;

	/**
	 * @brief コンストラクタ
	 */
	explicit Game3DScene();

	/**
	 * @brief 毎フレームの更新処理
	 * @param deltatime 前フレームからの経過時間（マイクロ秒）
	 */
	void update(uint64_t deltatime) override;

	/**
	 * @brief 毎フレームの描画処理
	 * @param deltatime 前フレームからの経過時間（マイクロ秒）
	 *
	 */
	void draw(uint64_t deltatime) override;

	/**
	 * @brief シーンの初期化処理
	 *
	 */
	void init() override;

	/**
	 * @brief シーンの終了処理
	 *
	 */
	void dispose() override;

	void resourceLoader();			// リソースを読み込む

	// フリーカメラのデバッグUI
	void DebugFreeCamera();

	// 平面のデバッグUI
	void DebugPlane();

	// 直線と平面の交点のデバッグUI
	void DebugLine();

private:
	/**
	 * @brief このシーンで使用するカメラ
	 */
	std::unique_ptr<Camera> m_camera;

	// DirectWrite
	std::unique_ptr<DirectWrite> m_directwrite;

	// フォントデータ
	FontData	m_fontdata;

	std::array<std::unique_ptr<Segment>, 3> m_segments;	// 3軸表示用線分

	// 平面表示用
	std::array<Vector3, 3> m_planevertices{};	// 平面を作る3頂点（回転前）
	std::array<Vector3, 3> m_rotatedvertices{};	// 回転後の3頂点
	Vector3                m_planerotation{};	// 平面の回転角度（度）XYZ
	CPlane                 m_plane{};			// 平面の方程式
	bool                   m_planevalid{ false };	// 3点が一直線上にないか

	// 直線表示用
	Vector3                m_linestart{};		// 開始点
	Vector3                m_lineend{};			// 終了点
	float                  m_linet{ 1.0f };		// 媒介変数 t

	// 交点
	bool                   m_hitvalid{ false };	// 交点があるか（平行でないか）
	float                  m_hitt{ 0.0f };		// 交点の媒介変数 t
	Vector3                m_hitpos{};			// 交点の座標
};

REGISTER_CLASS(Game3DScene)
