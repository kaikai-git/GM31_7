#include	<cstdint>
#include	"player.h"	
#include	"wall.h"
#include	"field.h"
#include	"../system/commontypes.h"
#include	"../system/Inputmanager.h"	
#include	"../system/transform.h"
#include	"../system/meshmanager.h"
#include	"../system/camera.h"
#include	"../system/collision.h"

void player::init() {

	m_srt.pos = Vector3(0, 0, 0);
	m_srt.scale = Vector3(1.0f, 1.0f, 1.0f);
	m_srt.rot = Vector3(0, 0, 0);
}

void player::update(uint64_t dt) {

	Camera* camera = m_ownerscene->getcamera();
	FreeCamera* freecamera = dynamic_cast<FreeCamera*>(camera);
	if (freecamera == nullptr) { return; }

	Matrix4x4 viewmtx = freecamera->GetViewMatrix();

	// ビュー行列からカメラの前方向・右方向を取得し、XZ平面に投影する
	Vector3 camforward = Vector3(viewmtx._13, 0.0f, viewmtx._33);
	Vector3 camright = Vector3(viewmtx._11, 0.0f, viewmtx._31);

	// 正規化(真上/真下を向いている場合の保険つき)
	float lenf = std::sqrtf(camforward.x * camforward.x + camforward.z * camforward.z);
	if (lenf > 0.0001f)
	{
		camforward.x /= lenf;
		camforward.z /= lenf;
	}
	else
	{
		camforward = Vector3(0.0f, 0.0f, 1.0f);
	}

	float lenr = std::sqrtf(camright.x * camright.x + camright.z * camright.z);
	if (lenr > 0.0001f)
	{
		camright.x /= lenr;
		camright.z /= lenr;
	}
	else
	{
		camright = Vector3(1.0f, 0.0f, 0.0f);
	}

	// 入力をカメラ基準の移動方向に合成する
	float dirx = 0.0f;
	float dirz = 0.0f;

	if (CInputManager::GetInstance().IsKeyPressed(DIK_W))
	{// 前移動(カメラの奥方向)
		dirx += camforward.x;
		dirz += camforward.z;
	}
	if (CInputManager::GetInstance().IsKeyPressed(DIK_S))
	{// 後移動(カメラの手前方向)
		dirx -= camforward.x;
		dirz -= camforward.z;
	}
	if (CInputManager::GetInstance().IsKeyPressed(DIK_A))
	{// 左移動(カメラの左方向)
		dirx -= camright.x;
		dirz -= camright.z;
	}
	if (CInputManager::GetInstance().IsKeyPressed(DIK_D))
	{// 右移動(カメラの右方向)
		dirx += camright.x;
		dirz += camright.z;
	}

	// 移動入力があれば正規化して移動量と目標角度をセット
	float lend = std::sqrtf(dirx * dirx + dirz * dirz);
	if (lend > 0.0001f)
	{
		dirx /= lend;
		dirz /= lend;

		m_move.x += dirx * VALUE_MOVE_MODEL;
		m_move.z += dirz * VALUE_MOVE_MODEL;

		// 移動方向 (-sin, -cos) から目標角度を逆算する
		m_destrot.y = std::atan2f(-dirx, -dirz);
	}

	if (CInputManager::GetInstance().IsKeyPressed(DIK_RIGHT))
	{// 左回転
		m_destrot.y = m_srt.rot.y - VALUE_ROTATE_MODEL;
		if (m_destrot.y < -PI)
		{
			m_destrot.y += PI * 2.0f;
		}
	}

	if (CInputManager::GetInstance().IsKeyPressed(DIK_LEFT))
	{// 右回転
		m_destrot.y = m_srt.rot.y + VALUE_ROTATE_MODEL;
		if (m_destrot.y > PI)
		{
			m_destrot.y -= PI * 2.0f;
		}
	}

	// 目標角度と現在角度との差分を求める
	float diffrot = m_destrot.y - m_srt.rot.y;
	if (diffrot > PI)
	{
		diffrot -= PI * 2.0f;
	}
	if (diffrot < -PI)
	{
		diffrot += PI * 2.0f;
	}

	// 比率計算
	m_srt.rot.y += diffrot * RATE_ROTATE_MODEL;
	if (m_srt.rot.y > PI)
	{
		m_srt.rot.y -= PI * 2.0f;
	}
	if (m_srt.rot.y < -PI)
	{
		m_srt.rot.y += PI * 2.0f;
	}

	// 壁との当たり判定
	std::vector<wall*> wallobjs = m_ownerscene->GetGameObjects<wall>();

	// 移動前の座標（壁のどちら側にいたかの基準になる）
	const Vector3 basepos = m_srt.pos;

	// プレイヤの当たり判定用の球
	GM31::GE::Collision::BoundingSphere bs;
	bs.radius = COLLISION_RADIUS;
	bs.center = basepos + m_move;

	m_ishitwall = false;

	// 1つの壁から押し出された結果、別の壁にめり込むことがあるので
	// めり込みが無くなるまで（最大 MAX_WALL_RESOLVE 回）繰り返す
	bool ishitthisloop = false;

	for (int loopcnt = 0; loopcnt < MAX_WALL_RESOLVE; loopcnt++) {

		ishitthisloop = false;

		// 今の移動量での次フレーム位置
		Vector3 nextpos = basepos + m_move;

		for (const auto& wallobj : wallobjs) {
			CPlane pl = wallobj->getEquation();
			PLANEINFO plinfo = pl.GetPlaneInfo();

			// 壁（無限平面）までの符号付き距離で大まかに絞り込む
			float lng = GM31::GE::Collision::calcPlaneDistance(plinfo.plane, nextpos);
			if (fabsf(lng) >= bs.radius) {
				continue;		// 平面から十分離れている
			}

			// 壁の板（OBB）と球で判定する
			GM31::GE::Collision::BoundingBoxOBB obb;

			SRT wallsrt = wallobj->getSRT();

			obb.lengthx = wallobj->getwidth();
			obb.lengthy = wallobj->getheight();
			obb.lengthz = wallobj->getthick();

			obb.worldcenter = wallsrt.pos;
			wallsrt.scale = Vector3(1, 1, 1);
			Matrix4x4 mtx = wallsrt.GetMatrix();
			obb.axisX = mtx.Right();
			obb.axisY = mtx.Up();
			obb.axisZ = mtx.Forward();

			bs.center = nextpos;

			if (GM31::GE::Collision::CollisionSphereOBB(bs, obb) == false) {
				continue;		// 板の範囲外を通過している
			}

			// めり込みベクトルを求める
			Vector3 penetration = GM31::GE::Collision::calcPenetrationVector(
				plinfo.plane, basepos, nextpos, bs.radius);

			// ちょうど接しているだけなら押し出す必要は無い
			if (penetration.LengthSquared() <= WALL_EPSILON) {
				continue;
			}

			// 壁との交点を求める（デバッグ表示用）
			GM31::GE::Collision::calcWallCrossPoint(
				plinfo.plane, basepos, m_move, m_crosspoint);

			m_penetration = penetration;

			// 次フレーム位置をめり込みベクトル分だけ押し出す
			nextpos += penetration;

			// 押し出された新しい位置から、移動量(m_move)を逆算する
			// （これにより、自然に壁に沿った移動ベクトルになります）
			m_move = nextpos - basepos;

			ishitthisloop = true;
			m_ishitwall = true;
		}

		// どの壁ともめり込まなくなったら終了
		if (ishitthisloop == false) {
			break;
		}
	}

	// 繰り返しても解決しない（壁と壁に挟まれた）場合はその場で止まる
	if (ishitthisloop) {
		m_move = Vector3(0, 0, 0);
	}

	/// 位置移動
	m_srt.pos += m_move;

	// 地面との当たり判定（移動後の位置で高さを求める）
	field* fieldobj = m_ownerscene->GetGameObject<field>();

	float height = fieldobj->GetHeight2(m_srt.pos);
	m_srt.pos.y = height;

	// 移動量に慣性をかける(減速率)//
	m_move += -m_move * RATE_MOVE_MODEL;
}
	
void player::draw(uint64_t dt) {

	Matrix4x4 mtx = m_srt.GetMatrix();

	// 行列をセットして描画
	ShaderManager::getShader<CShader>("Shader3D")->SetGPU();
	Renderer::SetWorldMatrix(&mtx);
	MeshManager::getRenderer<CMeshRenderer>(m_meshid)->Draw();
}

void player::dispose()
{

}
