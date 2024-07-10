#include "vulkan-utils.hpp"

std::vector<const char*> VulkanUtils::getRequiredExtensions() {
    uint32_t glfwExtensionCount = 0;
    const char** glfw_extensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    return {
        glfw_extensions,
        glfw_extensions + glfwExtensionCount
    };
}

std::vector<char> VulkanUtils::readFile(
    const std::string& filename
) {
    std::ifstream file { filename, std::ios::ate | std::ios::binary };

    if (!file.is_open()) throw std::runtime_error("failed to open file!");

    const size_t fileSize = file.tellg();
    std::vector<char> buffer (fileSize);

    file.seekg(0);
    file.read(buffer.data(), static_cast<std::streamsize>(fileSize));

    file.close();

    return buffer;
}

bool VulkanUtils::checkDeviceExtensionSupport(
    const vk::PhysicalDevice& device
) {
    std::set<std::string> required {
        DEVICE_EXTENSIONS.begin(),
        DEVICE_EXTENSIONS.end()
    };

    for (const auto& extension : device.enumerateDeviceExtensionProperties()) {
        required.erase(extension.extensionName);
    }

    return required.empty();
}

vk::SurfaceFormatKHR VulkanUtils::chooseFormat(
    const std::vector<vk::SurfaceFormatKHR>& available_formats
) {
    if (
        available_formats.size() == 1 &&
        available_formats[0].format == vk::Format::eUndefined
    ) return { vk::Format::eB8G8R8A8Unorm, vk::ColorSpaceKHR::eSrgbNonlinear };

    for (const auto& available : available_formats) {
        if (
            available.format == vk::Format::eB8G8R8A8Unorm &&
            available.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear
        ) return available;
    }

    return available_formats[0];
}

vk::PresentModeKHR VulkanUtils::chooseMode(
    const std::vector<vk::PresentModeKHR>& available_modes
) {
    auto best = vk::PresentModeKHR::eFifo;

    for (const auto& available_mode : available_modes) {
        if (available_mode == vk::PresentModeKHR::eMailbox) {
            return available_mode;
        }

        if (available_mode == vk::PresentModeKHR::eImmediate) {
            best = available_mode;
        }
    }

    return best;
}

void Vlkn::init() {
    createInstance();
    createSurface();
    pickPhysicalDevice();
    createLogicalDevice();
    createSwapchain();
    createImageViews();
    createRenderPass();
    createDescriptorSetLayout();
    createGraphicsPipeline();
    createFramebuffers();
    createCommandPool();
    createUniformBuffers();
    createDescriptorPool();
    createDescriptorSets();
    loadModels();
    startCommandBuffers();
    createSyncObjects();
};

void Vlkn::newFrame(
    const UniformBufferObject& ubo
) {
    generateUBO(ubo);

    if (
        device_.waitForFences(
            1, &fences_[current_frame_],
            VK_TRUE, std::numeric_limits<uint64_t>::max()
        ) != vk::Result::eSuccess
    ) throw std::runtime_error("ERR 20");

    uint32_t idx;
    try {
        const vk::ResultValue result = device_.acquireNextImageKHR(
            swapchain_,
            std::numeric_limits<uint64_t>::max(),
            image_available_[current_frame_], nullptr
        );
        idx = result.value;
    } catch (const vk::OutOfDateKHRError& err) {
        recreateSwapchain();
        return;
    } catch (const vk::SystemError& err) {
        throw std::runtime_error("ERR 21");
    }

    submitForDraw(command_buffers_[idx], idx);

    const vk::PresentInfoKHR present_info {
        1,
        &render_finished_[current_frame_],
        1,
        &swapchain_,
        &idx
    };

    vk::Result r_present;
    try {
        r_present = present_queue_.presentKHR(present_info);
    } catch (const vk::OutOfDateKHRError& err) {
        r_present = vk::Result::eErrorOutOfDateKHR;
    } catch (const vk::SystemError& err) {
        throw std::runtime_error("ERR 24");
    }

    if (r_present == vk::Result::eSuboptimalKHR || resized_) {
        resized_ = false;
        recreateSwapchain();
        return;
    }

    current_frame_ = (current_frame_ + 1) % max_f_frames_;
}

void Vlkn::submitForDraw(
    const vk::CommandBuffer& buffer,
    const uint32_t idx
) {
    vk::PipelineStageFlags waitStages = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    const vk::SubmitInfo submit_info = {
        1,
        &image_available_[current_frame_],
        &waitStages,
        1,
        &buffer,
        1,
        &render_finished_[current_frame_]
    };

    if (
        device_.resetFences(
            1,
            &fences_[current_frame_]
        ) != vk::Result::eSuccess
    ) throw std::runtime_error("ERR 22");

    try {
        graphics_queue_.submit(submit_info, fences_[current_frame_]);
    } catch (const vk::SystemError& err) {
        throw std::runtime_error("ERR 23");
    }
}

void Vlkn::cleanupSwapchain() {
    for (const auto& framebuffer : framebuffers_) device_.destroyFramebuffer(framebuffer);
    for (const auto& image_view : image_views_) device_.destroyImageView(image_view);

    device_.destroySwapchainKHR(swapchain_);

    for (const auto& group : command_buffers_)
        device_.freeCommandBuffers(command_pool_, group);

    device_.destroyPipeline(graphics_pipeline_);
    device_.destroyPipelineLayout(pipeline_layout_);
    device_.destroyRenderPass(render_pass_);
}

void Vlkn::cleanup() {
    cleanupSwapchain();

    device_.destroyCommandPool(command_pool_);
    device_.destroyDescriptorSetLayout(ubo_layout_);
    device_.destroyDescriptorPool(descriptor_pool_);

    for (size_t i = 0; i < max_f_frames_; ++i) {
        device_.destroyFence(fences_[i]);
        device_.destroySemaphore(render_finished_[i]);
        device_.destroySemaphore(image_available_[i]);

        delete uniform_buffers_[i];
    }

    instance_->destroySurfaceKHR(surface_);

    glfwDestroyWindow(info_.window_);
    glfwTerminate();
}

void Vlkn::recreateSwapchain() {
    info_.width_ = 0; info_.height_ = 0;
    while (info_.width_ == 0 || info_.height_ == 0) {
        glfwGetFramebufferSize(info_.window_, &info_.width_, &info_.height_);
        glfwWaitEvents();
    }

    device_.waitIdle();

    cleanupSwapchain();
    createSwapchain();
    createImageViews();
    createRenderPass();
    createGraphicsPipeline();
    createFramebuffers();
    startCommandBuffers();
}

void Vlkn::createInstance() {
    const vk::ApplicationInfo application_info = {
        info_.title_.c_str(),
        VK_MAKE_VERSION(0, 0, 0),
        "ThreeDL Engine",
        VK_MAKE_VERSION(0, 0, 0),
        VK_API_VERSION_1_0
    };

    const std::vector<const char*> extensions = VulkanUtils::getRequiredExtensions();
    const vk::InstanceCreateInfo instance_info = {
        {},
        &application_info,
        0, nullptr,
        static_cast<uint32_t>(extensions.size()), extensions.data()
    };

    try {
        instance_ = vk::createInstanceUnique(instance_info, nullptr);
    } catch (const vk::SystemError& err) {
        throw std::runtime_error("ERR 1");
    }
}

void Vlkn::createSurface() {
    VkSurfaceKHR surface;
    if (glfwCreateWindowSurface(instance_.get(), info_.window_, nullptr, &surface) != VK_SUCCESS) {
        throw std::runtime_error("ERR 2");
    }

    surface_ = surface;
}

void Vlkn::pickPhysicalDevice() {
    std::vector<vk::PhysicalDevice> devices = instance_->enumeratePhysicalDevices();
    if (devices.empty()) throw std::runtime_error("ERR 3");

    for (const auto& device : devices) {
        if (isDeviceSuitable(device)) { physical_device_ = device; break; }
    }
    if (!physical_device_) throw std::runtime_error("ERR 4");
}

void Vlkn::createLogicalDevice() {
    const auto [graphics, present] = findQueueFamilies(physical_device_);

    std::vector<vk::DeviceQueueCreateInfo> info_group;
    const std::set<uint32_t> unique = { graphics.value(), present.value() };

    float priority = 1.0f;
    for (const uint32_t family : unique) {
        info_group.emplace_back( vk::DeviceQueueCreateFlags {}, family, 1, &priority);
    }

    static constexpr vk::PhysicalDeviceFeatures features = {};
    const vk::DeviceCreateInfo device_info = {
        {},
        static_cast<uint32_t>(info_group.size()), info_group.data(),
        0, nullptr,
        static_cast<uint32_t>(DEVICE_EXTENSIONS.size()), DEVICE_EXTENSIONS.data(),
        &features
    };

    try {
        device_ = physical_device_.createDevice(device_info);
    }
    catch (const vk::SystemError& err) {
        throw std::runtime_error("ERR 5");
    }

    graphics_queue_ = device_.getQueue(graphics.value(), 0);
    present_queue_ = device_.getQueue(present.value(), 0);
}

void Vlkn::createSwapchain() {
    const SwapChainSupportDetails sc_support = {
        physical_device_.getSurfaceCapabilitiesKHR(surface_),
        physical_device_.getSurfaceFormatsKHR(surface_),
        physical_device_.getSurfacePresentModesKHR(surface_)
    };
    const vk::SurfaceFormatKHR surfaceFormat = VulkanUtils::chooseFormat(sc_support.formats);
    const vk::PresentModeKHR presentMode = VulkanUtils::chooseMode(sc_support.presentModes);
    const vk::Extent2D extent = chooseExtent(sc_support.capabilities);

    uint32_t imageCount = sc_support.capabilities.minImageCount + 1;
    if (
        sc_support.capabilities.maxImageCount > 0 &&
        imageCount > sc_support.capabilities.maxImageCount
    ) { imageCount = sc_support.capabilities.maxImageCount; }

    vk::SwapchainCreateInfoKHR info {
        {},
        surface_,
        imageCount,
        surfaceFormat.format,
        surfaceFormat.colorSpace,
        extent,
        1,
        vk::ImageUsageFlagBits::eColorAttachment
    };

    const auto [graphics, present] = findQueueFamilies(physical_device_);
    const uint32_t queueFamilyIndices[] = { graphics.value(), present.value() };

    if (graphics != present) {
        info.imageSharingMode = vk::SharingMode::eConcurrent;
        info.queueFamilyIndexCount = 2;
        info.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        info.imageSharingMode = vk::SharingMode::eExclusive;
    }
    info.presentMode = presentMode;
    info.clipped = VK_TRUE;
    info.preTransform = sc_support.capabilities.currentTransform;
    info.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
    info.oldSwapchain = vk::SwapchainKHR {nullptr};

    try {
        swapchain_ = device_.createSwapchainKHR(info);
    } catch (const vk::SystemError& err) {
        throw std::runtime_error("ERR 6");
    }

    images_ = device_.getSwapchainImagesKHR(swapchain_);
    extent_ = extent;
    format_ = surfaceFormat.format;
}

void Vlkn::createImageViews() {
    image_views_.resize(images_.size());

    for (size_t i = 0; i < images_.size(); i++) {
        try {
            image_views_[i] = device_.createImageView({
                    {},
                    images_[i],
                    vk::ImageViewType::e2D,
                    format_,
                    {},
                    {
                        vk::ImageAspectFlagBits::eColor,
                        0,
                        1,
                        0,
                        1
                    }
                }
            );
        } catch (const vk::SystemError& err) {
            throw std::runtime_error("failed to create image views!");
        }
    }
}

void Vlkn::createRenderPass() {
    const vk::AttachmentDescription color {
                    {},
                    format_,
                    vk::SampleCountFlagBits::e1,
                    vk::AttachmentLoadOp::eClear,
                    vk::AttachmentStoreOp::eStore,
                    vk::AttachmentLoadOp::eDontCare,
                    vk::AttachmentStoreOp::eDontCare,
                    vk::ImageLayout::eUndefined,
                    vk::ImageLayout::ePresentSrcKHR
                };

    static constexpr vk::AttachmentReference color_ref {
        0,
        vk::ImageLayout::eColorAttachmentOptimal
    };
    static constexpr vk::SubpassDescription subpass {
                    {},
                    vk::PipelineBindPoint::eGraphics,
                    0, nullptr,
                    1, &color_ref
                };
    static constexpr vk::SubpassDependency dependency {
        VK_SUBPASS_EXTERNAL,
        0,
        vk::PipelineStageFlagBits::eColorAttachmentOutput,
        vk::PipelineStageFlagBits::eColorAttachmentOutput,
        {},
        vk::AccessFlagBits::eColorAttachmentWrite
    };

    try {
        render_pass_ = device_.createRenderPass({
            {},
            1,
            &color,
            1,
            &subpass,
            1,
            &dependency
        });
    } catch (const vk::SystemError& e) {
        throw std::runtime_error("ERR 8");
    }
}

void Vlkn::createFramebuffers() {
    framebuffers_.resize(image_views_.size());

    for (size_t i = 0; i < image_views_.size(); i++) {
        const vk::ImageView attachments[] = { image_views_[i] };

        vk::FramebufferCreateInfo framebufferInfo = {
            {},
            render_pass_,
            1,
            attachments,
            extent_.width,
            extent_.height,
            1
        };

        try {
            framebuffers_[i] = device_.createFramebuffer(framebufferInfo);
        } catch (const vk::SystemError& err) {
            throw std::runtime_error("ERR 11");
        }
    }
}

void Vlkn::createCommandPool() {
    const auto [graphics, _] = findQueueFamilies(physical_device_);
    const vk::CommandPoolCreateInfo pool_info = {
        {},
        graphics.value()
    };

    try {
        command_pool_ = device_.createCommandPool(pool_info);
    } catch (const vk::SystemError& err) {
        throw std::runtime_error("ERR 12");
    }
}

void Vlkn::loadModels() {
    for (const auto& object : objects_) {
        object->loadMesh(device_, graphics_queue_, command_pool_, physical_device_);

        object->loadTexture(
            device_,
            graphics_queue_,
            command_pool_,
            physical_device_,
            texture_layout_,
            descriptor_pool_
        );
    }
}


void Vlkn::startCommandBuffers() {
    command_buffers_.resize(framebuffers_.size());

    const vk::CommandBufferAllocateInfo alloc_info = {
        command_pool_,
        vk::CommandBufferLevel::ePrimary,
        static_cast<uint32_t>(command_buffers_.size())
    };

    try {
        command_buffers_ = device_.allocateCommandBuffers(alloc_info);
    } catch (const vk::SystemError& err) {
        throw std::runtime_error("ERR 16");
    }

    for (size_t i = 0; i < command_buffers_.size(); i++) {
        static constexpr vk::CommandBufferBeginInfo begin_info {
            vk::CommandBufferUsageFlagBits::eSimultaneousUse
        };

        try {
            command_buffers_[i].begin(begin_info);
        } catch (const vk::SystemError& err) {
            throw std::runtime_error("ERR 17");
        }

        vk::ClearValue clearColor = { std::array<float, 4>{ 0.0f, 0.0f, 0.0f, 1.0f } };

        const vk::RenderPassBeginInfo pass_info = {
            render_pass_,
            framebuffers_[i],
            {
                {0, 0},
                extent_
            },
            1,
            &clearColor
        };

        command_buffers_[i].beginRenderPass(pass_info, vk::SubpassContents::eInline);
        command_buffers_[i].bindPipeline(vk::PipelineBindPoint::eGraphics, graphics_pipeline_);
        command_buffers_[i].bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline_layout_, 0, 1, &descriptor_sets_[current_frame_], 0, nullptr);

        for (const auto& object : objects_) {
            object->render(command_buffers_[i], pipeline_layout_);
        }

        command_buffers_[i].endRenderPass();

        try {
            command_buffers_[i].end();
        } catch (const vk::SystemError& err) {
            throw std::runtime_error("ERR 18");
        }
    }
}

void Vlkn::createSyncObjects() {
    image_available_.resize(max_f_frames_);
    render_finished_.resize(max_f_frames_);
    fences_.resize(max_f_frames_);

    try {
        for (size_t i = 0; i < max_f_frames_; i++) {
            fences_[i] = device_.createFence({vk::FenceCreateFlagBits::eSignaled});
            image_available_[i] = device_.createSemaphore({});
            render_finished_[i] = device_.createSemaphore({});
        }
    } catch (const vk::SystemError& err) {
        throw std::runtime_error("ERR 19");
    }
}

void Vlkn::createGraphicsPipeline() {
    const vk::UniqueShaderModule vert = createShaderModule(VulkanUtils::readFile("../shaders/vert.spv"));
    const vk::UniqueShaderModule frag = createShaderModule(VulkanUtils::readFile("../shaders/frag.spv"));

    const vk::PipelineShaderStageCreateInfo stages[] = {
        {
            {},
            vk::ShaderStageFlagBits::eVertex,
            vert.get(),
            "main"
        },
        {
            {},
            vk::ShaderStageFlagBits::eFragment,
            frag.get(),
            "main"
        }
    };

    const vk::VertexInputBindingDescription binding = Vertex::getBindingDescription();
    const std::array<vk::VertexInputAttributeDescription, 3> attributes = Vertex::getAttributeDescriptions();

    const vk::PipelineVertexInputStateCreateInfo vertex_info {
        {},
        1,
        &binding,
        static_cast<uint32_t>(attributes.size()),
        attributes.data()
    };

    static constexpr vk::PipelineInputAssemblyStateCreateInfo assembly {
        {},
        vk::PrimitiveTopology::eTriangleList,
        VK_FALSE
    };

    const vk::Viewport viewport = {
        0.0f, 0.0f,
        static_cast<float>(extent_.width),
        static_cast<float>(extent_.height),
        0.0f, 1.0f
    };

    const vk::Rect2D scissor = {
        {0, 0},
        extent_
    };

    vk::PipelineViewportStateCreateInfo viewport_info = {
        {},
        1,
        &viewport,
        1,
        &scissor
    };

    static constexpr vk::PipelineRasterizationStateCreateInfo rasterizer {
        {},
        VK_FALSE,
        VK_FALSE,
        vk::PolygonMode::eFill,
        vk::CullModeFlagBits::eFront,
        vk::FrontFace::eClockwise,
        VK_FALSE,
        0.0f,
        0.0f,
        0.0f,
        1.0f
    };

    static constexpr vk::PipelineMultisampleStateCreateInfo multisampling {
        {},
        vk::SampleCountFlagBits::e1,
        VK_FALSE,
        1.0f,
        nullptr,
        VK_FALSE,
        VK_FALSE
    };

    static constexpr vk::PipelineColorBlendAttachmentState blending_attach {
        VK_FALSE,
        vk::BlendFactor::eOne,
        vk::BlendFactor::eZero,
        vk::BlendOp::eAdd,
        vk::BlendFactor::eOne,
        vk::BlendFactor::eZero,
        vk::BlendOp::eAdd,
        vk::ColorComponentFlags(vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA)
    };

    static constexpr vk::PipelineColorBlendStateCreateInfo blending = {
        {},
        VK_FALSE,
        vk::LogicOp::eCopy,
        1,
        &blending_attach,
        {0.0f, 0.0f, 0.0f, 0.0f}
    };

    vk::DescriptorSetLayout layouts[] = { ubo_layout_, texture_layout_ };

    const vk::PipelineLayoutCreateInfo pipe_info = {
        {},
        2,
        layouts
    };

    try {
        pipeline_layout_ = device_.createPipelineLayout(pipe_info);
    } catch (const vk::SystemError& err) {
        throw std::runtime_error("ERR 9");
    }

    const vk::GraphicsPipelineCreateInfo info {
        {},
        2,
        stages,
        &vertex_info,
        &assembly,
        nullptr,
        &viewport_info,
        &rasterizer,
        &multisampling,
        nullptr,
        &blending,
        nullptr,
        pipeline_layout_,
        render_pass_,
        0,
        nullptr,
        -1
    };

    try {
        graphics_pipeline_ = device_.createGraphicsPipeline(nullptr, info).value;
    } catch (const vk::SystemError& err) {
        throw std::runtime_error("ERR 10");
    }
}

void Vlkn::createDescriptorSetLayout() {
    static constexpr vk::DescriptorSetLayoutBinding ubo_binding {
        0,
        vk::DescriptorType::eUniformBuffer,
        1,
        vk::ShaderStageFlagBits::eVertex,
        nullptr
    };

    vk::DescriptorSetLayoutBinding texture_binding {
        0,
        vk::DescriptorType::eCombinedImageSampler,
        1,
        vk::ShaderStageFlagBits::eFragment,
        nullptr
    };

    vk::DescriptorSetLayoutCreateInfo layout_info = {
        {},
        1,
        &ubo_binding
    };

    if (
        device_.createDescriptorSetLayout(
            &layout_info,
            nullptr,
            &ubo_layout_
        ) != vk::Result::eSuccess
    ) throw std::runtime_error("ERR 14");

    layout_info.pBindings = &texture_binding;

    if (
        device_.createDescriptorSetLayout(
            &layout_info,
            nullptr,
            &texture_layout_
        ) != vk::Result::eSuccess
    ) throw std::runtime_error("ERR 14");
}

void Vlkn::createUniformBuffers() {
    static constexpr vk::DeviceSize buffer_size = sizeof(UniformBufferObject);

    uniform_buffers_.resize(max_f_frames_);

    for (size_t i = 0; i < max_f_frames_; ++i) {
        uniform_buffers_[i] = new MemoryBuffer (
            buffer_size,
            vk::BufferUsageFlagBits::eUniformBuffer,
            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
            device_,
            graphics_queue_,
            command_pool_,
             physical_device_
        );
    }
}

void Vlkn::createDescriptorPool() {
    const vk::DescriptorPoolSize pool_sizes[] = {
        {
            vk::DescriptorType::eUniformBuffer,
            static_cast<uint32_t>(max_f_frames_)
        },
        {
            vk::DescriptorType::eCombinedImageSampler,
            1
        }
    };

    const vk::DescriptorPoolCreateInfo pool_info = {
        {vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet},
        static_cast<uint32_t>(max_f_frames_ + objects_.size()),
        static_cast<uint32_t>(std::size(pool_sizes)),
        pool_sizes
    };

    if (
        device_.createDescriptorPool(
            &pool_info,
            nullptr,
            &descriptor_pool_
        ) != vk::Result::eSuccess
    ) throw std::runtime_error("ERR 13");
}

void Vlkn::createDescriptorSets() {
    std::vector<vk::DescriptorSetLayout> layouts (max_f_frames_, ubo_layout_);

    const vk::DescriptorSetAllocateInfo alloc_info = {
        descriptor_pool_,
        static_cast<uint32_t>(max_f_frames_),
        layouts.data()
    };

    descriptor_sets_.resize(max_f_frames_);

    if (
        device_.allocateDescriptorSets(
            &alloc_info,
            descriptor_sets_.data()
        ) != vk::Result::eSuccess
    ) throw std::runtime_error("ERR 14");

    for (size_t i = 0; i < max_f_frames_; i++) {
        vk::DescriptorBufferInfo buffer_info = {
            uniform_buffers_[i]->getBuffer(),
            0,
            sizeof(UniformBufferObject)
        };

        vk::WriteDescriptorSet descriptor_write = {
            descriptor_sets_[i],
            0,
            0,
            1,
            vk::DescriptorType::eUniformBuffer,
            nullptr,
            &buffer_info,
            nullptr
        };

        device_.updateDescriptorSets(1, &descriptor_write, 0, nullptr);
    }
}

void Vlkn::generateUBO(UniformBufferObject ubo) const {

    const float aspect = extent_.width / static_cast<float>(extent_.height);
    static constexpr float fovy = glm::radians(75.0f / 2);
    static constexpr float n = 0.01f;
    static constexpr float f = 10000.0f;

    ubo.proj = glm::perspective(fovy, aspect, n, f);

    uniform_buffers_[current_frame_]->set(&ubo, sizeof(ubo));
}

[[nodiscard]] vk::UniqueShaderModule Vlkn::createShaderModule(
    const std::vector<char> &code
) const {
    try {
        return device_.createShaderModuleUnique({
            {},
            code.size(),
            reinterpret_cast<const uint32_t*>(code.data())
        });
    } catch (const vk::SystemError& err) {
        throw std::runtime_error("ERR 25");
    }
}

[[nodiscard]] vk::Extent2D Vlkn::chooseExtent(
    const vk::SurfaceCapabilitiesKHR& capabilities
) const  {
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        return capabilities.currentExtent;
    }

    int width, height;
    glfwGetFramebufferSize(info_.window_, &width, &height);

    vk::Extent2D extent = { static_cast<uint32_t>(width), static_cast<uint32_t>(height) };

    extent.width = std::max(
        capabilities.minImageExtent.width,
        std::min(capabilities.maxImageExtent.width, extent.width)
    );
    extent.height = std::max(
        capabilities.minImageExtent.height,
        std::min(capabilities.maxImageExtent.height, extent.height)
    );

    return extent;
}

[[nodiscard]] SwapChainSupportDetails Vlkn::querySwapChainSupport(
    const vk::PhysicalDevice& device
) const  {
    return {
        device.getSurfaceCapabilitiesKHR(surface_),
        device.getSurfaceFormatsKHR(surface_),
        device.getSurfacePresentModesKHR(surface_)
    };
}

[[nodiscard]] bool Vlkn::isDeviceSuitable(const vk::PhysicalDevice& device) const {
    const auto [graphics, present] = findQueueFamilies(device);
    const bool supported_extensions = VulkanUtils::checkDeviceExtensionSupport(device);
    bool swapChainAdequate = false;

    if (supported_extensions) {
        const SwapChainSupportDetails sc_support = querySwapChainSupport(device);
        swapChainAdequate = !sc_support.formats.empty() && !sc_support.presentModes.empty();
    }

    return graphics.has_value() && present.has_value() && supported_extensions && swapChainAdequate;
}

[[nodiscard]] GraphicsPresentInfo Vlkn::findQueueFamilies(
    const vk::PhysicalDevice& device
) const {
    GraphicsPresentInfo indices;

    std::vector<vk::QueueFamilyProperties> families = device.getQueueFamilyProperties();

    int i = 0;
    for (const auto& queueFamily : families) {
        if (
            queueFamily.queueCount > 0 &&
            queueFamily.queueFlags & vk::QueueFlagBits::eGraphics
        ) indices.graphics_family_ = i;

        if (
            queueFamily.queueCount > 0 &&
            device.getSurfaceSupportKHR(i, surface_)
        ) indices.present_family_ = i;

        if (
            indices.graphics_family_.has_value() &&
            indices.present_family_.has_value()
        ) break;

        ++i;
    }

    return indices;
}