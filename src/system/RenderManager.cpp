#include"RenderManager.h"
#include"../renderer/Mesh_Renderer.h"
#include"../object/Terrain.h"
#include"../object/SkyBox.h"
#include"../renderer/RenderScene.h"
#include"../buffer/UniformBuffer.h"
#include"../utils/Camera.h"
#include"../component/GameObject.h"
#include"../utils/Shader.h"
#include"../component/Lights.h"
#include"../component/transform.h"

#include<glm/gtc/type_ptr.hpp>

RenderManager::RenderManager() {
	m_shader = std::vector<std::shared_ptr<Shader>>(ShaderTypeNum,nullptr);
	initVPbuffer();
	initPointLightBuffer();
	initDirectionLightBuffer();
}
void RenderManager::initVPbuffer() {
	// initialize a UBO for VP matrices
	uniformVPBuffer = std::make_shared<UniformBuffer>(
			2 * sizeof(glm::mat4)
		); 
	// set binding point
	uniformVPBuffer->setBinding(0);
}
void RenderManager::initPointLightBuffer() {
	// TODO: 
	const int maxLight = 10; 
	int lightBufferSize = maxLight * (32) + 16; // maximum 10 point lights, std140 layout
	uniformPointLightBuffer = std::make_shared<UniformBuffer>(lightBufferSize);
	// set binding point
	uniformPointLightBuffer->setBinding(1);
}
void RenderManager::initDirectionLightBuffer() {
	const int maxLight = 10; 
	int lightBufferSize = maxLight * (48) + 16; 
	uniformDirectionLightBuffer = std::make_shared<UniformBuffer>(lightBufferSize);
	// set binding point
	uniformDirectionLightBuffer->setBinding(2);
}
RenderManager::~RenderManager() {

}

void RenderManager::prepareVPData(const std::shared_ptr<RenderScene>& renderScene) {
	const std::shared_ptr<Camera>& camera = renderScene->main_camera;

	const glm::mat4& projection = camera->GetPerspective();
	const glm::mat4& view = camera->GetViewMatrix();
	//glm::mat4 skyboxView = glm::mat4(glm::mat3(view));
	
	// update every frame
	if (uniformVPBuffer) {
		uniformVPBuffer->bindBuffer();
		GLuint UBO = uniformVPBuffer->UBO;
		glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(glm::mat4), glm::value_ptr(projection));
		glBufferSubData(GL_UNIFORM_BUFFER, sizeof(glm::mat4),sizeof(glm::mat4), glm::value_ptr(view));
	}
	// update the first time
	if (uniformVPBuffer->dirty) {
		//if dirty, set shader bindings of UBO
		for (std::shared_ptr<Shader>& shader : m_shader) {
			if (shader) {
				//if shader is not null
				shader->use();
				shader->setUniformBuffer("VP", uniformVPBuffer->binding);
			}
		}
		uniformVPBuffer->setDirtyFlag(false);
	}

	// update every frame: skybox view
	std::shared_ptr<Shader>& skyboxShader = m_shader[static_cast<int>(ShaderType::SKYBOX)];
	skyboxShader->use();
	skyboxShader->setMat4("view", glm::mat4(glm::mat3(camera->GetViewMatrix())));

	// campos
	for (auto& shader : m_shader) {
		if (shader) { 
			shader->use();
			shader->setVec3("camPos", renderScene->main_camera->Position); 
		}
	}
}

void RenderManager::preparePointLightData(const std::shared_ptr<RenderScene>& scene) {
	// TODO: test the consfusing offset
	// point light
	// update at the first time
	if (uniformPointLightBuffer->dirty) {
		uniformPointLightBuffer->dirty = false;
		for (std::shared_ptr<Shader>& shader : m_shader) {
			if (shader) {
				shader->use();
				shader->setUniformBuffer("PointLightBuffer", uniformPointLightBuffer->binding);
			}
		}
	}
	uniformPointLightBuffer->bindBuffer();
	unsigned int UBO = uniformPointLightBuffer->UBO;
	int lightNum = scene->pointLights.size();
	int dataSize = 32; // data size for a single light (under std140 layout)
	for (int i = 0; i < lightNum; i++) {
		auto& light = scene->pointLights[i]; 
		if (!light->dirty) {
			// if not dirty, then pass
			continue;
		}
		PointLightData& data = scene->pointLights[i]->data; 
		std::shared_ptr<Transform>& transform = std::dynamic_pointer_cast<Transform>(
			scene->pointLights[i]->gameObject->GetComponent("Transform"));
		glBufferSubData(GL_UNIFORM_BUFFER, 
			0 + i * dataSize, 
			sizeof(glm::vec3), glm::value_ptr(data.color)); //color 
		glBufferSubData(GL_UNIFORM_BUFFER, 
			16 + i * dataSize, 
			sizeof(glm::vec3), glm::value_ptr(transform->position)); //position
		light->setDirtyFlag(false); // ?
	}
	// add the number of lights to UBO
	glBufferSubData(GL_UNIFORM_BUFFER,
		dataSize * 10, sizeof(int), &lightNum);

	//uniformPointLightBuffer->bindBuffer();
}

void RenderManager::prepareDirectionLightData(const std::shared_ptr<RenderScene>& scene) {
	// update light data only when dirty 
	if (uniformDirectionLightBuffer->dirty) {
		uniformDirectionLightBuffer->dirty = false;
		for (std::shared_ptr<Shader>& shader : m_shader) {
			if (shader) {
				shader->setUniformBuffer("DirectionLightBuffer", uniformDirectionLightBuffer->binding);
			}
		}
	}
	uniformDirectionLightBuffer->bindBuffer();
	unsigned int UBO = uniformDirectionLightBuffer->UBO;
	int lightNum = scene->directionLights.size();
	int dataSize = 48; // data size for a single light (under std140 layout)
	for (int i = 0; i < lightNum; i++) {
		auto& light = scene->directionLights[i];
		std::shared_ptr<Transform>& transform = std::dynamic_pointer_cast<Transform>(
			scene->directionLights[i]->gameObject->GetComponent("Transform"));

		if (!light->dirty) {
			continue;
		}
		DirectionLightData& data = scene->directionLights[i]->data; 
		glBufferSubData(GL_UNIFORM_BUFFER, 
			0 + i * dataSize, 
			sizeof(glm::vec3), glm::value_ptr(data.color)); // ambient
		glBufferSubData(GL_UNIFORM_BUFFER, 
			16 + i * dataSize, 
			sizeof(glm::vec3), glm::value_ptr(transform->position)); //
		glBufferSubData(GL_UNIFORM_BUFFER, 
			32 + i * dataSize, 
			sizeof(glm::vec3), glm::value_ptr(data.direction));
		//glBufferSubData(GL_UNIFORM_BUFFER, 
			//48 + i * dataSize, 
			//sizeof(glm::vec3), glm::value_ptr(data.direction));
		light->setDirtyFlag(false);
	}
	glBufferSubData(GL_UNIFORM_BUFFER, 10 * dataSize, 
		sizeof(int), &lightNum);
}


void RenderManager::render(const std::shared_ptr<RenderScene>& scene) {
	// TODO: wrap up this function to be Scene rendering pass
	prepareVPData(scene);
	preparePointLightData(scene);
	prepareDirectionLightData(scene);

	for (auto object : scene->objects) {
		std::shared_ptr<MeshRenderer>& renderer = std::dynamic_pointer_cast<MeshRenderer>(object->GetComponent("MeshRenderer"));
		if (renderer && renderer->shader) {
			renderer->shader->use();
			renderer->render();
		}
	}
	// render Terrain 
	std::shared_ptr<Terrain>& terrain = scene->terrain;
	if (terrain && terrain->shader) {
		terrain->shader->use();
		terrain->render();
	}

	// render skybox 
	std::shared_ptr<SkyBox>& skybox = scene->skybox;
	if (skybox && skybox->shader) {
		skybox->shader->use();
		skybox->render();
	}
}

std::shared_ptr<Shader> RenderManager::getShader(ShaderType type) {
	int index = static_cast<int>(type);
	if(!m_shader[index]){
		//if not initialized 
		m_shader[index] = RenderManager::generateShader(type);
	}
	return m_shader[index];
}
std::shared_ptr<Shader> RenderManager::generateShader(ShaderType type) {

	switch (type) {
		case ShaderType::LIGHT:
			return std::make_shared<Shader>(
				"./src/shader/light.vs", "./src/shader/light.fs", nullptr,
				nullptr, nullptr
				);
			break;
		case ShaderType::PBR:
			return std::make_shared<Shader>(
				"./src/shader/pbr.vs", "./src/shader/pbr.fs", nullptr,
				nullptr, nullptr
				);
			break;
		case ShaderType::SIMPLE:
			return std::make_shared<Shader>(
				"./src/shader/simple.vs", "./src/shader/simple.fs"
				);
			break;
		case ShaderType::SKYBOX:
			return std::make_shared<Shader>(
				"./src/shader/skybox.vs", "./src/shader/skybox.fs"
				);
			break;
		case ShaderType::TERRAIN:
			return std::make_shared<Shader>(
				"./src/shader/terrain.vs", "./src/shader/pbr.fs", nullptr,
				"./src/shader/terrain.tesc", "./src/shader/terrain.tese"
				);
			break;
		default:
			std::cerr << "No such shader type" << '\n';
			break;
	}
	//return std::make_shared<Shader>(nullptr, nullptr, nullptr, nullptr, nullptr);
}
