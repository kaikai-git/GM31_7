#pragma once
#include <memory>
#include <unordered_map>
#include <string>
#include <array>
#include "CMesh.h"
#include "CMeshRenderer.h"
#include "CShader.h"
#include "noncopyable.h"

class ShaderManager : NonCopyable {
	static inline std::unordered_map<std::string, std::unique_ptr<CShader>> m_shadercontainer{};

public:
	template<class T>
	static bool RegisterShader(std::string key, std::unique_ptr<T> data) {
		// ‘¶İ‚µ‚Ä‚éê‡‚Í‰½‚à‚µ‚È‚¢
		if (m_shadercontainer.contains(key)) return false;

		// ‘¶İ‚µ‚Ä‚¢‚È‚¯‚ê‚Î“o˜^
		m_shadercontainer[key] = std::move(data);
		return true;
	}

	template<class T>
	static T* getShader(std::string key) {
		return static_cast<T*>(m_shadercontainer[key].get());
	}
};

class MeshManager : NonCopyable{

	static inline std::unordered_map<std::string, std::unique_ptr<CMesh>> m_meshcontainer{};
	static inline std::unordered_map<std::string, std::unique_ptr<CMeshRenderer>> m_renderercontainer{};

public:
	template<class T>
	static bool RegisterMesh(std::string key, std::unique_ptr<T> meshdata){
		// ‘¶İ‚µ‚Ä‚éê‡‚Í‰½‚à‚µ‚È‚¢
		if (m_meshcontainer.contains(key)) return false;

		// ‘¶İ‚µ‚Ä‚¢‚È‚¯‚ê‚Î“o˜^
		m_meshcontainer[key] = std::move(meshdata);
		return true;
	}

	template<class T>
	static bool RegisterMeshRenderer(std::string key, std::unique_ptr<T> data) {
		// ‘¶İ‚µ‚Ä‚éê‡‚Í‰½‚à‚µ‚È‚¢
		if (m_renderercontainer.contains(key)) return false;

		// ‘¶İ‚µ‚Ä‚¢‚È‚¯‚ê‚Î“o˜^
		m_renderercontainer[key] = std::move(data);
		return true;
	}

	template<class T>
	static T* getMesh(std::string key) {
		return static_cast<T*>(m_meshcontainer[key].get());
	}

	template<class T>
	static T* getRenderer(std::string key) {
		return static_cast<T*>(m_renderercontainer[key].get());
	}
};
