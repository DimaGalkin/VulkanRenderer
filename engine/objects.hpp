#pragma once

#include <iostream>
#include <vulkan/vulkan.hpp>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <opencv4/opencv2/videoio.hpp>
#include<opencv4/opencv2/core/mat.hpp>
#include <opencv2/core/ocl.hpp>

#include <vector>
#include <string>
#include <memory>
#include <ranges>

#include "types.hpp"
#include "vulkan/buffers.hpp"

namespace tdl {
    /**
     * @brief Used to store information about an objects material loaded from an MTL file.
     *
     * note that transparency is in 'd' ( read MTL spec )
    */
    struct Material {
        std::string name = "default";

        glm::vec3 ambient = {1.0f, 1.0f, 1.0f};

        bool diffuse_is_map = false;
        glm::vec3 diffuse = {1.0f, 1.0f, 1.0f};
        std::string diffuse_map_path;

        bool specular_is_map = false;
        glm::vec3 specular = {1.0f, 1.0f, 1.0f};
        std::string specular_map_path;

        float specular_exponent = 10.0f;
        float transparency_d = 1.0f;

        static std::array<unsigned char, 4> lTorRGBA (
            const glm::vec3& linear
        ) {
            return {
                static_cast<unsigned char>(linear.x * 255),
                static_cast<unsigned char>(linear.y * 255),
                static_cast<unsigned char>(linear.z * 255),
                255
            };
        }
    };

    /**
     * @breif Describes the UBO that each object is given.
    */
    struct ModelObject {
        glm::mat4 translation = glm::mat4(1);
        glm::mat4 rotation = glm::mat4(1);
    };

    class Model;

    /**
     * @brief wrapper around std::shared_ptr to allow for [] operator to be used without dereferencing
     *
     * This allows for a more user freindly interface for std::shared_ptr, the [] operator is overloaded to allow for the
     * user to access the objects_ attribute of tdl::Model without having to dereference the pointer.
     *
     * @tparam ModelT specifies type of smart pointer
    */
    template <typename ModelT> class shared_model final {
        public:
            /**
             * @breif Creates a shared_ptr of type ModelT using a list of parameters
             *
             * @param args all parameters usualy passed when instantiating a class of type ModelT
             */
            template <typename... Args>
            explicit shared_model (
                Args... args
            ) : ptr_ { std::make_shared<ModelT>(args...) } {}

            /**
             * @breif Creates a model_ptr from a shared_ptr
             *
             * @param ptr shared_ptr of type ModelT
            */
            shared_model ( // NOLINT (explicit)
                const std::shared_ptr<ModelT>& ptr
            ) : ptr_ { ptr } {}

            /**
             * @breif creates an empty ModelT if the constructor is avalible
            */
            shared_model() : ptr_ { std::make_shared<ModelT>() } {}

            /**
             * @breif creates a shared_model which points to nullptr
             *
             * @param ptr nullptr
            */
            shared_model ( // NOLINT (explicit)
                std::nullptr_t ptr
            ) : ptr_ { nullptr } {}

            /**
             * @breif Calls the [] operator on the ptr_ if it is not nullptr
             *
             * @param key name of the objects to look for
             * @return shared_model<Model>
            */
            shared_model<Model> operator[] (
                const std::string& key
            ) const {
                if (ptr_ == nullptr) return nullptr;
                return (*ptr_)[key];
            }

            /**
             * @breif Calls the [] operator on the ptr_ if it is not nullptr
             *
             * @param index index of the object to look for
             * @return shared_model<Model>
            */
            shared_model<Model> operator[] (
                const int index
            ) const {
                if (ptr_ == nullptr) return nullptr;
                if (index < 0 || index >= ptr_->keys_.size()) return nullptr;
                return (*ptr_)[ptr_->keys_[index]];
            }

            ModelT operator*() const {
                return *ptr_;
            }

            bool operator== (
                const shared_model<ModelT>& other
            ) const {
                return ptr_ == other.ptr_;
            }

            bool operator!= (
                const shared_model<ModelT>& other
            ) const {
                return ptr_ != other.ptr_;
            }

            ModelT* operator->() const {
                return ptr_.get();
            }

            explicit operator ModelT& () { return *ptr_; };
            explicit operator const ModelT& () const { return *ptr_; }

            std::shared_ptr<ModelT> ptr_;
    };

    /**
     * @brief describes a vertex in 3D space and some more important data.
     *
     *  Stores position and texture information of a vertex. Has static methods to retrive attribute descriptions used by
     *  Vulkan.
    */
    class Vertex final {
        public:
            Vertex (
                const glm::vec3& pos,
                const glm::vec3& color,
                const glm::vec2& uv
            ) : pos { pos },
                color { color },
                uv { uv }
            {}

            glm::vec3 pos, color;
            glm::vec2 uv;

            static vk::VertexInputBindingDescription getBindingDescription();
            static std::array<vk::VertexInputAttributeDescription, 3> getAttributeDescriptions();
    };

    class ObjectInterface;
    /**
     * @brief Static methods used to load an OBJ file.
     *
     * Provides a load method that takes a filename and loads data from an OBJ file as vector of vertices.
    */
    class OBJLoader final {
        public:
            /**
             * @breif splits a string into a vector by a given delimiter
             *
             * @param str string to split
             * @param delimiter delimiter to split by
             * @return std::vector<std::string>
             */
            static std::vector<std::string> split (
                const std::string& str,
                char delimiter
            );

            /**
             * @breif loads OBJ file and MTL file
             *
             * @param path path to OBJ file to load
             * @return std::vector<std::pair<std::string, std::shared_ptr<ObjectInterface>>>
            */
            static std::vector<std::pair<std::string, std::shared_ptr<ObjectInterface>>> loadOBJ (
                const std::string& path
            );

            /**
             * @breif loads OBJ file and textures it with a image or video at the given path
             *
             * @param path path to OBJ file to load
             * @param tex_path path to texture file to load
             * @return std::vector<std::pair<std::string, std::shared_ptr<ObjectInterface>>>
            */
            static std::vector<std::pair<std::string, std::shared_ptr<ObjectInterface>>> loadVWT (
                const std::string& path,
                const std::string& tex_path
            );

            /**
             * @breif loads OBJ file and makes is the colour represented by col
             *
             * @param path path to OBJ file to load
             * @param col colour of the object (RGBA format)
             * @return std::vector<std::pair<std::string, std::shared_ptr<ObjectInterface>>>
            */
            static std::vector<std::pair<std::string, std::shared_ptr<ObjectInterface>>> loadVWC (
                const std::string& path,
                const std::array<unsigned char, 4>& col
            );

            /**
             * @breif loades an mtl file as a series of materials
             *
             * @param path path to mtl file
             * @return std::vector<Material>
            */
            static std::vector<Material> loadMTL (
                const std::string& path
            );

            /**
             * @breif turns the path to the OBJ file into the path of the mtl file based on the path to the OBJ file and
             * the relative path of the mtl file to the OBJ file
             *
             * @param name name of mtl file
             * @param obj_path path to OBJ file containing 'mtllib'
             * @return
            */
            static std::string getMTLPath (
                const std::string& name,
                const std::string& obj_path
            );
    };

    /**
     * @brief Stores vertex information about a mesh
     *
     * A mesh that loads and stores vertex information in a vulkan buffer and provides methods to render it.
    */
    class Mesh final {
        public:
            /**
             * @breif sets the vertex data that the mesh stores and renders
             *
             * @param vertices vertex data (std::vector<tdl::Vertex>)
            */
            explicit Mesh (
                const std::vector<Vertex>& vertices
            ) : vertices_ { vertices } {}

            /**
             * @breif creates the vk::Buffer for the model and copies the vertex data into it
             *
             * @param device currently selected GPU (logical)
             * @param graphics_queue vk::Queue
             * @param command_pool command pool used to allocate command buffers
             * @param p_device currently selected GPU (physical)
            */
            void initBuffer (
                vk::Device device,
                vk::Queue graphics_queue,
                vk::CommandPool command_pool,
                vk::PhysicalDevice p_device
            );

            /**
             * @breif tells the command buffer to bind the vertex data (render the vertices)
             *
             * @param command_buffer vk::CommandBuffer used to render the mesh
            */
            void render (
                vk::CommandBuffer command_buffer
            ) const;

            ~Mesh() = default;

            std::vector<Vertex> vertices_;
        private:
            MemoryBuffer* buffer_ = nullptr;

            std::string path_;
            bool loaded_ = false;

            Material material_;
    };
    using MeshPtr = std::shared_ptr<Mesh>;

    /**
     * @breif Holds image data and provides methods to 'render' the image so it can be sampled from a shader.
     *
     * Can hold a static image or a video in which case a single frame is loaded to the shader. Frame rate of video is
     * independent of the frame rate of the engine.
    */
    class Texture final {
        public:
            /**
             * @breif Creates a texture that is a solid colour
             *
             * Creates a texture that is just one solid colour, default if no params passed is a solid white colour.
             *
             * @param color colour of the object (RGBA format)
            */
            explicit Texture (
                const std::array<unsigned char, 4>& color = {255, 255, 255, 255}
            ) : type_ { File::Image },
                color_ { color },
                is_color_ { true }
            {}

            /**
             * @breif Sets the path to the texture file and sets type of texture
             *
             * Path to texure is set in the constructor to be lazy loaded when required.
             *
             * @param path path to the image or video file
             * @param type image / video depending on path passed to constructor
            */
            explicit Texture (
                const std::string& path,
                const File type = tdl::File::Image
            ) : type_ { type },
                path_ { path },
                color_ { 0, 0, 0, 0 }
            {}

            /**
             * @breif loads texture from file
             *
             * Detects which load method to call and loads texture into vk::Image
             *
             * @param device currently used GPU (logical)
             * @param graphics_queue vk::Queue
             * @param command_pool command pool used to allocate command buffers
             * @param p_device currently used GPU (physical)
             * @param layout layout of the image
             * @param descriptor_pool descriptor pool used to allocate descriptor sets
            */
            void load (
                vk::Device device,
                vk::Queue graphics_queue,
                vk::CommandPool command_pool,
                vk::PhysicalDevice p_device,
                vk::DescriptorSetLayout layout,
                vk::DescriptorPool descriptor_pool
            );

            /**
             * @breif binds the texture (vk::Image) to the command buffer for rendering
             *
             * @param command_buffer command buffer to bind texture to
             * @param pipeline_layout vk::PipelineLayout
            */
            void render (
                vk::CommandBuffer command_buffer,
                vk::PipelineLayout pipeline_layout
            ) const;

            void loadNextFrame(); // queries opencv capture
            void setNextImage(); // creates vulkan image from latest frame

            ~Texture() = default;

        private:
            void loadImage(); // loads image from file (stb library)
            void loadVideo(); // loads video from file (opencv library)
            void loadColor(); // sets image to a single pixel of required colour

            File type_;

            std::unique_ptr<cv::VideoCapture> video_;
            cv::Mat frame_;
            cv::Mat frame_data_;
            std::mutex frame_mutex_;

            std::string path_;
            std::array<unsigned char, 4> color_;
            bool is_color_ = false;
            bool loaded_ = false;

            uint32_t width_ = 0;
            uint32_t height_ = 0;
            float fps_ = 0.0f;

            vk::Device device_;
            vk::Queue graphics_queue_;
            vk::CommandPool command_pool_;
            vk::PhysicalDevice p_device_;
            vk::DescriptorSetLayout layout_;
            vk::DescriptorPool descriptor_pool_;

            std::chrono::high_resolution_clock::time_point time_ = std::chrono::high_resolution_clock::now();

            Image image_;
    };
    using TexPtr = std::shared_ptr<Texture>;

    /**
     * @breif Purely virtual interface used to allow for objects of different types to be placed in an std::vector
    */
    class ObjectInterface {
        friend class Model;
        friend class Vlkn;

        public:
            explicit ObjectInterface (
                const MeshPtr& mesh
            ) : mesh_ { mesh } {}

            virtual void rotate (
                const glm::vec3& angles,
                const glm::vec3& centre
            ) = 0;

            virtual void rotate (
                const glm::vec3& angles
            ) = 0;

            virtual void translate (
                const glm::vec3& val
            ) = 0;

            virtual ~ObjectInterface() = default;

        protected:
            virtual void loadTexture (
                vk::Device device,
                vk::Queue graphics_queue,
                vk::CommandPool command_pool,
                vk::PhysicalDevice p_device,
                const vk::DescriptorSetLayout& layout,
                const vk::DescriptorPool& descriptor_pool
            ) const = 0;

            virtual void loadMesh (
                vk::Device device,
                vk::Queue graphics_queue,
                vk::CommandPool command_pool,
                vk::PhysicalDevice p_device
            )  = 0;

            virtual void render (
                vk::CommandBuffer command_buffer,
                vk::PipelineLayout pipeline_layout,
                unsigned long cframe
            ) const = 0;

            virtual void initUBOs (
                unsigned int max_f_frames,
                vk::Device device,
                vk::Queue graphics_queue,
                vk::CommandPool command_pool,
                vk::PhysicalDevice physical_device
            ) = 0;

            virtual void createDescriptorSets (
                unsigned int max_f_frames,
                vk::DescriptorPool descriptor_pool,
                vk::Device device,
                vk::DescriptorSetLayout ubo_layout
            ) = 0;

            virtual void caluclateCentre() = 0;
            virtual void imageTick() = 0;
            virtual void frameTick() = 0;

            std::vector<MemoryBuffer*> ubos_;
            ModelObject ubo_data_ {};
            bool has_changed_ = true;
            TexPtr tex_;
            MeshPtr mesh_;
            glm::vec3 centre_ {};
    };
    using ObjectPtr = std::shared_ptr<ObjectInterface>;

    /**
     * @breif Encapsulates a mesh and texture that form a 3D model
     *
     * Stores information about a 3D model like its mesh, texture and material and provides methods to 'render' the model
     * by binding it to a command buffer.
     *
     * @tparam tex_type specifies whether this is a video or image texture
    */
    template <File tex_type = File::Image>
    class Object final : public ObjectInterface {
        friend class Model;
        friend class Vlkn;

        public:
            /**
             * @breif Creates an object that stores
             *
             * @param mesh mesh that stores the objects vertex data
             * @param material material that the mesh is made from
            */
            Object (
                const MeshPtr& mesh,
                const Material& material
            ) : ObjectInterface { mesh },
                material_ { material }
            {
                if (material_.diffuse_is_map) {
                    tex_ = std::make_shared<Texture>(material_.diffuse_map_path);
                } else {
                    tex_ = std::make_shared<Texture>(Material::lTorRGBA(material_.diffuse));
                }
            }

            /**
             * @breif rotates the model around a given point
             *
             * @param angles x y z to rotate the model by
             * @param centre the centre of rotation
            */
            void rotate (
                const glm::vec3& angles,
                const glm::vec3& centre
            ) override {
                ubo_data_.rotation = glm::translate(ubo_data_.rotation, centre);
                ubo_data_.rotation = glm::rotate(ubo_data_.rotation, angles.x, {1, 0, 0});
                ubo_data_.rotation = glm::rotate(ubo_data_.rotation, angles.y, {0, 1, 0});
                ubo_data_.rotation = glm::rotate(ubo_data_.rotation, angles.z, {0, 0, 1});
                ubo_data_.rotation = glm::translate(ubo_data_.rotation, -centre);

                has_changed_ = true;
            }


            /**
             * @breif rotates the model around its centre
             *
             * @param angles x y z to rotate the model by
            */
            void rotate (
                const glm::vec3& angles
            ) override {
                ubo_data_.rotation = glm::translate(ubo_data_.rotation, centre_);
                ubo_data_.rotation = glm::rotate(ubo_data_.rotation, angles.x, {1, 0, 0});
                ubo_data_.rotation = glm::rotate(ubo_data_.rotation, angles.y, {0, 1, 0});
                ubo_data_.rotation = glm::rotate(ubo_data_.rotation, angles.z, {0, 0, 1});
                ubo_data_.rotation = glm::translate(ubo_data_.rotation, -centre_);

                has_changed_ = true;
            }

            /**
             * @breif translates the model by a given vector
             *
             * @param val x y z to translate the model by
            */
            void translate (
                const glm::vec3& val
            ) override {
                ubo_data_.translation = glm::translate(ubo_data_.translation, val);
            }

            ~Object() override {
                for (const auto& memory : ubos_) {
                    delete memory;
                }
            }

        protected:
            /**
             * @brief loads the texture from stored path into a vk::Image
             *
             * @param device GPU currently being used (logical)
             * @param graphics_queue vk::Queue
             * @param command_pool command pool used to allocate command buffers
             * @param p_device GPU currently being used (physical)
             * @param layout layout of bindings
             * @param descriptor_pool pool used to allocate descriptor sets
            */
            void loadTexture (
                const vk::Device device,
                const vk::Queue graphics_queue,
                const vk::CommandPool command_pool,
                const vk::PhysicalDevice p_device,
                const vk::DescriptorSetLayout& layout,
                const vk::DescriptorPool& descriptor_pool
            ) const override {
                tex_->load(
                    device,
                    graphics_queue,
                    command_pool,
                    p_device,
                    layout,
                    descriptor_pool
                );
            }

            /**
             * @breif loads mesh data into a vk::Buffer
             *
             * @param device GPU currently being used (logical)
             * @param graphics_queue vk::Queue
             * @param command_pool command pool used to allocate command buffers
             * @param p_device GPU currently being used (physical)
            */
            void loadMesh (
                const vk::Device device,
                const vk::Queue graphics_queue,
                const vk::CommandPool command_pool,
                const vk::PhysicalDevice p_device
            ) override {
                mesh_->initBuffer(device, graphics_queue, command_pool, p_device);
                caluclateCentre();
            }


            /**
             * @breif Binds the texture and vertex buffers to the given command buffer
             *
             * @param command_buffer command buffer to bind to
             * @param pipeline_layout layout of bindings
             * @param cframe current frame number
             */
            void render (
                const vk::CommandBuffer command_buffer,
                const vk::PipelineLayout pipeline_layout,
                const unsigned long cframe
            ) const override {
                command_buffer.bindDescriptorSets(
                    vk::PipelineBindPoint::eGraphics,
                    pipeline_layout,
                    2,
                    1,
                    &descriptor_sets_[cframe],
                    0,
                    nullptr
                );

                tex_->render(command_buffer, pipeline_layout);
                mesh_->render(command_buffer);
            }

            /**
             * @breif Allocates memory buffers for the UBOs
             *
             * @param max_f_frames max number of pre rendered frames
             * @param device current GPU (logical)
             * @param graphics_queue vk::Queue
             * @param command_pool pool used to allocate command buffers
             * @param physical_device current GPU (physical)
             */
            void initUBOs(
                const unsigned int max_f_frames,
                const vk::Device device,
                const vk::Queue graphics_queue,
                const vk::CommandPool command_pool,
                const vk::PhysicalDevice physical_device
            ) override {
                ubos_.resize(max_f_frames);
                for (unsigned int i = 0; i < max_f_frames; ++i) {
                    ubos_[i] = new MemoryBuffer (
                        sizeof(ModelObject),
                        vk::BufferUsageFlagBits::eUniformBuffer,
                        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
                        device,
                        graphics_queue,
                        command_pool,
                        physical_device
                    );
                }
            }

            /**
             * @breif Creates descriptor sets for all UBO buffers
             *
             * @param max_f_frames max number of pre rendered frames
             * @param descriptor_pool pool used to allocate descriptor sets
             * @param device current GPU (logical)
             * @param ubo_layout layout of UBO
             */
            void createDescriptorSets(
                const unsigned int max_f_frames,
                const vk::DescriptorPool descriptor_pool,
                const vk::Device device,
                const vk::DescriptorSetLayout ubo_layout
            ) override {
                std::vector<vk::DescriptorSetLayout> layouts (max_f_frames, ubo_layout);

                const vk::DescriptorSetAllocateInfo alloc_info {
                    descriptor_pool,
                    static_cast<uint32_t>(max_f_frames),
                    layouts.data()
                };

                descriptor_sets_.resize(max_f_frames);

                if (
                    device.allocateDescriptorSets(
                        &alloc_info,
                        descriptor_sets_.data()
                    ) != vk::Result::eSuccess
                ) throw std::runtime_error("ERR 14 ");

                for (int i = 0; i < max_f_frames; ++i) {
                    const vk::DescriptorBufferInfo buffer_info {
                        ubos_[i]->getBuffer(),
                        0,
                        sizeof(ModelObject)
                    };

                    const vk::WriteDescriptorSet descriptor_write {
                        descriptor_sets_[i],
                        1,
                        0,
                        1,
                        vk::DescriptorType::eUniformBuffer,
                        nullptr,
                        &buffer_info,
                        nullptr
                    };

                    device.updateDescriptorSets(1, &descriptor_write, 0, nullptr);
                }
            }

            /**
             * @breif Calculates centre of the mesh
             *
             * Method sets the internal value of centre_ by taking the average position of all the x, y and z values.
            */
            void caluclateCentre() override {
                centre_ = glm::vec3(0.0f);
                for (const auto& vert : mesh_->vertices_) centre_ += vert.pos;
                centre_ /= mesh_->vertices_.size();
            }

            /**
             * @breif Tells OpenCV VideoCapture to retrive the next frame
             *
             * This method only performs an operation if the texture stores a video. If enabled when this method is
             * called the OpenCV VideoCapture loads the next frame and converts it to the RGBA format.
            */
            void imageTick() override {
                if constexpr (tex_type == tdl::File::Video) tex_->setNextImage();
            }

            /**
             * @breif Copies frame data into the vk::Image of the texture
             *
             * This method only performs an operation if the texture stores a video. If enabled this method copies the
             * data loaded from the OpenCV VideoCapture into a vk::Image and reformats it. When the next frame is
             * rendered the contents of the new frame are displayed.
            */
            void frameTick() override {
                if constexpr (tex_type == tdl::File::Video) tex_->loadNextFrame();
            }

            Material material_;
            std::vector<vk::DescriptorSet> descriptor_sets_;
    };

    /**
     * @breif represents a group of objects
     *
     * Holds multiple objects and renders them, provides a transform that applies to all of them. Provides the []
     * operator to access objects by index or by name.
    */
    class Model {
        public:
            friend class Vlkn;
            friend class ThreeDL;
            friend class shared_model<Model>;

            /**
             * @breif Loads a model from OBJ file and material data from the MTL file
             *
             * Path to the MTL file is not explicitly given but read from the usemtl line in the OBJ file
             *
             * @param path path to the OBJ file
            */
            explicit Model (
                const std::string& path
            ) : centre_ {} {
                objects_ = OBJLoader::loadOBJ(path);
                caluclateCentre();

                keys_ = getKeys();
            }

            explicit Model (
                const std::vector<std::pair<std::string, std::shared_ptr<ObjectInterface>>>& objects
            ) : objects_ { objects },
                centre_ {}
            {
                caluclateCentre();
                keys_ = getKeys();
            }

            // loads model and textures it with given image
            Model (
                const std::string& path,
                const std::string& tex_path
            ) : centre_ {} {
                objects_ = OBJLoader::loadVWT(path, tex_path);
                caluclateCentre();

                keys_ = getKeys();
            }

            // loads model and gives it color
            Model (
                const std::string& path,
                const std::array<unsigned char, 4>& color
            ) : centre_ {} {
                objects_ = OBJLoader::loadVWC(path, color);
                caluclateCentre();

                keys_ = getKeys();
            }

            void rotate (
                const glm::vec3& angles,
                const glm::vec3& centre
            );

            void rotate (
                const glm::vec3& angles
            );

            void translate (
                const glm::vec3& val
            );

            shared_model<Model> operator[] (const std::string& name) const;

            [[nodiscard]] std::vector<std::string> getKeys() const;

            ~Model() {
                for (const auto& memory : ubos_) {
                    delete memory;
                }
            }
        private:
            void imageTick();
            void frameTick();

            void loadTexture (
                vk::Device device,
                vk::Queue graphics_queue,
                vk::CommandPool command_pool,
                vk::PhysicalDevice p_device,
                vk::DescriptorSetLayout layout,
                vk::DescriptorPool descriptor_pool
            ) const;

            void loadMesh(
                vk::Device device,
                vk::Queue graphics_queue,
                vk::CommandPool command_pool,
                vk::PhysicalDevice p_device
            );

            void render(
                vk::CommandBuffer command_buffer,
                vk::PipelineLayout pipeline_layout,
                unsigned long cframe
            );

            void initUBOs(
                unsigned int max_f_frames,
                vk::Device device,
                vk::Queue graphics_queue,
                vk::CommandPool command_pool,
                vk::PhysicalDevice physical_device
            );

            void createDescriptorSets(
                unsigned int max_f_frames,
                vk::DescriptorPool descriptor_pool,
                vk::Device device,
                vk::DescriptorSetLayout model_layout,
                vk::DescriptorSetLayout object_layout
            );

            void caluclateCentre();

            std::vector<std::pair<std::string, std::shared_ptr<ObjectInterface>>> objects_;
            std::vector<std::string> keys_;

            glm::vec3 centre_;

            ModelObject ubo_data_ {};
            std::vector<MemoryBuffer*> ubos_;
            std::vector<vk::DescriptorSet> descriptor_sets_;
            bool has_changed_ = true;
    };

    template <typename... Args>
    shared_model<Model> make_model (
        Args... args
    ) {
        return shared_model<Model>(args...);
    }
};