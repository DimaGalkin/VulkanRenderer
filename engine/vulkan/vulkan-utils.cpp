#include "vulkan-utils.hpp"

#include <set>

std::vector<const char*> tdl::Vlkn::getRequiredExtensions() {
    uint32_t glfwExtensionCount = 0;
    const char** glfw_extensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    return {
        glfw_extensions,
        glfw_extensions + glfwExtensionCount
    };
}

std::vector<char> tdl::Vlkn::readFile(
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

bool tdl::Vlkn::checkDeviceExtensionSupport(
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

vk::SurfaceFormatKHR tdl::Vlkn::chooseFormat(
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

vk::PresentModeKHR tdl::Vlkn::chooseMode(
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

void tdl::Vlkn::init() {
    createInstance();
    createSurface();
    pickPhysicalDevice();
    createLogicalDevice();
    createSwapchain();
    createImageViews();
    createRenderPass();
    createDescriptorSetLayout();
    createGraphicsPipeline();
    createCommandPool();
    createZBuffer();
    createFramebuffers();
    createUniformBuffers();
    createDescriptorPool();
    createDescriptorSets();
    loadModels();
    startCommandBuffers();
    createSyncObjects();
};

void tdl::Vlkn::newFrame(
    const UniformBufferObject& ubo
) {
    if (
        device_.waitForFences(
            1, &fences_[current_frame_],
            VK_TRUE, std::numeric_limits<uint64_t>::max()
        ) != vk::Result::eSuccess
    ) throw std::runtime_error("ERR 30: Failed to wait for fences. Vlkn::newFrame(...)");

    uint32_t idx;
    try {
        const vk::ResultValue result = device_.acquireNextImageKHR(
            swapchain_,
            std::numeric_limits<uint64_t>::max(),
            image_available_[current_frame_], nullptr
        );
        idx = result.value;
    } catch (const vk::OutOfDateKHRError& _) {
        recreateSwapchain();
        return;
    } catch (const vk::SystemError& err) {
        throw std::runtime_error(
            "ERR 031: aquireImageKHR failed. Vlkn::newFrame(...)\n"
            + std::string(err.what())
        );
    }

    regenUBOs(ubo);

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
    } catch (const vk::OutOfDateKHRError& _) {
        r_present = vk::Result::eErrorOutOfDateKHR;
    } catch (const vk::SystemError& err) {
        throw std::runtime_error(
            "ERR 032: Failed to get presentKHR. Vlkn::newImage(...)\n"
            + std::string(err.what())
        );
    }

    if (r_present == vk::Result::eSuboptimalKHR || resized_) {
        resized_ = false;
        recreateSwapchain();
        return;
    }

    current_frame_ = (current_frame_ + 1) % max_f_frames_;
}

void tdl::Vlkn::submitForDraw(
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
        ) throw std::runtime_error("ERR 033: Failed to reset fences. Vlkn::submitForDraw(...)");

    try {
        graphics_queue_.submit(submit_info, fences_[current_frame_]);
    } catch (const vk::SystemError& err) {
        throw std::runtime_error(
            "ERR 034: Failed to submit command buffer for rendering. Vlkn::submitForDraw(...)\n"
            + std::string(err.what())
        );
    }
}

void tdl::Vlkn::cleanupSwapchain() {
    for (const auto& framebuffer : framebuffers_) device_.destroyFramebuffer(framebuffer);
    for (const auto& image_view : image_views_) device_.destroyImageView(image_view);

    device_.destroySwapchainKHR(swapchain_);

    // for (const auto& group : command_buffers_)
    //     device_.freeCommandBuffers(command_pool_, group);

    device_.destroyPipeline(graphics_pipeline_);
    device_.destroyPipelineLayout(pipeline_layout_);
    device_.destroyRenderPass(render_pass_);

    device_.destroyImage(z_buffer_);
    device_.freeMemory(z_buffer_memory_);
    device_.destroyImageView(z_buffer_view_);
}

void tdl::Vlkn::cleanup() {
    cleanupSwapchain();

    device_.destroyCommandPool(command_pool_);
    device_.destroyDescriptorSetLayout(ubo_layout_);
    device_.destroyDescriptorSetLayout(object_layout_);
    device_.destroyDescriptorSetLayout(model_layout_);
    device_.destroyDescriptorPool(descriptor_pool_);

    for (const auto& group : command_buffers_)
        device_.freeCommandBuffers(command_pool_, group);

        device_.destroyPipeline(graphics_pipeline_);
        device_.destroyPipelineLayout(pipeline_layout_);
        device_.destroyRenderPass(render_pass_);

    for (size_t i = 0; i < max_f_frames_; ++i) {
        device_.destroyFence(fences_[i]);
        device_.destroySemaphore(render_finished_[i]);
        device_.destroySemaphore(image_available_[i]);

        delete uniform_buffers_[i];
    }

    instance_->destroySurfaceKHR(surface_);

    glfwDestroyWindow(info_->window_);
    glfwTerminate();
}

void tdl::Vlkn::recreateSwapchain() {
    int width = 0; int height = 0;
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(info_->window_, &width, &height);
        glfwWaitEvents();
    }

    device_.waitIdle();

    cleanupSwapchain();

    createSwapchain();
    createImageViews();
    createZBuffer();
    createRenderPass();
    createGraphicsPipeline();
    createFramebuffers();
    startCommandBuffers();
}

void tdl::Vlkn::createInstance() {
    const vk::ApplicationInfo application_info = {
        info_->title_.c_str(),
        VK_MAKE_VERSION(0, 0, 0),
        "ThreeDL Engine",
        VK_MAKE_VERSION(0, 0, 0),
        VK_API_VERSION_1_0
    };

    const std::vector<const char*> extensions = getRequiredExtensions();
    const vk::InstanceCreateInfo instance_info = {
        {},
        &application_info,
        0, nullptr,
        static_cast<uint32_t>(extensions.size()), extensions.data()
    };

    try {
        instance_ = vk::createInstanceUnique(instance_info, nullptr);
    } catch (const vk::SystemError& err) {
        throw std::runtime_error(
            "ERR 035: Failed to create vk::Instance. Vlkn::createInstance(...)\n"
            + std::string(err.what())
        );
    }
}

void tdl::Vlkn::createSurface() {
    VkSurfaceKHR surface;
    if (glfwCreateWindowSurface(instance_.get(), info_->window_, nullptr, &surface) != VK_SUCCESS) {
        throw std::runtime_error("ERR 036: Failed to create window surface. Vlkn::createSurface(...)");
    }

    surface_ = surface;
}

void tdl::Vlkn::pickPhysicalDevice() {
    const std::vector<vk::PhysicalDevice> devices = instance_->enumeratePhysicalDevices();
    if (devices.empty()) throw std::runtime_error("ERR 037: No devices found! Vlkn::pickPhysicalDevice(...)");

    physical_device_ = devices[1];

    // for (const auto& device : devices) {
    //     if (isDeviceSuitable(device)) { physical_device_ = device; break; }
    // }
    // if (!physical_device_) throw std::runtime_error("ERR 038: No suitable devices found! Vlkn::pickPhysicalDevice(...)");
}

void tdl::Vlkn::createLogicalDevice() {
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
    } catch (const vk::SystemError& err) {
        throw std::runtime_error(
            "ERR 039: Failed to create Logical device. Vlkn::createLogicalDevice(...)\n"
            + std::string(err.what())
        );
    }

    graphics_queue_ = device_.getQueue(graphics.value(), 0);
    present_queue_ = device_.getQueue(present.value(), 0);
}

void tdl::Vlkn::createSwapchain() {
    const SwapChainSupportDetails sc_support = {
        physical_device_.getSurfaceCapabilitiesKHR(surface_),
        physical_device_.getSurfaceFormatsKHR(surface_),
        physical_device_.getSurfacePresentModesKHR(surface_)
    };
    const vk::SurfaceFormatKHR surfaceFormat = chooseFormat(sc_support.formats);
    const vk::PresentModeKHR presentMode = chooseMode(sc_support.present_modes);
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
    info.oldSwapchain = vk::SwapchainKHR { nullptr };

    try {
        swapchain_ = device_.createSwapchainKHR(info);
    } catch (const vk::SystemError& err) {
        throw std::runtime_error(
            "ERR 040: Failed to create swapchain. Vlkn::createSwapchain(...)\n"
            + std::string(err.what())
        );
    }

    images_ = device_.getSwapchainImagesKHR(swapchain_);
    extent_ = extent;
    format_ = surfaceFormat.format;
}

void tdl::Vlkn::createImageViews() {
    image_views_.resize(images_.size());

    for (size_t i = 0; i < images_.size(); ++i) {
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
            throw std::runtime_error(
                "ERR 041: Failed to create image views. Vlkn::createImageViews(...)\n"
                + std::string(err.what())
            );
        }
    }
}

void tdl::Vlkn::createRenderPass() {
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

    static constexpr vk::AttachmentDescription depth {
        {},
        vk::Format::eD32Sfloat,
        vk::SampleCountFlagBits::e1,
        vk::AttachmentLoadOp::eClear,
        vk::AttachmentStoreOp::eDontCare,
        vk::AttachmentLoadOp::eDontCare,
        vk::AttachmentStoreOp::eDontCare,
        vk::ImageLayout::eUndefined,
        vk::ImageLayout::eDepthStencilAttachmentOptimal
    };

    static constexpr vk::AttachmentReference depth_ref {
        1,
        vk::ImageLayout::eDepthStencilAttachmentOptimal
    };
    static constexpr vk::AttachmentReference color_ref {
        0,
        vk::ImageLayout::eColorAttachmentOptimal
    };

    static constexpr vk::SubpassDescription subpass {
        {},
        vk::PipelineBindPoint::eGraphics,
        0, nullptr,
        1, &color_ref,
        nullptr, &depth_ref
    };

    const std::array<vk::AttachmentDescription, 2> attachments = { color, depth };

    static constexpr vk::SubpassDependency dependency {
        VK_SUBPASS_EXTERNAL,
        0,
        vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eEarlyFragmentTests,
        vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eEarlyFragmentTests,
        {},
        vk::AccessFlagBits::eColorAttachmentWrite | vk::AccessFlagBits::eDepthStencilAttachmentWrite
    };

    try {
        render_pass_ = device_.createRenderPass({
            {},
            attachments.size(),
            attachments.data(),
            1,
            &subpass,
            1,
            &dependency
        });
    } catch (const vk::SystemError& err) {
        throw std::runtime_error(
            "ERR 042: Failed to create render pass. Vlkn::createRenderPass(...)\n"
            + std::string(err.what())
        );
    }
}

void tdl::Vlkn::createFramebuffers() {
    framebuffers_.resize(image_views_.size());

    for (size_t i = 0; i < image_views_.size(); ++i) {
        const std::array<vk::ImageView, 2> attachments = { image_views_[i], z_buffer_view_ };

        vk::FramebufferCreateInfo framebufferInfo = {
            {},
            render_pass_,
            attachments.size(),
            attachments.data(),
            extent_.width,
            extent_.height,
            1
        };

        try {
            framebuffers_[i] = device_.createFramebuffer(framebufferInfo);
        } catch (const vk::SystemError& err) {
            throw std::runtime_error(
                "ERR 043: Failed to create framebuffer. Vlkn::createFramebuffers(...)\n"
                + std::string(err.what())
            );
        }
    }
}

void tdl::Vlkn::createCommandPool() {
    const auto [graphics, _] = findQueueFamilies(physical_device_);
    const vk::CommandPoolCreateInfo pool_info = {
        {},
        graphics.value()
    };

    try {
        command_pool_ = device_.createCommandPool(pool_info);
    } catch (const vk::SystemError& err) {
        throw std::runtime_error(
            "ERR 044: Failed to create command pool. Vlkn::createCommandPool(...)\n"
            + std::string(err.what())
        );
    }
}

void tdl::Vlkn::loadModels() {
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

        object->initUBOs(
            max_f_frames_,
            device_,
            graphics_queue_,
            command_pool_,
            physical_device_
        );

        object->createDescriptorSets(max_f_frames_, descriptor_pool_, device_, model_layout_, object_layout_);
    }
}

void tdl::Vlkn::startCommandBuffers() {
    command_buffers_.resize(framebuffers_.size());

    const vk::CommandBufferAllocateInfo alloc_info = {
        command_pool_,
        vk::CommandBufferLevel::ePrimary,
        static_cast<uint32_t>(command_buffers_.size())
    };

    try {
        command_buffers_ = device_.allocateCommandBuffers(alloc_info);
    } catch (const vk::SystemError& err) {
        throw std::runtime_error(
            "ERR 045: Failed to create command buffers. Vlkn::createCommandBuffers(...)\n"
            + std::string(err.what())
        );
    }

    for (size_t i = 0; i < command_buffers_.size(); ++i) {
        static constexpr vk::CommandBufferBeginInfo begin_info {
            vk::CommandBufferUsageFlagBits::eSimultaneousUse
        };

        try {
            command_buffers_[i].begin(begin_info);
        } catch (const vk::SystemError& err) {
            throw std::runtime_error(
                "ERR 046: Failed to start command buffer. Vlkn::startCommandBuffers(...)\n"
                + std::string(err.what())
            );
        }

        std::array<vk::ClearValue, 2> clear_values {
            vk::ClearColorValue(std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.0f}),
            vk::ClearDepthStencilValue(1.0f, 0)
        };

        const vk::RenderPassBeginInfo pass_info = {
            render_pass_,
            framebuffers_[i],
            {
                {0, 0},
                extent_
            },
            clear_values.size(),
            clear_values.data()
        };

        command_buffers_[i].beginRenderPass(pass_info, vk::SubpassContents::eInline);
        command_buffers_[i].bindPipeline(vk::PipelineBindPoint::eGraphics, graphics_pipeline_);
        command_buffers_[i].bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline_layout_, 0, 1, &descriptor_sets_[current_frame_], 0, nullptr);

        for (const auto& object : objects_) {
            object->render(command_buffers_[i], pipeline_layout_, current_frame_);
        }

        command_buffers_[i].endRenderPass();

        try {
            command_buffers_[i].end();
        } catch (const vk::SystemError& err) {
            throw std::runtime_error(
            "ERR 047: Failed to end command buffer. Vlkn::startCommandBuffers(...)\n"
            + std::string(err.what())
        );
        }
    }
}

void tdl::Vlkn::createSyncObjects() {
    image_available_.resize(max_f_frames_);
    render_finished_.resize(max_f_frames_);
    fences_.resize(max_f_frames_);

    try {
        for (size_t i = 0; i < max_f_frames_; ++i) {
            fences_[i] = device_.createFence({vk::FenceCreateFlagBits::eSignaled});
            image_available_[i] = device_.createSemaphore({});
            render_finished_[i] = device_.createSemaphore({});
        }
    } catch (const vk::SystemError& err) {
        throw std::runtime_error(
            "ERR 048: Failed to create fences & semaphores. Vlkn::createSyncObjects(...)\n"
            + std::string(err.what())
        );
    }
}

void tdl::Vlkn::createGraphicsPipeline() {
    const vk::UniqueShaderModule vert = createShaderModule(readFile("../shaders/vert.spv"));
    const vk::UniqueShaderModule frag = createShaderModule(readFile("../shaders/frag.spv"));

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

    vk::PipelineViewportStateCreateInfo viewport_info {
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
        vk::ColorComponentFlags(
            vk::ColorComponentFlagBits::eR |
            vk::ColorComponentFlagBits::eG |
            vk::ColorComponentFlagBits::eB |
            vk::ColorComponentFlagBits::eA
        )
    };

    static constexpr vk::PipelineColorBlendStateCreateInfo blending {
        {},
        VK_FALSE,
        vk::LogicOp::eCopy,
        1,
        &blending_attach,
        {0.0f, 0.0f, 0.0f, 0.0f}
    };

    const vk::DescriptorSetLayout layouts[] = { ubo_layout_, texture_layout_, object_layout_, model_layout_ };

    vk::PipelineLayoutCreateInfo pipe_info { // NOLINT (not a constant expression)
        {},
        std::size(layouts),
        layouts
    };

    try {
        pipeline_layout_ = device_.createPipelineLayout(pipe_info);
    } catch (const vk::SystemError& err) {
        throw std::runtime_error(
            "ERR 049: Failed to create pipeline layout. Vlkn::createPipelineLayout(...)\n"
            + std::string(err.what())
        );
    }

    static constexpr vk::PipelineDepthStencilStateCreateInfo depth_stencil {
        {},
        VK_TRUE,
        VK_TRUE,
        vk::CompareOp::eLess,
        VK_FALSE,
        VK_FALSE,
        {},
        {},
        0.0f,
        1.0f
    };

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
        &depth_stencil,
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
        throw std::runtime_error(
            "ERR 050: Failed to create graphics pipeline. Vlkn::createGraphicsPipeline(...)\n"
            + std::string(err.what())
        );
    }
}

void tdl::Vlkn::createDescriptorSetLayout() {
    static constexpr vk::DescriptorSetLayoutBinding ubo_binding {
        0,
        vk::DescriptorType::eUniformBuffer,
        1,
        vk::ShaderStageFlagBits::eVertex,
        nullptr
    };

    static constexpr vk::DescriptorSetLayoutBinding object_binding {
        1,
        vk::DescriptorType::eUniformBuffer,
        1,
        vk::ShaderStageFlagBits::eVertex,
        nullptr
    };

    static constexpr vk::DescriptorSetLayoutBinding model_binding {
        2,
        vk::DescriptorType::eUniformBuffer,
        1,
        vk::ShaderStageFlagBits::eVertex,
        nullptr
    };

    static constexpr vk::DescriptorSetLayoutBinding texture_binding {
        0,
        vk::DescriptorType::eCombinedImageSampler,
        1,
        vk::ShaderStageFlagBits::eFragment,
        nullptr
    };

    vk::DescriptorSetLayoutCreateInfo layout_info {
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
    ) throw std::runtime_error (
        "ERR 057: failed to create descriptor layout for ubo_binding"
        "Vlkn::createDescriptorSetLayout(...)"
    );

    layout_info.pBindings = &object_binding;

    if (
        device_.createDescriptorSetLayout(
            &layout_info,
            nullptr,
            &object_layout_
        ) != vk::Result::eSuccess
    ) throw std::runtime_error (
        "ERR 058: failed to create descriptor layout for object_binding"
        "Vlkn::createDescriptorSetLayout(...)"
    );

    layout_info.pBindings = &model_binding;

    if (
        device_.createDescriptorSetLayout(
            &layout_info,
            nullptr,
            &model_layout_
        ) != vk::Result::eSuccess
    ) throw std::runtime_error (
        "ERR 059: failed to create descriptor layout for model_binding"
        "Vlkn::createDescriptorSetLayout(...)"
    );

    layout_info.pBindings = &texture_binding;

    if (
        device_.createDescriptorSetLayout(
            &layout_info,
            nullptr,
            &texture_layout_
        ) != vk::Result::eSuccess
    ) throw std::runtime_error (
        "ERR 060: failed to create descriptor layout for texture_binding"
        "Vlkn::createDescriptorSetLayout(...)"
    );
}

void tdl::Vlkn::createZBuffer() {
    static constexpr auto format = vk::Format::eD32Sfloat;

    const vk::ImageCreateInfo info {
        {},
        vk::ImageType::e2D,
        format,
        {extent_.width, extent_.height, 1},
        1,
        1,
        vk::SampleCountFlagBits::e1,
        vk::ImageTiling::eOptimal,
        vk::ImageUsageFlagBits::eDepthStencilAttachment,
        vk::SharingMode::eExclusive,
        0, nullptr,
        vk::ImageLayout::eUndefined
    };

    try {
        z_buffer_ = device_.createImage(info);
    } catch (const vk::SystemError& err) {
        throw std::runtime_error(
            "ERR 051: Failed to create zbuffer. Vlkn::createZBuffer(...)\n"
            + std::string(err.what())
        );
    }

    const vk::MemoryRequirements reqs = device_.getImageMemoryRequirements(z_buffer_);

    const vk::MemoryAllocateInfo alloc_info {
        reqs.size,
        MemoryBuffer::findMemoryType(
            physical_device_,
            reqs.memoryTypeBits,
            vk::MemoryPropertyFlagBits::eDeviceLocal
        )
    };

    try {
        z_buffer_memory_ = device_.allocateMemory(alloc_info);
    } catch (const vk::SystemError& err) {
        throw std::runtime_error(
            "ERR 052: Failed to allocate memory for zbuffer. Vlkn::createZBuffer(...)\n"
            + std::string(err.what())
        );
    }

    device_.bindImageMemory(z_buffer_, z_buffer_memory_, 0);

    const vk::ImageViewCreateInfo view_info {
        {},
        z_buffer_,
        vk::ImageViewType::e2D,
        format,
        {},
        {
            vk::ImageAspectFlagBits::eDepth,
            0,
            1,
            0,
            1
        }
    };

    try {
        z_buffer_view_ = device_.createImageView(view_info);
    } catch (const vk::SystemError& err) {
        throw std::runtime_error(
            "ERR 053: Failed to create zbuffer view. Vlkn::createZBuffer(...)\n"
            + std::string(err.what())
        );
    }
}


void tdl::Vlkn::createUniformBuffers() {
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

void tdl::Vlkn::createDescriptorPool() {
    const vk::DescriptorPoolSize pool_sizes[] {
        {
            vk::DescriptorType::eUniformBuffer,
            static_cast<uint32_t>(max_f_frames_)
        },
        {
            vk::DescriptorType::eCombinedImageSampler,
            1
        }
    };

    size_t descriptor_size = 0;

    for (const auto& model : objects_) {
        descriptor_size += max_f_frames_ + model->objects_.size() * (1 + max_f_frames_);
    }

    const vk::DescriptorPoolCreateInfo pool_info {
        {vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet},
        static_cast<uint32_t>(
            max_f_frames_ + descriptor_size
        ),
        static_cast<uint32_t>(std::size(pool_sizes)),
        pool_sizes
    };

    if (
        device_.createDescriptorPool(
            &pool_info,
            nullptr,
            &descriptor_pool_
        ) != vk::Result::eSuccess
        ) throw std::runtime_error("ERR 054: Failed to allocate descriptor pool. Vlkn::createDescriptorPool(...)");
}

void tdl::Vlkn::createDescriptorSets() {
    std::vector<vk::DescriptorSetLayout> layouts (max_f_frames_, ubo_layout_);

    const vk::DescriptorSetAllocateInfo alloc_info {
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
    ) throw std::runtime_error("ERR 055: Vlkn::createDescriptorSets(...)");

    for (size_t i = 0; i < max_f_frames_; ++i) {
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

void tdl::Vlkn::regenUBOs(const UniformBufferObject& ubo) const {
    uniform_buffers_[current_frame_]->set(&ubo, sizeof(ubo));

    for (const auto& model : objects_) {
        model->imageTick();

        for (const auto& [_, obj] : model->objects_) {
            if(obj->has_changed_) {
                obj->ubos_[current_frame_]->set(&obj->ubo_data_, sizeof(obj->ubo_data_));
                obj->has_changed_ = false;
            }
        }

        if(model->has_changed_) {
            model->ubos_[current_frame_]->set(&model->ubo_data_, sizeof(model->ubo_data_));
            model->has_changed_ = false;
        }
    }
}

[[nodiscard]] vk::UniqueShaderModule tdl::Vlkn::createShaderModule(
    const std::vector<char>& code
) const {
    try {
        return device_.createShaderModuleUnique({
            {},
            code.size(),
            reinterpret_cast<const uint32_t*>(code.data())
        });
    } catch (const vk::SystemError& err) {
        throw std::runtime_error(
            "ERR 056: Failed to create shader module. Vlkn::createShaderModule(...)\n"
            + std::string(err.what())
        );
    }
}

[[nodiscard]] vk::Extent2D tdl::Vlkn::chooseExtent(
    const vk::SurfaceCapabilitiesKHR& capabilities
) const {
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        return capabilities.currentExtent;
    }

    glfwGetFramebufferSize(info_->window_, &info_->width_, &info_->height_);

    vk::Extent2D extent = { static_cast<uint32_t>(info_->width_), static_cast<uint32_t>(info_->height_) };

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

[[nodiscard]] tdl::SwapChainSupportDetails tdl::Vlkn::querySwapChainSupport(
    const vk::PhysicalDevice& device
) const  {
    return {
        device.getSurfaceCapabilitiesKHR(surface_),
        device.getSurfaceFormatsKHR(surface_),
        device.getSurfacePresentModesKHR(surface_)
    };
}

[[nodiscard]] bool tdl::Vlkn::isDeviceSuitable(const vk::PhysicalDevice& device) const {
    const auto [graphics, present] = findQueueFamilies(device);
    const bool supported_extensions = checkDeviceExtensionSupport(device);
    bool swapChainAdequate = false;

    if (supported_extensions) {
        const auto [
            _,
            formats,
            present_modes
        ] = querySwapChainSupport(device);

        swapChainAdequate = !formats.empty() && !present_modes.empty();
    }

    return graphics.has_value() && present.has_value() && supported_extensions && swapChainAdequate;
}

[[nodiscard]] tdl::GraphicsPresentInfo tdl::Vlkn::findQueueFamilies(
    const vk::PhysicalDevice& device
) const {
    GraphicsPresentInfo indices;

    std::vector<vk::QueueFamilyProperties> families = device.getQueueFamilyProperties();

    int i = 0;
    for (const auto& queueFamily : families) {
        if (
            queueFamily.queueCount > 0 &&
            queueFamily.queueFlags & vk::QueueFlagBits::eGraphics
        ) indices.graphics_family = i;

        if (
            queueFamily.queueCount > 0 &&
            device.getSurfaceSupportKHR(i, surface_)
        ) indices.present_family = i;

        if (
            indices.graphics_family.has_value() &&
            indices.present_family.has_value()
        ) break;

        ++i;
    }

    return indices;
}