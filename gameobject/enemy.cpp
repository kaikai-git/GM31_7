#include	<cstdint>
#include	"enemy.h"	
#include	"../system/commontypes.h"
#include	"../system/Inputmanager.h"	
#include	"../system/transform.h"
#include "../system/meshmanager.h"

void enemy::init() {

	m_srt.pos = Vector3(0, 0, 0);
	m_srt.scale = Vector3(1.0f, 1.0f, 1.0f);
	m_srt.rot = Vector3(0, 0, 0);
}

void enemy::update(uint64_t dt) {

	{
	}
}

void enemy::draw(uint64_t dt) {

	Matrix4x4 mtx = m_srt.GetMatrix();	

	// 行列をセットして描画
	ShaderManager::getShader<CShader>("Shader3D")->SetGPU();
	Renderer::SetWorldMatrix(&mtx);
	MeshManager::getRenderer<CMeshRenderer>(m_meshid)->Draw();
}

void enemy::dispose()
{

}
