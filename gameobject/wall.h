#pragma once

#include	<cstdint>
#include	"gameobject.h"
#include	"../system/CPlane.h"
#include	"../system/IScene.h"

// 前方宣言
class ScenenBase;

class wall : public gameobject {
public:
	// ISceneのポインタを受け取るコンストラクタを追加
	wall(SceneBase* scene) :gameobject(scene) {}

	void update(uint64_t delta) override;
	void draw(uint64_t delta) override;
	void init() override;
	void dispose() override;

	void setwidth(float w) {
		m_width = w;
	}
	void setheight(float h) {
		m_height = h;
	}
	void setthick(float t) {
		m_thick = t;
	}

	float getwidth() const {
		return m_width;
	}

	float getheight() const {
		return m_height;
	}

	float getthick() const {
		return m_thick;
	}

	CPlane getEquation() const{
		return m_planeEquation;
	}

	void setRotationY(float angle) {
		SRT srt = getSRT();
		srt.rot.y = angle;
		setSRT(srt);
	}

	void calcEqation();

	void drawred(uint64_t dt);

private:
	float	m_height{100};				// 高さ
	float	m_width{500};				// 幅
	float	m_thick{1.0f};				// 厚さ

	CPlane  m_planeEquation{};			// 平面の方程式
};