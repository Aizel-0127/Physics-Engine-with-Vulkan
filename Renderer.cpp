#pragma once
#include "Renderer.h"
#include "VulkanContext.h"
#include "SwapchainManager.h"
#include "PipelineManager.h"
#include "ResourceManager.h"
#include "Scene.h"

void Renderer::createPresentCommandResources(QueueFamilyIndices queueFamilyIndices, SwapchainManager& swapchain)
{
    if (queueFamilyIndices.graphicsFamily.value() == queueFamilyIndices.presentFamily.value()) {
        return; // Если семейства совпадают, ресурсы не нужны
    }

    // 1. Создаём пул для presentFamily
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = queueFamilyIndices.presentFamily.value();

    if (vkCreateCommandPool(device, &poolInfo, nullptr, &presentCommandPool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create present command pool!");
    }

    // 2. Выделяем по одному командному буферу на каждое изображение swapchain
    presentCommandBuffers.resize(swapchain.swapChainImages.size());

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = presentCommandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = static_cast<uint32_t>(presentCommandBuffers.size());

    if (vkAllocateCommandBuffers(device, &allocInfo, presentCommandBuffers.data()) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate present command buffers!");
    }

    // 3. Создаём семафоры для передачи владения
    imageOwnershipSemaphores.resize(swapchain.swapChainImages.size());

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    for (size_t i = 0; i < swapchain.swapChainImages.size(); i++) {
        if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageOwnershipSemaphores[i]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create ownership semaphores!");
        }
    }

    // 4. Записываем Release-барьеры в каждый командный буфер
    for (size_t i = 0; i < swapchain.swapChainImages.size(); i++) {
        recordPresentOwnershipRelease(queueFamilyIndices, swapchain, static_cast<uint32_t>(i));
    }
}

void Renderer::createSyncObjects()
{
    // Описание семафора.
    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semaphoreInfo.pNext = nullptr;
    semaphoreInfo.flags = 0;

    // Описание fence'а.
    // ВАЖНО: флаг SIGNALED обязателен, иначе первый кадр будет ждать вечно.
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.pNext = nullptr;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(device, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create synchronization objects for a frame!");
        }
    }
}

void Renderer::createCommandBuffers()
{
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.pNext = nullptr;
    allocInfo.commandPool = commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = MAX_FRAMES_IN_FLIGHT;

    if (vkAllocateCommandBuffers(device, &allocInfo, graphicsCommandBuffers.data()) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate command buffers!");
    }
}

void Renderer::createCommandPool(QueueFamilyIndices queueFamilyIndices)
{
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.pNext = nullptr;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();

    if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create command pool!");
    }
}

void Renderer::recordPresentOwnershipRelease(QueueFamilyIndices queueFamilyIndices, SwapchainManager& swapchain, uint32_t imageIndex)
{
    VkCommandBuffer cmd = presentCommandBuffers[imageIndex];

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

    vkBeginCommandBuffer(cmd, &beginInfo);

    VkImageMemoryBarrier releaseBarrier{};
    releaseBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    releaseBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    releaseBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    releaseBarrier.srcQueueFamilyIndex = queueFamilyIndices.graphicsFamily.value();
    releaseBarrier.dstQueueFamilyIndex = queueFamilyIndices.presentFamily.value();
    releaseBarrier.image = swapchain.swapChainImages[imageIndex];
    releaseBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    releaseBarrier.srcAccessMask = 0;
    releaseBarrier.dstAccessMask = 0;

    vkCmdPipelineBarrier(
        cmd,
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
        0, 0, nullptr, 0, nullptr, 1, &releaseBarrier
    );

    vkEndCommandBuffer(cmd);
}

void Renderer::drawFrame(
    VulkanContext& context,
    SwapchainManager& swapchain,
    PipelineManager& pipelines,
    ResourceManager& resources,
    Scene& scene,
    TracyVkCtx tracyVkCtx
)
{
    ZoneScoped;

    if (swapchain.framebufferResized) {
        if (swapchain.currentWidth > 0 && swapchain.currentHeight > 0) {
            swapchain.framebufferResized = false;
            swapchain.recreateSwapChain(context,*this);
            return;
        }
    }

    glm::mat4 projection;
    // Ортографическая проекция: от (0,0) до (WIDTH, HEIGHT), ось Y вниз
    projection = glm::ortho(0.0f, static_cast<float>(WIDTH),
        static_cast<float>(HEIGHT), 0.0f,
        -1.0f, 1.0f);

    // 1. Ждём, пока GPU закончит с командным буфером текущего кадра
    vkWaitForFences(context.device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

    // 2. Получаем изображение из swapchain
    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(
        context.device,
        swapchain.swapChain,
        UINT64_MAX,
        imageAvailableSemaphores[currentFrame],
        VK_NULL_HANDLE,
        &imageIndex
    );

    // Обработка VK_ERROR_OUT_OF_DATE_KHR — swapchain устарел
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        swapchain.recreateSwapChain(context, *this);
        return;
    }
    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("failed to acquire swap chain image!");
    }
    // 3. Обновляем UBO данными кругов
    resources.updateCircleUniformBuffer(scene,currentFrame);

    // 4. Сбрасываем fence — теперь он unsignaled
    //    ДОЛЖНО быть после acquire, потому что acquire может блокироваться,
    //    а fence должен остаться сигнализированным до этого момента.
    vkResetFences(context.device, 1, &inFlightFences[currentFrame]);

    // 5. Сбрасываем командный буфер и начинаем запись
    vkResetCommandBuffer(graphicsCommandBuffers[currentFrame], 0);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(graphicsCommandBuffers[currentFrame], &beginInfo);

    VkImageMemoryBarrier acquireBarrier{};
    acquireBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    acquireBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    acquireBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    acquireBarrier.image = swapchain.swapChainImages[imageIndex];
    acquireBarrier.srcAccessMask = 0;
    acquireBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    acquireBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

    vkCmdPipelineBarrier(
        graphicsCommandBuffers[currentFrame],
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        0, 0, nullptr, 0, nullptr, 1, &acquireBarrier
    );

    // 6.1. Acquire-барьер для MSAA colorImage (только если MSAA включен)
    if (context.MSAAEnabled()) {
        VkImageMemoryBarrier msaaBarrier{};
        msaaBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        msaaBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED; // Или текущий layout
        msaaBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        msaaBarrier.image = swapchain.colorImage;
        msaaBarrier.srcAccessMask = 0; // Или VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, если нужно
        msaaBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        msaaBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

        vkCmdPipelineBarrier(
            graphicsCommandBuffers[currentFrame],
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            0, 0, nullptr, 0, nullptr, 1, &msaaBarrier
        );
    }

    // 7. Начинаем dynamic rendering
    VkRenderingAttachmentInfoKHR colorAttachment{};
    colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
    colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;

    colorAttachment.clearValue = { {0.1f, 0.1f, 0.15f, 1.0f} };

    if (context.MSAAEnabled()) {
        colorAttachment.imageView = swapchain.colorImageView;
        colorAttachment.resolveImageView = swapchain.swapChainImageViews[imageIndex];
        colorAttachment.resolveImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colorAttachment.resolveMode = VK_RESOLVE_MODE_AVERAGE_BIT;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    }
    else {
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.imageView = swapchain.swapChainImageViews[imageIndex];
    }

    VkRenderingInfoKHR renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR;
    renderingInfo.renderArea = { {0, 0}, swapchain.swapChainExtent };
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments = &colorAttachment;

    if (!scene.quads.empty()) {
        // 1. Обновляем данные на CPU
        scene.UpdateQuadVertices();
        // 2. Проверяем, хватает ли памяти в буферах GPU. 
        // Если квадов стало больше, функция пересоздаст буферы.
        resources.ensureQuadBufferCapacity(context.physicalDevice, scene.quads.size());
        VkDeviceSize dataSize = sizeof(Vertex) * scene.quadVertices.size();

        // 2. Копируем данные CPU -> Staging Buffer (RAM)
        void* data;
        vkMapMemory(context.device, resources.quadStagingBufferMemory[currentFrame], 0, dataSize, 0, &data);
        memcpy(data, scene.quadVertices.data(), dataSize);
        vkUnmapMemory(context.device, resources.quadStagingBufferMemory[currentFrame]);

        // 3. Копируем данные Staging Buffer -> Vertex Buffer (VRAM)
        VkBufferCopy copyRegion{};
        copyRegion.srcOffset = 0;
        copyRegion.dstOffset = 0;
        copyRegion.size = dataSize;
        vkCmdCopyBuffer(graphicsCommandBuffers[currentFrame], resources.quadStagingBuffers[currentFrame], resources.quadVertexBuffers[currentFrame], 1, &copyRegion);

        // 4. БАРЬЕР (Критически важно!)
        // Говорим GPU: "Не начинай читать вершины, пока не закончишь их копирование"
        VkBufferMemoryBarrier bufferBarrier{};
        bufferBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        bufferBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        bufferBarrier.dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
        bufferBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        bufferBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        bufferBarrier.buffer = resources.quadVertexBuffers[currentFrame];
        bufferBarrier.offset = 0;
        bufferBarrier.size = VK_WHOLE_SIZE;

        vkCmdPipelineBarrier(
            graphicsCommandBuffers[currentFrame],
            VK_PIPELINE_STAGE_TRANSFER_BIT,           // Ждем завершения копирования
            VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,       // Перед тем как начнем читать вершины
            0, 0, nullptr, 1, &bufferBarrier, 0, nullptr
        );
    }

    // Начинаем GPU-зону для всего прохода рендеринга
    TracyVkZone(tracyVkCtx, graphicsCommandBuffers[currentFrame], "Main Render Pass");
    vkCmdBeginRendering(graphicsCommandBuffers[currentFrame], &renderingInfo);

    // 8. Устанавливаем динамические состояния
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(swapchain.swapChainExtent.width);
    viewport.height = static_cast<float>(swapchain.swapChainExtent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(graphicsCommandBuffers[currentFrame], 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = { 0, 0 };
    scissor.extent = swapchain.swapChainExtent;
    vkCmdSetScissor(graphicsCommandBuffers[currentFrame], 0, 1, &scissor);
    vkCmdPushConstants(graphicsCommandBuffers[currentFrame],
        pipelines.pipelineLayout,
        VK_SHADER_STAGE_VERTEX_BIT,
        0, sizeof(glm::mat4), &projection);
    // 9. Привязываем pipeline и descriptor set
    if (!scene.quads.empty()) {
        vkCmdBindPipeline(graphicsCommandBuffers[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.quadPipeline);
        VkBuffer vertexBuffers[] = { resources.quadVertexBuffers[currentFrame] }; // Берем VRAM буфер
        VkDeviceSize offsets[] = { 0 };
        vkCmdBindVertexBuffers(graphicsCommandBuffers[currentFrame], 0, 1, vertexBuffers, offsets);
        vkCmdDraw(graphicsCommandBuffers[currentFrame], static_cast<uint32_t>(scene.quadVertices.size()), 1, 0, 0);
    }


    vkCmdBindPipeline(graphicsCommandBuffers[currentFrame],
        VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.circlePipeline);
    vkCmdBindDescriptorSets(graphicsCommandBuffers[currentFrame],
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        pipelines.pipelineLayout, 0, 1,
        &resources.descriptorSets[currentFrame],
        0, nullptr);
    // 11. Draw call: 4 вершины квада, по одному инстансу на круг
    vkCmdDraw(graphicsCommandBuffers[currentFrame],
        4,
        static_cast<uint32_t>(scene.circles.size()),
        0, 0);

    // 12. Завершаем rendering
    vkCmdEndRendering(graphicsCommandBuffers[currentFrame]);

    // 13. Release-барьер: передача владения graphics → present
    VkImageMemoryBarrier releaseBarrier{};
    releaseBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    releaseBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    releaseBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    releaseBarrier.image = swapchain.swapChainImages[imageIndex];
    releaseBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    releaseBarrier.dstAccessMask = 0;
    releaseBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

    if (context.queueFamilyIndices.graphicsFamily.value() != context.queueFamilyIndices.presentFamily.value()) {
        releaseBarrier.srcQueueFamilyIndex = context.queueFamilyIndices.graphicsFamily.value();
        releaseBarrier.dstQueueFamilyIndex = context.queueFamilyIndices.presentFamily.value();
    }
    else {
        releaseBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        releaseBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    }

    vkCmdPipelineBarrier(
        graphicsCommandBuffers[currentFrame],
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
        0, 0, nullptr, 0, nullptr, 1, &releaseBarrier
    );
    // Собираем данные с GPU перед отправкой команд
    TracyVkCollect(tracyVkCtx, graphicsCommandBuffers[currentFrame]);
    // 14. Завершаем запись командного буфера
    vkEndCommandBuffer(graphicsCommandBuffers[currentFrame]);

    // 15. Submit в graphicsQueue
    {
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        VkSemaphore waitSemaphores[] = { imageAvailableSemaphores[currentFrame] };
        VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;

        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &graphicsCommandBuffers[currentFrame];

        VkSemaphore signalSemaphores[] = { swapchain.renderFinishedSemaphores[imageIndex] };
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;

        if (vkQueueSubmit(context.graphicsQueue, 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS) {
            throw std::runtime_error("failed to submit draw command buffer!");
        }
    }

    if (context.queueFamilyIndices.graphicsFamily.value() != context.queueFamilyIndices.presentFamily.value())
    {
        VkSubmitInfo presentSubmit{};
        presentSubmit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        // Ждём завершения графического submit'а
        VkSemaphore waitSemaphores[] = { swapchain.renderFinishedSemaphores[imageIndex] };
        VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_ALL_COMMANDS_BIT };
        presentSubmit.waitSemaphoreCount = 1;
        presentSubmit.pWaitSemaphores = waitSemaphores;
        presentSubmit.pWaitDstStageMask = waitStages;

        // Командный буфер с aquire-барьером (present → graphics)
        presentSubmit.commandBufferCount = 1;
        presentSubmit.pCommandBuffers = &presentCommandBuffers[imageIndex];

        // Сигналим семафор, который будет ждать vkQueuePresentKHR
        VkSemaphore signalSemaphores[] = { imageOwnershipSemaphores[imageIndex] };
        presentSubmit.signalSemaphoreCount = 1;
        presentSubmit.pSignalSemaphores = signalSemaphores;

        if (vkQueueSubmit(context.presentQueue, 1, &presentSubmit, VK_NULL_HANDLE) != VK_SUCCESS) {
            throw std::runtime_error("failed to submit present command buffer!");
        }
    }

    // 16. Презентация
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = (context.queueFamilyIndices.graphicsFamily.value() != context.queueFamilyIndices.presentFamily.value()) ?
        &imageOwnershipSemaphores[imageIndex] : &swapchain.renderFinishedSemaphores[imageIndex];

    VkSwapchainKHR swapChains[] = { swapchain.swapChain };
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = &imageIndex;

    result = vkQueuePresentKHR(context.presentQueue, &presentInfo);

    // 17. Обработка устаревшего swapchain
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || swapchain.framebufferResized) {
        if (swapchain.currentWidth > 0 && swapchain.currentHeight > 0) {
            swapchain.framebufferResized = false;
            swapchain.recreateSwapChain(context, *this);
        }
    }
    else if (result != VK_SUCCESS) {
        throw std::runtime_error("failed to present swap chain image!");
    }

    // 18. Переходим к следующему кадру в полёте
    currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}