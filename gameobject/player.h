#pragma once

#include	<cstdint>
#include	<string>
#include	"gameobject.h"
#include	"../system/IScene.h"

class CStaticMeshRenderer;
class CStaticMesh;

class player : public gameobject {
	//	
	/* 本来のコード
	static constexpr float VALUE_MOVE_MODEL = 2.0f;				// 移動量
	static constexpr float VALUE_ROTATE_MODEL = PI * 0.02f;		// 回転量
	static constexpr float RATE_ROTATE_MODEL = 0.4f;			// 回転割合
	static constexpr float RATE_MOVE_MODEL = 0.2f;					// 移動減衰割合
	*/
	// debug用
public:
	inline static float VALUE_MOVE_MODEL = 2.0f;				// 移動量
	inline static float VALUE_ROTATE_MODEL = PI * 0.002f;		// 回転量
	inline static float RATE_ROTATE_MODEL = 0.4f;				// 回転割合	
	inline static float RATE_MOVE_MODEL = 0.2f;					// 移動減衰割合

	// 壁との当たり判定用
	static constexpr float	COLLISION_RADIUS = 20.0f;	// 当たり判定の球の半径
	static constexpr int	MAX_WALL_RESOLVE = 4;		// 壁の押し出しを繰り返す最大回数
	static constexpr float	WALL_EPSILON = 0.0001f;		// めり込みとみなす最小量（長さの2乗）
public:
	// ISceneのポインタを受け取るコンストラクタを追加
	player(SceneBase* scene) :gameobject(scene) {}

	void update(uint64_t delta) override;
	void draw(uint64_t delta) override;
	void init() override;
	void dispose() override;

	void setmeshid(const std::string& id) { m_meshid = id; }

private:
	CStaticMeshRenderer* m_meshrenderer = nullptr;
	CStaticMesh* m_mesh = nullptr;

	std::string m_meshid{};

	Vector3	m_move{};
	Vector3	m_destrot{};

	Vector3	m_crosspoint{};			// 壁との交点（デバッグ表示用）
	Vector3	m_penetration{};		// めり込みベクトル（デバッグ表示用）
	bool	m_ishitwall = false;	// 壁に当たったかどうか


};