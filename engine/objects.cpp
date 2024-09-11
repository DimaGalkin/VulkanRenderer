#include "objects.hpp"

#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

#include <opencv2/imgproc.hpp>

#include <sstream>
#include <fstream>

vk::VertexInputBindingDescription tdl::Vertex::getBindingDescription() {
    return {
        0,
        sizeof(Vertex), // Tell Vulkan size of one vertex
        vk::VertexInputRate::eVertex // Tell vulkan this binding is a vertex
    };
}

std::array<vk::VertexInputAttributeDescription, 3> tdl::Vertex::getAttributeDescriptions() {
    return {
        // vertex position at location 0
        vk::VertexInputAttributeDescription {
            0,
            0,
            vk::Format::eR32G32B32Sfloat, // vec3 float
            offsetof(Vertex, pos)
        },
        // vertex color at location 1
        vk::VertexInputAttributeDescription {
            1,
            0,
            vk::Format::eR32G32B32Sfloat, // vec3 float
            offsetof(Vertex, color)
        },
        // vertex uv coords at location 2
        vk::VertexInputAttributeDescription {
            2,
            0,
            vk::Format::eR32G32Sfloat, // vec2 float
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
    std::istringstream tokenStream { str }; // string stream to use std::getline

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
    size_t currrent_mtl = -1; // Material to apply to current object

    std::vector<glm::vec3> vertices;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec2> uvs;
    std::vector<Vertex> verts;
    size_t start = 0; // start of slice of current object in the verts vector

    std::ifstream file { path }; // open OBJ file

    if (!file.is_open()) {
        throw std::runtime_error("ERR 015: Failed to open OBJ file with path: " + path);
    }

    for (std::string line; std::getline(file, line);) {
        // in OBJ files tokens are seperated by a space
        const std::vector<std::string> tokens = split(line, ' ');

        if(tokens.empty()) continue; // no processing required for blank lines

        if (tokens[0] == "mtllib") {
            // load and parse MTL file
            std::string mtlpath = getMTLPath(tokens[1], path); // MTL file path is assumed to be relative
            materials = loadMTL(mtlpath);
            continue;
        }

        if (tokens[0] == "usemtl") {
            if (!first) {
                // take a slice of all the vertcies that the current material is applied to
                std::vector<Vertex> slice (&verts.at(start), &verts.back() + 1);
                auto mesh = std::make_shared<Mesh>(slice);
                start = verts.size(); // update location to start slice

                if (currrent_mtl < 0) { //NOLINT condition not 'always false'
                    throw std::runtime_error("ERR 60: Material requested but no materials have been loaded. OBJLoader::loadOBJ(...)"); //NOLINT code not 'unreachable'
                }

                // create and add new object
                object = std::make_shared<Object<tdl::File::Image>>(mesh, materials[currrent_mtl]);
                objects.emplace_back(obj_name, object);
            }
            first = false; // all subsequent calls of usemtl will create a new object

            // look up wanted material
            for (int i = 0; i < materials.size(); ++i) {
                if (materials[i].name == tokens[1]) {
                    currrent_mtl = i;
                    break;
                }
            }

            continue;
        }

        if (tokens[0] == "o") {
            obj_name = tokens[1]; // objects specifies the name of the current group of vertices
        }

        if (tokens[0] == "v") {
            // 'v' means that we read vertex coordinates
            vertices.emplace_back(
                std::stof(tokens[1]),
                -std::stof(tokens[2]),
                std::stof(tokens[3])
            );
        } else if (tokens[0] == "f") {
            // 'f' means we create a triangle from the data we have read so far
            auto v1 = split(tokens[1], '/');
            auto v2 = split(tokens[2], '/');
            auto v3 = split(tokens[3], '/');

            bool valid_uvs = true;
            if (v1[1].empty()) valid_uvs = false; // UV coord may be omitted: this stops a SEGFAULT

            // adds three contigous vertices into the array (Vulkan interperates this as a triangle)

            verts.push_back({
                vertices[std::stoi(v1[0]) - 1],
                normals[std::stoi(v1[2]) - 1],
                (valid_uvs) ? uvs[std::stoi(v1[1]) - 1] : glm::vec3(0)
            });

            verts.push_back({
                vertices[std::stoi(v2[0]) - 1],
                normals[std::stoi(v2[2]) - 1],
                (valid_uvs) ? uvs[std::stoi(v2[1]) - 1] : glm::vec3(0)
            });

            verts.push_back({
                vertices[std::stoi(v3[0]) - 1],
                normals[std::stoi(v3[2]) - 1],
                (valid_uvs) ? uvs[std::stoi(v3[1]) - 1] : glm::vec3(0)
            });
        } else if (tokens[0] == "vt") {
            // 'vt' texture coordinates are read
            uvs.emplace_back(
                std::stof(tokens[1]),
                1 - std::stof(tokens[2])
            );
        } else if (tokens[0] == "vn") {
            normals.emplace_back(
                std::stof(tokens[1]),
                -std::stof(tokens[2]),
                std::stof(tokens[3])
            );
        }
    }

    // load remaining vertices as the last object
    std::vector<Vertex> slice (&verts.at(start), &verts.back() + 1);
    auto mesh = std::make_shared<Mesh>(slice);
    object = std::make_shared<Object<tdl::File::Image>>(mesh, materials[currrent_mtl]);
    objects.emplace_back(obj_name, object);

    file.close();

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

    std::ifstream file { path }; // open OBJ file

    if (!file.is_open()) {
        throw std::runtime_error("ERR 015: Failed to open OBJ file with path: " + path);
    }

    for (std::string line; std::getline(file, line);) {
        const std::vector<std::string> tokens = split(line, ' ');

        if (tokens[0] == "v") {
            // position in 3D space
            vertices.emplace_back(
                std::stof(tokens[1]),
                -std::stof(tokens[2]),
                std::stof(tokens[3])
            );
        } else if (tokens[0] == "f") {
            // loads three vertices that form a triangle
            auto v1 = split(tokens[1], '/');
            auto v2 = split(tokens[2], '/');
            auto v3 = split(tokens[3], '/');

            // store three contiguous vertices that vulkan will interpret as a triangle
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
            // coordinates on a 2D plane (where to sample the texture)
            uvs.emplace_back(
                std::stof(tokens[1]),
                1 - std::stof(tokens[2])
            );
        }
    }

    Material mtl {}; // Model only has one material
    mtl.name = obj_name;
    mtl.diffuse_map_path = tex_path; // path to image
    mtl.diffuse_is_map = true; // texture is an image

    auto mesh = std::make_shared<Mesh>(verts);
    object = std::make_shared<Object<tdl::File::Image>>(mesh, mtl);
    objects.emplace_back(obj_name, object);

    file.close();

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

    std::ifstream file { path }; // open OBJ file

    if (!file.is_open()) {
        throw std::runtime_error("ERR 015: Failed to open OBJ file with path: " + path);
    }

    for (std::string line; std::getline(file, line);) {
        const std::vector<std::string> tokens = split(line, ' ');

        if (tokens[0] == "v") {
            // position in 3D space
            vertices.emplace_back(
                std::stof(tokens[1]),
                -std::stof(tokens[2]),
                std::stof(tokens[3])
            );
        } else if (tokens[0] == "f") {
            // loads three vertices to form a triangle
            auto v1 = split(tokens[1], '/');
            auto v2 = split(tokens[2], '/');
            auto v3 = split(tokens[3], '/');

            // three contigous vertices are written whih vulkan will interpret as a triangle

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
            // point on a 2D plane (where to sample the texture)
            uvs.emplace_back(
                std::stof(tokens[1]),
                1 - std::stof(tokens[2])
            );
        }
    }

    Material mtl {}; // single material
    mtl.name = obj_name;
    mtl.diffuse = {
        col[0] / 255.0,
        col[1] / 255.0,
        col[2] / 255.0
    }; // convert RGBA to linear color

    auto mesh = std::make_shared<Mesh>(verts);
    object = std::make_shared<Object<tdl::File::Image>>(mesh, mtl);
    objects.emplace_back(obj_name, object);

    file.close();

    return objects;
}

std::vector<tdl::Material> tdl::OBJLoader::loadMTL(
    const std::string& path
) {
    std::vector<Material> materials;
    Material material;
    bool first = true;

    std::ifstream mtl { path }; // open MTL file

    for (std::string line; std::getline(mtl, line);) {
        std::vector<std::string> tokens = split(line, ' ');
        if (tokens.empty()) continue;

        if (tokens[0] == "newmtl") {
            if (!first) {
                materials.push_back(material);
                material = {};
            }
            first = false; // add new materials for all subsequent calls

            material.name = tokens[1]; // newmtl provides name of material
        } else if (tokens[0] == "map_Kd") {
            material.diffuse_is_map = true; // map_ specifies this is image
            material.diffuse_map_path = getMTLPath(tokens[1], path); // path to image
        } else if (tokens[0] == "Kd") {
            // no map_ means this is a single colour
            material.diffuse = glm::vec3(
                std::stof(tokens[1]),
                std::stof(tokens[2]),
                std::stof(tokens[3])
            );
        }
    }

    materials.push_back(material); // add final material

    mtl.close();

    return materials;
}


std::string tdl::OBJLoader::getMTLPath(
    const std::string& name,
    const std::string& obj_path
) {
    std::string path = obj_path;
    path.erase(path.rfind('/') + 1); // erase name of OBJ file from path

    return path + name; // add name of MTL file to path
}


void tdl::Mesh::initBuffer(
    const vk::Device device,
    const vk::Queue graphics_queue,
    const vk::CommandPool command_pool,
    const vk::PhysicalDevice p_device
) {
    if (buffer_ != nullptr) return; // do not re-initialse buffer

    // create new buffer on the heap
    buffer_ = new MemoryBuffer {
        vertices_.size() * sizeof(vertices_.at(0)),
        vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer,
        vk::MemoryPropertyFlagBits::eDeviceLocal,
        device,
        graphics_queue,
        command_pool,
        p_device
    };

    // a staging buffer improves performance
    const MemoryBuffer copy_buffer {
        vertices_.size() * sizeof(vertices_.at(0)),
        vk::BufferUsageFlagBits::eTransferSrc,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
        device,
        graphics_queue,
        command_pool,
        p_device
    };

    // write to staging buffer
    copy_buffer.set(vertices_.data(), vertices_.size() * sizeof(vertices_.at(0)));
    // copy staging buffer into main buffer
    buffer_->copy(copy_buffer, vertices_.size() * sizeof(vertices_.at(0)));
}

void tdl::Mesh::render(
    const vk::CommandBuffer command_buffer
) const {
    const vk::Buffer buffers[] = { buffer_->getBuffer() }; // buffer containing triangles
    static constexpr vk::DeviceSize offsets[] = { 0 };
    command_buffer.bindVertexBuffers(0, 1, buffers, offsets); // bind buffer to command buffer
    // draw all the vertices in the buffer
    command_buffer.draw(static_cast<uint32_t>(vertices_.size()), 1, 0, 0);
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

    // select how to load data into the vk::Image

    if (is_color_) {
        loadColor();
        return;
    }

    if (type_ == tdl::File::Image) {
        loadImage();
    } else if (type_ == tdl::File::Video) {
        loadVideo();
    } else {
        throw std::runtime_error("ERR 061: Unkown texture type. Texture::load(...)");
    }
}

void tdl::Texture::setNextImage() {
    // use mutex to tell the animation thread that the image is being loaded and can not be
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

    // update image with new frame data
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

void tdl::Texture::render(
    const vk::CommandBuffer command_buffer,
    const vk::PipelineLayout pipeline_layout
) const {
    // binds image to command buffer
    image_.render(command_buffer, pipeline_layout);
}

void tdl::Texture::loadNextFrame() {
    // match frame rate to video so video apears to playback smoothly
    if (
        std::chrono::duration<float, std::chrono::milliseconds::period>(
            std::chrono::high_resolution_clock::now() - time_
        ).count() < (1000.0f / fps_)
    ) {
        return;
    }

    time_ = std::chrono::high_resolution_clock::now();

    // if video has ended restart it
    if (!video_->grab()) {
        video_->release();
        video_ = std::make_unique<cv::VideoCapture>(path_ , cv::CAP_FFMPEG);
        video_->grab();
    }

    video_->retrieve(frame_); // grab next frame

    // mutex to tell main thread that the frame is being converted
    frame_mutex_.lock();

    // convert from BGR to RGBA format
    cv::cvtColor(frame_, frame_data_, cv::COLOR_BGR2RGBA);

    frame_mutex_.unlock();
}

void tdl::Texture::loadImage() {
    if (loaded_) return; // do not load image > 1

    int width, height, channels;
    const stbi_uc* pixels = stbi_load(
        path_.c_str(),
        &width, &height,
        &channels,
        STBI_rgb_alpha
    );

    if (!pixels) { // image has not loaded
        throw std::runtime_error("Failed to load texture image!");
    }

    // loads array of unsigned chars into vk::Image
    image_.loadImage(
        pixels,
        static_cast<uint32_t>(width),
        static_cast<uint32_t>(height),
        device_,
        command_pool_,
        graphics_queue_,
        p_device_
    );

    stbi_image_free(const_cast<stbi_uc*>(pixels)); // release stb representation of image

    image_.createSampler();

    image_.createDescriptor(
        layout_,
        descriptor_pool_
    );

    image_.updateDescriptor();

    loaded_ = true; // stop this code being excecuted again
}

void tdl::Texture::loadVideo() {
    if (loaded_) return; // only let this code run once

    // load video
    video_ = std::make_unique<cv::VideoCapture>(path_, cv::CAP_FFMPEG);

    // retrive required parameters from video
    width_ = static_cast<uint32_t>(video_->get(cv::CAP_PROP_FRAME_WIDTH));
    height_ = static_cast<uint32_t>(video_->get(cv::CAP_PROP_FRAME_HEIGHT));
    fps_ = static_cast<float>(video_->get(cv::CAP_PROP_FPS));

    image_.setDevice(device_);
    image_.createDescriptor(
        layout_,
        descriptor_pool_
    );

    *video_ >> frame_; // load first frame

    if (frame_.empty()) {
        throw std::runtime_error("ERR 062: Could not load first frame of video texture! Texture::loadVideo(...)");
    }

    // convert image format from BGR to RGBA
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

    loaded_ = true; // stop this code from bering run again
}

void tdl::Texture::loadColor() {
    if (loaded_) return; // stop this code from being run multiple times

    image_.setDevice(device_);
    image_.createDescriptor(
        layout_,
        descriptor_pool_
    );

    // set texture as a 1x1 pixel of a single color
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

    loaded_ = true; // stop this code from being run again
}

void tdl::Model::rotate(
    const glm::vec3& angles,
    const glm::vec3& centre
) {
    // rotate model around a given centre by translating it and then rotating it

    ubo_data_.rotation = glm::translate(ubo_data_.rotation, centre);
    ubo_data_.rotation = glm::rotate(ubo_data_.rotation, angles.x, {1, 0, 0});
    ubo_data_.rotation = glm::rotate(ubo_data_.rotation, angles.y, {0, 1, 0});
    ubo_data_.rotation = glm::rotate(ubo_data_.rotation, angles.z, {0, 0, 1});
    ubo_data_.rotation = glm::translate(ubo_data_.rotation, -centre);

    has_changed_ = true; // tell ThreeDL that the matrix has changed since it was last retrived
}

void tdl::Model::rotate(
    const glm::vec3& angles
) {
    // rotate model around a given centre by translating it and then rotating it

    ubo_data_.rotation = glm::translate(ubo_data_.rotation, centre_);
    ubo_data_.rotation = glm::rotate(ubo_data_.rotation, angles.x, {1, 0, 0});
    ubo_data_.rotation = glm::rotate(ubo_data_.rotation, angles.y, {0, 1, 0});
    ubo_data_.rotation = glm::rotate(ubo_data_.rotation, angles.z, {0, 0, 1});
    ubo_data_.rotation = glm::translate(ubo_data_.rotation, -centre_);

    has_changed_ = true; // tell ThreeDL that the matrix has changed since it was last retrived
}

void tdl::Model::translate(
    const glm::vec3& val
) {
    ubo_data_.translation = glm::translate(ubo_data_.translation, val);
}

tdl::shared_model<tdl::Model> tdl::Model::operator[] (
    const std::string& name
) const {
    // filter out all objects that don't meet the name requirements using a filter and a lambda function
    auto valid = objects_ | std::views::filter([ name ](
        const std::pair<std::string, std::shared_ptr<tdl::ObjectInterface>>& obj
    ) {
        return obj.first == name;
    });

    // construct a vector by copying data from filter_view
    const std::vector<std::pair<std::string, std::shared_ptr<tdl::ObjectInterface>>> valid_vec {
        valid.begin(),
        valid.end()
    };

    return shared_model<Model>(valid_vec); // construct Model using vector
}

std::vector<std::string> tdl::Model::getKeys() const {
    // retrive all of the names of the objects
    const auto keys = objects_ | std::views::keys;
    return { keys.begin(), keys.end() };
}

void tdl::Model::imageTick() {
    for (const auto &obj: objects_ | std::views::values) {
        obj->tex_->setNextImage(); // load all texture frames as images
    }
}

void tdl::Model::frameTick() {
    for (const auto &obj: objects_ | std::views::values) {
        obj->tex_->loadNextFrame(); // tick all video textures to next frame
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
    // bind model UBO
    command_buffer.bindDescriptorSets(
            vk::PipelineBindPoint::eGraphics,
            pipeline_layout,
            3,
            1,
            &descriptor_sets_[cframe],
            0,
            nullptr
    );

    // bind all object UBOs
    for (const auto &obj: objects_ | std::views::values) {
        obj->render(command_buffer, pipeline_layout, cframe);
    }
}

void tdl::Model::initUBOs(
    const unsigned int max_f_frames,
    const vk::Device device,
    const vk::Queue graphics_queue,
    const vk::CommandPool command_pool,
    const vk::PhysicalDevice physical_device
) {
    // allocate space for the model UBO
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

    // allocate space for object UBOs
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
            sizeof(ModelObject) // UBO contains a ModelObject
        };

        const vk::WriteDescriptorSet descriptor_write {
            descriptor_sets_[i],
            2,
            0,
            1,
            vk::DescriptorType::eUniformBuffer, // descriptor contains a uniform buffer
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
        centre_ += obj->centre_; // get sum of all positions
    }
    centre_ /= objects_.size(); // get average position
}
