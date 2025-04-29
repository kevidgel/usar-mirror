#pragma once

#include <string>
#include <memory>

#include "common.hpp"
#include "tiny_gltf.h"
#include "shaders.h"

#include <glm/gtc/matrix_transform.hpp>

namespace UsArMirror {
class ModelRenderer {
public:
    ModelRenderer(const std::string& filename);
    ~ModelRenderer();

    void initShader();

    // Render a model
    void render( int width, int height, glm::mat4 proj, glm::mat4 view, float opacity);

    // Cleanup resources
    void cleanup();

private:
    // Add private members and helper functions here
    tinygltf::Model model_;
    Shaders shader_;
    std::pair<GLuint, std::map<int, GLuint>> vaoAndEbos_;
    GLuint modelTexture_ = 0;
    // GLuint modelTexture = 0; // Global model texture
    GLuint MVP_u;
    GLuint sun_position_u;
    GLuint sun_color_u;
    GLuint tex_u;

    glm::mat4 model_mat;
    glm::mat4 model_rot;
    glm::vec3 model_pos;
    glm::vec3 sun_position;
    glm::vec3 sun_color;


    bool loadModel(tinygltf::Model &model, const char *filename);
    void bindMesh(std::map<int, GLuint>& vbos, tinygltf::Model &model, tinygltf::Mesh &mesh);
    void bindModelNodes(std::map<int, GLuint>& vbos, tinygltf::Model &model,
        tinygltf::Node &node);
        std::pair<GLuint, std::map<int, GLuint>> bindModel(tinygltf::Model &model);
    void drawMesh(const std::map<int, GLuint>& vbos,
        tinygltf::Model &model, tinygltf::Mesh &mesh) ;
    void drawModelNodes(const std::pair<GLuint, std::map<int, GLuint>>& vaoAndEbos,
        tinygltf::Model &model, tinygltf::Node &node);
    void drawModel(const std::pair<GLuint, std::map<int, GLuint>> &vaoAndEbos, tinygltf::Model &model);
    void dbgModel(tinygltf::Model &model);
    // glm::mat4 genView(const glm::vec3& eye, const glm::vec3& target);
    // glm::mat4 genMVP(const glm::mat4& view, const glm::mat4& model, float fov, int width, int height);

};
}
