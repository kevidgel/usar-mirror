// model_renderer.cpp
#include <fstream>
#include <iostream>

#include "model_renderer.hpp"
#include <iostream>
#include <glm/gtc/type_ptr.hpp>
#include "common.hpp" // for BUFFER_OFFSET macro
// #include "tiny_gltf.h"
#include "shaders.h"
// #include "window.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
// #include "tiny_gltf_loader.cpp"
#define BUFFER_OFFSET(i) ((char *)NULL + (i))


namespace UsArMirror {

    ModelRenderer::ModelRenderer(const std::shared_ptr<State>& state,const std::string& filename)
        : modelTexture_(0), state(state) {
        std::cout << "Loading model: " << filename << std::endl;
        
        initShader();

        if (loadModel(model_, filename.c_str())) {
            vaoAndEbos_ = bindModel(model_);
        } else {
            std::cerr << "Failed to load model: " << filename << std::endl;
        }
    }

    ModelRenderer::~ModelRenderer() {
        std::cout << "Cleaning up model renderer resources." << std::endl;
        glDeleteVertexArrays(1, &vaoAndEbos_.first);
        // if (modelTexture_) glDeleteTextures(1, &modelTexture_);
        glfwTerminate();
    }

    void ModelRenderer::initShader() {
      // 1. Load shaders (vertex + fragment), compile, and link
      shader_ = Shaders();
  
      if (shader_.pid == 0) {
          std::cerr << "Failed to compile/link shader!" << std::endl;
          return;
      }
  
      // 2. Set uniform locations
      MVP_u          = glGetUniformLocation(shader_.pid, "MVP");
      sun_position_u = glGetUniformLocation(shader_.pid, "sun_position");
      sun_color_u    = glGetUniformLocation(shader_.pid, "sun_color");
      tex_u          = glGetUniformLocation(shader_.pid, "tex");
  
      if (MVP_u == -1 || sun_position_u == -1 || sun_color_u == -1 || tex_u == -1) {
          std::cerr << "Warning: Some uniforms not found!" << std::endl;
      }
  
      // 3. Set constant uniform values (like sampler2D binding)
      glUseProgram(shader_.pid);
      glUniform1i(tex_u, 0); // Set sampler to use GL_TEXTURE0
  
      // 4. Initialize model transforms (optional)
      // model_mat = glm::mat4(1.0f);
      // model_rot = glm::mat4(1.0f);
      model_pos = glm::vec3(0.0f, 0.0f, 0.0f);
      sun_position = glm::vec3(3.0f, 10.0f, -5.0f);
      sun_color = glm::vec3(1.0f);
    }
  

    void ModelRenderer::render(glm::mat4 proj, glm::mat4 view, glm::mat4 model_mat, float opacity) {
        
        if (!shader_.pid) {
            std::cerr << "Shader not initialized!\n";
            return;
        }
        if (vaoAndEbos_.first == 0) {
            std::cerr << "VAO not initialized!\n";
            return;
        }
        if (model_.nodes.empty()) {
            std::cerr << "Model is empty!\n";
            return;
        }
      
        glViewport(0, 0, state->viewportWidth * state->viewportScaling, state->viewportHeight * state->viewportScaling);

        glUseProgram(shader_.pid);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, modelTexture_);

        // model_rot = glm::rotate(model_rot, glm::radians(0.8f), glm::vec3(0, 1, 0));
        // glm::mat4 trans = glm::translate(glm::mat4(1.0f), model_pos);
        // model_mat = trans * model_rot * model_mat;

        glm::mat4 mvp = proj * view * model_mat;

        glUniformMatrix4fv(MVP_u, 1, GL_FALSE, &mvp[0][0]);
        glUniform3fv(sun_position_u, 1, &sun_position[0]);
        glUniform3fv(sun_color_u, 1, &sun_color[0]);

        GLuint opacityLoc = glGetUniformLocation(shader_.pid, "opacity");
        glUniform1f(opacityLoc, opacity); // 0.0 = transparent, 1.0 = opaque

        drawModel(vaoAndEbos_, model_);
    }
    
    void ModelRenderer::cleanup() {
        glDeleteVertexArrays(1, &vaoAndEbos_.first);
        // if (modelTexture_) glDeleteTextures(1, &modelTexture_);
    }

    bool ModelRenderer::loadModel(tinygltf::Model &model, const char *filename) {
      tinygltf::TinyGLTF loader;
      std::string err;
      std::string warn;
    
      bool res = loader.LoadASCIIFromFile(&model, &err, &warn, filename);
      if (!warn.empty()) {
        std::cout << "WARN: " << warn << std::endl;
      }
    
      if (!err.empty()) {
        std::cout << "ERR: " << err << std::endl;
      }
    
      if (!res)
        std::cout << "Failed to load glTF: " << filename << std::endl;
      else
        std::cout << "Loaded glTF: " << filename << std::endl;
    
      dbgModel(model);
      return res;
    }
    
    void ModelRenderer::bindMesh(std::map<int, GLuint>& vbos,
                  tinygltf::Model &model, tinygltf::Mesh &mesh) {
      for (size_t i = 0; i < model.bufferViews.size(); ++i) {
        const tinygltf::BufferView &bufferView = model.bufferViews[i];
        if (bufferView.target == 0) {  // TODO impl drawarrays
          std::cout << "WARN: bufferView.target is zero" << std::endl;
          continue;  // Unsupported bufferView.
                     /*
                       From spec2.0 readme:
                       https://github.com/KhronosGroup/glTF/tree/master/specification/2.0
                                ... drawArrays function should be used with a count equal to
                       the count            property of any of the accessors referenced by the
                       attributes            property            (they are all equal for a given
                       primitive).
                     */
        }
    
        const tinygltf::Buffer &buffer = model.buffers[bufferView.buffer];
        std::cout << "bufferview.target " << bufferView.target << std::endl;
    
        GLuint vbo;
        glGenBuffers(1, &vbo);
        vbos[i] = vbo;
        glBindBuffer(bufferView.target, vbo);
    
        std::cout << "buffer.data.size = " << buffer.data.size()
                  << ", bufferview.byteOffset = " << bufferView.byteOffset
                  << std::endl;
    
        glBufferData(bufferView.target, bufferView.byteLength,
                     &buffer.data.at(0) + bufferView.byteOffset, GL_STATIC_DRAW);
      }
    
      for (size_t i = 0; i < mesh.primitives.size(); ++i) {
        tinygltf::Primitive primitive = mesh.primitives[i];
        tinygltf::Accessor indexAccessor = model.accessors[primitive.indices];
    
        for (auto &attrib : primitive.attributes) {
          tinygltf::Accessor accessor = model.accessors[attrib.second];
          int byteStride =
              accessor.ByteStride(model.bufferViews[accessor.bufferView]);
          glBindBuffer(GL_ARRAY_BUFFER, vbos[accessor.bufferView]);
    
          int size = 1;
          if (accessor.type != TINYGLTF_TYPE_SCALAR) {
            size = accessor.type;
          }
          int vaa = -1;
          if (attrib.first.compare("POSITION") == 0) vaa = 0;
          if (attrib.first.compare("NORMAL") == 0) vaa = 1;
          if (attrib.first.compare("TEXCOORD_0") == 0) vaa = 2;
          if (vaa > -1) {
            glEnableVertexAttribArray(vaa);
            glVertexAttribPointer(vaa, size, accessor.componentType,
                                  accessor.normalized ? GL_TRUE : GL_FALSE,
                                  byteStride, BUFFER_OFFSET(accessor.byteOffset));
          } else
            std::cout << "vaa missing: " << attrib.first << std::endl;
        }
    
        if (model.textures.size() > 0) {
          // fixme: Use material's baseColor
          glActiveTexture(GL_TEXTURE0);
          tinygltf::Texture &tex = model.textures[0];
    
          if (tex.source > -1) {
    
            GLuint texid;
            glGenTextures(1, &texid);//TODO: check this
    
            tinygltf::Image &image = model.images[tex.source];
    
            glBindTexture(GL_TEXTURE_2D, texid);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    
            GLenum format = GL_RGBA;
    
            if (image.component == 1) {
              format = GL_RED;
            } else if (image.component == 2) {
              format = GL_RG;
            } else if (image.component == 3) {
              format = GL_RGB;
            } else {
              // ???
            }
    
            GLenum type = GL_UNSIGNED_BYTE;
            if (image.bits == 8) {
              // ok
            } else if (image.bits == 16) {
              type = GL_UNSIGNED_SHORT;
            } else {
              // ???
            }
    
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image.width, image.height, 0,
                         format, type, &image.image.at(0));
          }
        }
      }
    }
    
    // bind models
    void ModelRenderer::bindModelNodes(std::map<int, GLuint>& vbos, tinygltf::Model &model,
                        tinygltf::Node &node) {
      if ((node.mesh >= 0) && (node.mesh < model.meshes.size())) {
        bindMesh(vbos, model, model.meshes[node.mesh]);
      }
    
      for (size_t i = 0; i < node.children.size(); i++) {
        assert((node.children[i] >= 0) && (node.children[i] < model.nodes.size()));
        bindModelNodes(vbos, model, model.nodes[node.children[i]]);
      }
    }
    
    std::pair<GLuint, std::map<int, GLuint>> ModelRenderer::bindModel(tinygltf::Model &model) {
        std::map<int, GLuint> vbos;
        GLuint vao;
        glGenVertexArrays(1, &vao);
        glBindVertexArray(vao);
    
        for (size_t i = 0; i < model.bufferViews.size(); ++i) {
            const auto &bufferView = model.bufferViews[i];
            if (bufferView.target == 0) continue;
            const auto &buffer = model.buffers[bufferView.buffer];
            GLuint vbo;
            glGenBuffers(1, &vbo);
            vbos[i] = vbo;
            glBindBuffer(bufferView.target, vbo);
            glBufferData(bufferView.target, bufferView.byteLength,
                         &buffer.data.at(0) + bufferView.byteOffset, GL_STATIC_DRAW);
        }
    
        for (const auto &mesh : model.meshes) {
            for (const auto &primitive : mesh.primitives) {
                for (const auto &attrib : primitive.attributes) {
                    const auto &accessor = model.accessors[attrib.second];
                    int stride = accessor.ByteStride(model.bufferViews[accessor.bufferView]);
                    glBindBuffer(GL_ARRAY_BUFFER, vbos[accessor.bufferView]);
                    int loc = (attrib.first == "POSITION") ? 0 : (attrib.first == "NORMAL") ? 1 : 2;
                    glEnableVertexAttribArray(loc);
                    glVertexAttribPointer(loc, accessor.type, accessor.componentType,
                                          accessor.normalized ? GL_TRUE : GL_FALSE,
                                          stride, BUFFER_OFFSET(accessor.byteOffset));
                }
                if (!model.textures.empty()) {
                    const auto &tex = model.textures[0];
                    if (tex.source > -1) {
                        glGenTextures(1, &modelTexture_);
                        const auto &image = model.images[tex.source];
                        glBindTexture(GL_TEXTURE_2D, modelTexture_);
                        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
                    
                    // t = (image.component == 1) ? GL_RED : (image.component == 2) ? GL_RG : (image.component == 3) ? GL_RGB : GL_RGBA;
                    //     glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image.width, image.height, 0,
                    //                  format, GL_UNSIGNED_BYTE, image.image.data());
                    }
                }
            }
        }
    
        glBindVertexArray(0);
        return {vao, vbos};
    }
    
    void ModelRenderer::drawMesh(const std::map<int, GLuint>& vbos,
                  tinygltf::Model &model, tinygltf::Mesh &mesh) {
      for (size_t i = 0; i < mesh.primitives.size(); ++i) {
        tinygltf::Primitive primitive = mesh.primitives[i];
        tinygltf::Accessor indexAccessor = model.accessors[primitive.indices];
    
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbos.at(indexAccessor.bufferView));
    
        glDrawElements(primitive.mode, indexAccessor.count,
                       indexAccessor.componentType,
                       BUFFER_OFFSET(indexAccessor.byteOffset));
      }
    }
    
    // recursively draw node and children nodes of model
    void ModelRenderer::drawModelNodes(const std::pair<GLuint, std::map<int, GLuint>>& vaoAndEbos,
                        tinygltf::Model &model, tinygltf::Node &node) {
      if ((node.mesh >= 0) && (node.mesh < model.meshes.size())) {
        drawMesh(vaoAndEbos.second, model, model.meshes[node.mesh]);
      }
      for (size_t i = 0; i < node.children.size(); i++) {
        drawModelNodes(vaoAndEbos, model, model.nodes[node.children[i]]);
      }
    }
    
    void ModelRenderer::drawModel(const std::pair<GLuint, std::map<int, GLuint>> &vaoAndEbos, tinygltf::Model &model) {
        glBindVertexArray(vaoAndEbos.first);
        for (const auto &mesh : model.meshes) {
            for (const auto &primitive : mesh.primitives) {
                const auto &accessor = model.accessors[primitive.indices];
                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vaoAndEbos.second.at(accessor.bufferView));
                glDrawElements(primitive.mode, accessor.count, accessor.componentType,
                               BUFFER_OFFSET(accessor.byteOffset));
            }
        }
        glBindVertexArray(0);
    }
    
    void ModelRenderer::dbgModel(tinygltf::Model &model) {
      for (auto &mesh : model.meshes) {
        std::cout << "mesh : " << mesh.name << std::endl;
        for (auto &primitive : mesh.primitives) {
          const tinygltf::Accessor &indexAccessor = model.accessors[primitive.indices];
          std::cout << "indexaccessor: count " << indexAccessor.count
                    << ", type " << indexAccessor.componentType << std::endl;
    
          if (primitive.material >= 0 && primitive.material < model.materials.size()) {
            tinygltf::Material &mat = model.materials[primitive.material];
            for (auto &mats : mat.values) {
              std::cout << "mat : " << mats.first.c_str() << std::endl;
            }
          }
    
          for (auto &image : model.images) {
            std::cout << "image name : " << image.uri << std::endl;
            std::cout << "  size : " << image.image.size() << std::endl;
            std::cout << "  w/h : " << image.width << "/" << image.height << std::endl;
          }
    
          std::cout << "indices : " << primitive.indices << std::endl;
          std::cout << "mode     : " << "(" << primitive.mode << ")" << std::endl;
    
          for (auto &attrib : primitive.attributes) {
            std::cout << "attribute : " << attrib.first.c_str() << std::endl;
          }
    
          // 👇 Print vertex positions:
          if (primitive.attributes.find("POSITION") != primitive.attributes.end()) {
            const auto &accessor = model.accessors[primitive.attributes.at("POSITION")];
            const auto &bufferView = model.bufferViews[accessor.bufferView];
            const auto &buffer = model.buffers[bufferView.buffer];
    
            const unsigned char* dataPtr = buffer.data.data() + bufferView.byteOffset + accessor.byteOffset;
            size_t count = accessor.count;
            int stride = accessor.ByteStride(bufferView);
            if (stride == 0) stride = sizeof(float) * 3; // Default stride for vec3
    
            std::cout << "Vertex positions:" << std::endl;
            for (size_t i = 0; i < count; ++i) {
              const float* pos = reinterpret_cast<const float*>(dataPtr + i * stride);
              std::cout << "  [" << i << "]: (" << pos[0] << ", " << pos[1] << ", " << pos[2] << ")" << std::endl;
            }
          }
        }
      }
    }
    
    
    // glm::mat4 ModelRenderer::genView(glm::vec3 pos, glm::vec3 lookat) {
    //   // Camera matrix
    //   glm::mat4 view = glm::lookAt(
    //       pos,                // Camera in World Space
    //       lookat,             // and looks at the origin
    //       glm::vec3(0, 1, 0)  // Head is up (set to 0,-1,0 to look upside-down)
    //   );
    
    //   return view;
    // }
    
    // glm::mat4 ModelRenderer::genMVP(glm::mat4 view_mat, glm::mat4 model_mat, float fov, int w,
    //                  int h) {
    //   glm::mat4 Projection =
    //       glm::perspective(glm::radians(fov), (float)w / (float)h, 0.01f, 1000.0f);
    
    //   // Or, for an ortho camera :
    //   // glm::mat4 Projection = glm::ortho(-10.0f,10.0f,-10.0f,10.0f,0.0f,100.0f);
    //   // // In world coordinates
    
    //   glm::mat4 mvp = Projection * view_mat * model_mat;
    
    //   return mvp;
    // }
}    
