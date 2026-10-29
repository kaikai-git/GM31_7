#pragma once

#include	<cstdint>
#include	<string>
#include	"gameobject.h"
#include	"../system/IScene.h"

class CStaticMeshRenderer;
class CStaticMesh;

class enemy : public gameobject {

public:
	// ISceneのポインタを受け取るコンストラクタを追加
	enemy(SceneBase* scene) :gameobject(scene) {}

	void update(uint64_t delta) override;
	void draw(uint64_t delta) override;
	void init() override;
	void dispose() override;

	void setmeshid(const std::string& id) { m_meshid = id; }	

private:
	CStaticMeshRenderer* m_meshrenderer = nullptr;
	CStaticMesh* m_mesh = nullptr;

	std::string m_meshid{};
};