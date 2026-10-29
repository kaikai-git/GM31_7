#pragma once
#include <cstdint>
#include "../system/collision.h"

class gameobject;

/**
 * @brief コリジョンレイヤ
 */
enum class CollisionLayer : uint32_t {
	None = 0,				// 0
	Player = 1 << 0,		// 1
	Enemy = 1 << 1,			// 2
	PlayerBullet = 1 << 2,	// 4
	EnemyBullet = 1 << 3,	// 8
	All = ~0u
};

// BoundingSphere を継承して当たり判定情報を追加
struct CollisionShape : public GM31::GE::Collision::BoundingSphere {
	uint32_t    myCategory = 0; // 自分のグループ
	uint32_t    hitMask = 0; // 当たりたい相手
	gameobject* owner = nullptr;

	CollisionShape(Vector3 c, float r, uint32_t cat, uint32_t mask, gameobject* own) {
		center = c;
		radius = r;
		myCategory = cat;
		hitMask = mask;
		owner = own;
	}
};
