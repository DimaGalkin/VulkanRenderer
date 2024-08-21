#include "objects.hpp"

#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

#include <opencv2/imgproc.hpp>

#include <sstream>
#include <fstream>
#include <iostream>

vk::VertexInputBindingDescription tdl::Vertex::getBindingDescription() {
    return {
        0,
        sizeof(Vertex),
        vk::VertexInputRate::eVertex
    };
}

std::array<vk::VertexInputAttributeDescription, 3> tdl::Vertex::getAttributeDescriptions() {
    return {
        vk::VertexInputAttributeDescription {
            0,
            0,
            vk::Format::eR32G32B32Sfloat,
            offsetof(Vertex, pos)
        },
        vk::VertexInputAttributeDescription {
            1,
            0,
            vk::Format::eR32G32B32Sfloat,
            offsetof(Vertex, color)
        },
        vk::VertexInputAttributeDescription {
            2,
            0,
            vk::Format::eR32G32Sfloat,
            offsetof(Vertex, uv)
        }
    };
}

std::vector<std::string> tdl::OBJLoader::split(
    const std::string& str,
    const char delimiter
) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream {str};

    while (std::getline(tokenStream, token, delimiter)) {
        if (!token.empty()) tokens.push_back(token);
    }

    return tokens;
}

std::vector<std::pair<std::string, std::shared_ptr<tdl::ObjectInterface>>> tdl::OBJLoader::loadOBJ(
    const std::string& path
) {
    std::vector<std::pair<std::string, std::shared_ptr<tdl::ObjectInterface>>> objects;
    ObjectPtr object;
    std::string obj_name;
    bool first = true;

    std::vector<Material>  materials;
    size_t currrent_mtl = -1;

    std::vector<glm::vec3> vertices;
    std::vector<glm::vec2> uvs;
    std::vector<Vertex> verts;
    size_t start = 0;

    std::ifstream file { path };

    if (!file.is_open()) {
        throw std::runtime_error("ERR 015: Failed to open OBJ file with path: " + path);
    }

    for (std::string line; std::getline(file, line);) {
        const std::vector<std::string> tokens = split(line, ' ');

        if(tokens.empty()) continue;

        if (tokens[0] == "mtllib") {
            std::string mtlpath = getMTLPath(tokens[1], path);
            materials = loadMTL(mtlpath);
            continue;
        }

        if (tokens[0] == "usemtl") {
            if (!first) {
                std::vector<Vertex> vertss (&verts.at(start), &verts.back() + 1);
                auto mesh = std::make_shared<Mesh>(vertss);
                start = verts.size();
                object = std::make_shared<Object<tdl::File::Image>>(mesh, materials[currrent_mtl]);
                objects.emplace_back(obj_name, object);
            }
            first = false;

            for (int i = 0; i < materials.size(); ++i) {
                if (materials[i].name == tokens[1]) {
                    currrent_mtl = i;
                    break;
                }
            }

            continue;
        }

        if (tokens[0] == "o") {
            obj_name = tokens[1];
        }

        if (tokens[0] == "v") {
            vertices.emplace_back(
                std::stof(tokens[1]),
                -std::stof(tokens[2]),
                std::stof(tokens[3])
            );
        } else if (tokens[0] == "f") {
            auto v1 = split(tokens[1], '/');
            auto v2 = split(tokens[2], '/');
            auto v3 = split(tokens[3], '/');

            bool valid_uvs = true;
            if (v1[1].empty()) valid_uvs = false;

            verts.push_back({
                vertices[std::stoi(v1[0]) - 1],
                {1.0f, 1.0f, 1.0f},
                (valid_uvs) ? uvs[std::stoi(v1[1]) - 1] : glm::vec3(0)
            });

            verts.push_back({
                vertices[std::stoi(v2[0]) - 1],
                {1.0f, 1.0f, 1.0f},
                (valid_uvs) ? uvs[std::stoi(v2[1]) - 1] : glm::vec3(0)
            });

            verts.push_back({
                vertices[std::stoi(v3[0]) - 1],
                {1.0f, 1.0f, 1.0f},
                (valid_uvs) ? uvs[std::stoi(v3[1]) - 1] : glm::vec3(0)
            });
        } else if (tokens[0] == "vt") {
            uvs.emplace_back(
                std::stof(tokens[1]),
                1 - std::stof(tokens[2])
            );
        }
    }

    std::vector<Vertex> vertss (&verts.at(start), &verts.back() + 1);
    auto mesh = std::make_shared<Mesh>(vertss);
    object = std::make_shared<Object<tdl::File::Image>>(mesh, materials[currrent_mtl]);
    objects.emplace_back(obj_name, object);

    return objects;
}

std::vector<std::pair<std::string, std::shared_ptr<tdl::ObjectInterface>>> tdl::OBJLoader::loadVWT(
    const std::string& path,
    const std::string& tex_path
) {
    std::vector<std::pair<std::string, std::shared_ptr<tdl::ObjectInterface>>> objects;
    ObjectPtr object;
    std::string obj_name = "model";

    std::vector<glm::vec3> vertices;
    std::vector<glm::vec2> uvs;
    std::vector<Vertex> verts;

    std::ifstream file { path };

    if (!file.is_open()) {
        throw std::runtime_error("ERR 015: Failed to open OBJ file with path: " + path);
    }

    for (std::string line; std::getline(file, line);) {
        const std::vector<std::string> tokens = split(line, ' ');

        if (tokens[0] == "v") {
            vertices.emplace_back(
                std::stof(tokens[1]),
                -std::stof(tokens[2]),
                std::stof(tokens[3])
            );
        } else if (tokens[0] == "f") {
            auto v1 = split(tokens[1], '/');
            auto v2 = split(tokens[2], '/');
            auto v3 = split(tokens[3], '/');

            verts.push_back({
                vertices[std::stoi(v1[0]) - 1],
                {1.0f, 1.0f, 1.0f},
                uvs[std::stoi(v1[1]) - 1]
            });

            verts.push_back({
                vertices[std::stoi(v2[0]) - 1],
                {1.0f, 1.0f, 1.0f},
                uvs[std::stoi(v2[1]) - 1]
            });

            verts.push_back({
                vertices[std::stoi(v3[0]) - 1],
                {1.0f, 1.0f, 1.0f},
                uvs[std::stoi(v3[1]) - 1]
            });
        } else if (tokens[0] == "vt") {
            uvs.emplace_back(
                std::stof(tokens[1]),
                1 - std::stof(tokens[2])
            );
        }
    }

    Material mtl {};
    mtl.name = obj_name;
    mtl.diffuse_map_path = tex_path;
    mtl.diffuse_is_map = true;

    auto mesh = std::make_shared<Mesh>(verts);
    object = std::make_shared<Object<tdl::File::Image>>(mesh, mtl);
    objects.emplace_back(obj_name, object);

    return objects;
}

std::vector<std::pair<std::string, std::shared_ptr<tdl::ObjectInterface>>> tdl::OBJLoader::loadVWC(
    const std::string& path,
    const std::array<unsigned char, 4>& col
) {
    std::vector<std::pair<std::string, std::shared_ptr<tdl::ObjectInterface>>> objects;
    ObjectPtr object;
    std::string obj_name = "model";

    std::vector<glm::vec3> vertices;
    std::vector<glm::vec2> uvs;
    std::vector<Vertex> verts;

    std::ifstream file { path };

    if (!file.is_open()) {
        throw std::runtime_error("ERR 015: Failed to open OBJ file with path: " + path);
    }

    for (std::string line; std::getline(file, line);) {
        const std::vector<std::string> tokens = split(line, ' ');

        if (tokens[0] == "v") {
            vertices.emplace_back(
                std::stof(tokens[1]),
                -std::stof(tokens[2]),
                std::stof(tokens[3])
            );
        } else if (tokens[0] == "f") {
            auto v1 = split(tokens[1], '/');
            auto v2 = split(tokens[2], '/');
            auto v3 = split(tokens[3], '/');

            verts.push_back({
                vertices[std::stoi(v1[0]) - 1],
                {1.0f, 1.0f, 1.0f},
                uvs[std::stoi(v1[1]) - 1]
            });

            verts.push_back({
                vertices[std::stoi(v2[0]) - 1],
                {1.0f, 1.0f, 1.0f},
                uvs[std::stoi(v2[1]) - 1]
            });

            verts.push_back({
                vertices[std::stoi(v3[0]) - 1],
                {1.0f, 1.0f, 1.0f},
                uvs[std::stoi(v3[1]) - 1]
            });
        } else if (tokens[0] == "vt") {
            uvs.emplace_back(
                std::stof(tokens[1]),
                1 - std::stof(tokens[2])
            );
        }
    }

    Material mtl {};
    mtl.name = obj_name;
    //mtl.diffuse_ = col; // TODO: make this function actualy set the color of the object.

    auto mesh = std::make_shared<Mesh>(verts);
    object = std::make_shared<Object<tdl::File::Image>>(mesh, mtl);
    objects.emplace_back(obj_name, object);

    return objects;
}

std::vector<tdl::Material> tdl::OBJLoader::loadMTL(
    const std::string& path
) {
    std::vector<Material> materials;
    Material material;
    bool first = true;

    std::ifstream mtl { path };

    for (std::string line; std::getline(mtl, line);) {
        std::vector<std::string> tokens = split(line, ' ');
        if (tokens.empty()) continue;

        if (tokens[0] == "newmtl") {
            if (!first) {
                materials.push_back(material);
                material = {};
            }
            first = false;

            material.name = tokens[1];
        } else if (tokens[0] == "map_Kd") {
            material.diffuse_is_map = true;
            material.diffuse_map_path = getMTLPath(tokens[1], path);
        } else if (tokens[0] == "Kd") {
            material.diffuse = glm::vec3(
                std::stof(tokens[1]),
                std::stof(tokens[2]),
                std::stof(tokens[3])
            );
        }
    }

    materials.push_back(material);

    return materials;
}


std::string tdl::OBJLoader::getMTLPath(
    const std::string& name,
    const std::string& obj_path
) {
    std::string path = obj_path;
    path.erase(path.rfind('/') + 1);

    return path + name;
}


void tdl::Mesh::initBuffer(
    const vk::Device device,
    const vk::Queue graphics_queue,
    const vk::CommandPool command_pool,
    const vk::PhysicalDevice p_device
) {
    if (buffer_ != nullptr) return;

    buffer_ = new MemoryBuffer {
        vertices_.size() * sizeof(vertices_.at(0)),
        vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer,
        vk::MemoryPropertyFlagBits::eDeviceLocal,
        device,
        graphics_queue,
        command_pool,
        p_device
    };

    const MemoryBuffer copy_buffer {
        vertices_.size() * sizeof(vertices_.at(0)),
        vk::BufferUsageFlagBits::eTransferSrc,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
        device,
        graphics_queue,
        command_pool,
        p_device
    };

    copy_buffer.set(vertices_.data(), vertices_.size() * sizeof(vertices_.at(0)));
    buffer_->copy(copy_buffer, vertices_.size() * sizeof(vertices_.at(0)));
}

void tdl::Mesh::render(
    const vk::CommandBuffer& command_buffer
) const {
    const vk::Buffer buffers[] = { buffer_->getBuffer() };
    static constexpr vk::DeviceSize offsets[] = { 0 };
    command_buffer.bindVertexBuffers(0, 1, buffers, offsets);
    command_buffer.draw(static_cast<uint32_t>(vertices_.size()), 1, 0, 0);
}

void tdl::Mesh::load() {
    if (loaded_) return;
    //vertices_ = tdl::OBJLoader::load(path_);
    loaded_ = true;
}

void tdl::Texture::load(
    const vk::Device device,
    const vk::Queue graphics_queue,
    const vk::CommandPool command_pool,
    const vk::PhysicalDevice p_device,
    const vk::DescriptorSetLayout layout,
    const vk::DescriptorPool descriptor_pool
) {
    device_ = device;
    graphics_queue_ = graphics_queue;
    command_pool_ = command_pool;
    p_device_ = p_device;
    layout_ = layout;
    descriptor_pool_ = descriptor_pool;

    if (is_color_) {
        loadColor();
        return;
    }

    if (type_ == tdl::File::Image) {
        loadImage();
    } else if (type_ == tdl::File::Video) {
        loadVideo();
    } else {
        throw std::runtime_error("Type not set");
    }
}

void tdl::Texture::setNextImage() {
    frame_mutex_.lock();
    image_.buffer_->set(frame_data_.data, width_ * height_ * 4);
    frame_mutex_.unlock();

    Image::setImageLayout(
        image_.image_,
        vk::ImageLayout::eUndefined,
        vk::ImageLayout::eTransferDstOptimal,
        device_,
        command_pool_,
        graphics_queue_
    );

    image_.buffer_->asImage(image_.image_, width_, height_);

    Image::setImageLayout(
        image_.image_,
        vk::ImageLayout::eTransferDstOptimal,
        vk::ImageLayout::eShaderReadOnlyOptimal,
        device_,
        command_pool_,
        graphics_queue_
    );
}

void tdl::Texture::recreateImageView(
    const vk::Device device
) {
    image_.recreateImageView(device);
}

void tdl::Texture::render(
    const vk::CommandBuffer &command_buffer,
    const vk::PipelineLayout &pipeline_layout
) const {
    image_.render(command_buffer, pipeline_layout);
}

void tdl::Texture::loadNextFrame() {
    if (
        std::chrono::duration<float, std::chrono::milliseconds::period>(
            std::chrono::high_resolution_clock::now() - time_
        ).count() < (1000.0f/fps_)
    ) {
        return;
    }

    time_ = std::chrono::high_resolution_clock::now();

    if (!video_->grab()) {
        video_->release();
        video_ = std::make_unique<cv::VideoCapture>(path_ , cv::CAP_FFMPEG);
        video_->grab();
    }

    video_->retrieve(frame_);

    frame_mutex_.lock();

    cv::cvtColor(frame_, frame_data_, cv::COLOR_BGR2RGBA);

    frame_mutex_.unlock();
}

void tdl::Texture::loadImage() {
    if (loaded_) return;

    int width, height, channels;
    const stbi_uc* pixels = stbi_load(
        path_.c_str(),
        &width, &height,
        &channels,
        STBI_rgb_alpha
    );

    if (!pixels) {
        throw std::runtime_error("Failed to load texture image!");
    }

    image_.loadImage(
        pixels,
        static_cast<uint32_t>(width),
        static_cast<uint32_t>(height),
        device_,
        command_pool_,
        graphics_queue_,
        p_device_
    );

    stbi_image_free(const_cast<stbi_uc*>(pixels));

    image_.createSampler();

    image_.createDescriptor(
        layout_,
        descriptor_pool_
    );

    image_.updateDescriptor();

    loaded_ = true;
}

void tdl::Texture::loadVideo() {
    if (loaded_) return;

    video_ = std::make_unique<cv::VideoCapture>(path_, cv::CAP_FFMPEG);

    width_ = static_cast<uint32_t>(video_->get(cv::CAP_PROP_FRAME_WIDTH));
    height_ = static_cast<uint32_t>(video_->get(cv::CAP_PROP_FRAME_HEIGHT));
    fps_ = static_cast<float>(video_->get(cv::CAP_PROP_FPS));

    image_.setDevice(device_);
    image_.createDescriptor(
        layout_,
        descriptor_pool_
    );

    *video_ >> frame_;

    if (frame_.empty()) {
        video_->release();
        video_ = std::make_unique<cv::VideoCapture>(path_ , cv::CAP_FFMPEG);

        *video_ >> frame_;
    }

    cv::cvtColor(frame_, frame_data_, cv::COLOR_BGR2RGBA);

    image_.loadImage(
        frame_data_.data,
        width_,
        height_,
        device_,
        command_pool_,
        graphics_queue_,
        p_device_
    );

    image_.createSampler();
    image_.updateDescriptor();

    loaded_ = true;
}

void tdl::Texture::loadColor() {
    if (loaded_) return;

    image_.setDevice(device_);
    image_.createDescriptor(
        layout_,
        descriptor_pool_
    );

    image_.loadImage(
        color_.data(),
        1,
        1,
        device_,
        command_pool_,
        graphics_queue_,
        p_device_
    );

    image_.createSampler();
    image_.updateDescriptor();

    loaded_ = true;
}

void tdl::Model::rotate(
    const glm::vec3 &angles,
    const glm::vec3 &centre
) {
    ubo_data_.rotation = glm::translate(ubo_data_.rotation, centre);
    ubo_data_.rotation = glm::rotate(ubo_data_.rotation, angles.x, {1, 0, 0});
    ubo_data_.rotation = glm::rotate(ubo_data_.rotation, angles.y, {0, 1, 0});
    ubo_data_.rotation = glm::rotate(ubo_data_.rotation, angles.z, {0, 0, 1});
    ubo_data_.rotation = glm::translate(ubo_data_.rotation, -centre);

    has_changed_ = true;
}

void tdl::Model::rotate(
    const glm::vec3 &angles
) {
    ubo_data_.rotation = glm::translate(ubo_data_.rotation, centre_);
    ubo_data_.rotation = glm::rotate(ubo_data_.rotation, angles.x, {1, 0, 0});
    ubo_data_.rotation = glm::rotate(ubo_data_.rotation, angles.y, {0, 1, 0});
    ubo_data_.rotation = glm::rotate(ubo_data_.rotation, angles.z, {0, 0, 1});
    ubo_data_.rotation = glm::translate(ubo_data_.rotation, -centre_);

    has_changed_ = true;
}

void tdl::Model::translate(
    const glm::vec3 &val
) {
    ubo_data_.translation = glm::translate(ubo_data_.translation, val);
}

tdl::shared_model<tdl::Model> tdl::Model::operator[] (
    const std::string& name
) const {
    auto valid = objects_ | std::views::filter([ name ](
        const std::pair<std::string, std::shared_ptr<tdl::ObjectInterface>>& obj
    ) {
        return obj.first == name;
    });

    const std::vector<std::pair<std::string, std::shared_ptr<tdl::ObjectInterface>>> valid_vec = {valid.begin(), valid.end()};

    return shared_model<Model>(valid_vec);
}

std::vector<std::string> tdl::Model::getKeys() const {
    std::vector<std::string> keys;

    for (const auto& [name, _] : objects_) {
        keys.push_back(name);
    }

    return keys;
}

void tdl::Model::imageTick() {
    for (const auto &obj: objects_ | std::views::values) {
        obj->tex_->setNextImage();
    }
}

void tdl::Model::frameTick() {
    for (const auto &obj: objects_ | std::views::values) {
    obj->tex_->loadNextFrame();
    }
}

void tdl::Model::loadTexture(
    const vk::Device device,
    const vk::Queue graphics_queue,
    const vk::CommandPool command_pool,
    const vk::PhysicalDevice p_device,
    const vk::DescriptorSetLayout layout,
    const vk::DescriptorPool descriptor_pool
) const {
    for (const auto &obj: objects_ | std::views::values) {
        obj->loadTexture(
            device,
            graphics_queue,
            command_pool,
            p_device,
            layout,
            descriptor_pool
        );
    }
}

void tdl::Model::loadMesh(
    const vk::Device device,
    const vk::Queue graphics_queue,
    const vk::CommandPool command_pool,
    const vk::PhysicalDevice p_device
) {
    for (const auto &obj: objects_ | std::views::values) {
        obj->loadMesh(
            device,
            graphics_queue,
            command_pool,
            p_device
        );
    }
}

void tdl::Model::render(
    const vk::CommandBuffer command_buffer,
    const vk::PipelineLayout pipeline_layout,
    const unsigned long cframe
) {
    for (const auto &obj: objects_ | std::views::values) {
        obj->render(command_buffer, pipeline_layout, cframe);
    }

    command_buffer.bindDescriptorSets(
            vk::PipelineBindPoint::eGraphics,
            pipeline_layout,
            3,
            1,
            &descriptor_sets_[cframe],
            0,
            nullptr
    );
}

void tdl::Model::initUBOs(
    const unsigned int max_f_frames,
    const vk::Device device,
    const vk::Queue graphics_queue,
    const vk::CommandPool command_pool,
    const vk::PhysicalDevice physical_device
) {
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

    for (const auto &obj: objects_ | std::views::values) {
        obj->initUBOs(
            max_f_frames,
            device,
            graphics_queue,
            command_pool,
            physical_device
        );
    }
}

void tdl::Model::createDescriptorSets(
    const unsigned int max_f_frames,
    const vk::DescriptorPool descriptor_pool,
    const vk::Device device,
    const vk::DescriptorSetLayout model_layout,
    const vk::DescriptorSetLayout object_layout
) {
    for (const auto &obj: objects_ | std::views::values) {
        obj->createDescriptorSets(
            max_f_frames,
            descriptor_pool,
            device,
            object_layout
        );
    }

    std::vector<vk::DescriptorSetLayout> layouts (max_f_frames, model_layout);

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
            2,
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

void tdl::Model::caluclateCentre() {
    centre_ = glm::vec3 {0.0f, 0.0f, 0.0f};
    for (const auto &obj: objects_ | std::views::values) {
        obj->caluclateCentre();
        centre_ += obj->centre_;
    }
    centre_ /= objects_.size();
}

