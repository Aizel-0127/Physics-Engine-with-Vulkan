#pragma once
#include "Primitives.h"
#include "Config.h"
//#include "MainWindow.h"

enum class BlendPreset {		//Для добавления нового пресета нужно добавить название в этой структуре
								//и параметры пресета в функции makeBlendAttachment
	Opaque,          // blendEnable = FALSE
	//AlphaBlend,      // SRC_ALPHA / ONE_MINUS_SRC_ALPHA (стандартная прозрачность)
	//Additive,        // SRC_ALPHA / ONE (свечение)
	//Multiply         // DST_COLOR / ZERO (умножение)
};

struct BasePipelineConfig
{
	std::vector<std::string> shaderPaths;
	const char* entryPoint = "main";
	VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
	VkVertexInputBindingDescription vertexBindingDesc{};
	std::array<VkVertexInputAttributeDescription, 2> vertexAttrDescs{};
	VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
	VkPipelineViewportStateCreateInfo viewportState{};
	VkPipelineRasterizationStateCreateInfo rasterizer{};
	VkPipelineMultisampleStateCreateInfo multisampling{};
	VkPipelineDepthStencilStateCreateInfo depthStencil{};
	BlendPreset blendPreset = BlendPreset::Opaque;
	VkColorComponentFlags colorWriteMask =
		VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
		VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;	//Значения по умолчанию для colorBlending
	VkPipelineColorBlendStateCreateInfo colorBlending{};
	std::vector<VkDynamicState> dynamicStates = {
	VK_DYNAMIC_STATE_VIEWPORT,
	VK_DYNAMIC_STATE_SCISSOR
	};
	VkPipelineDynamicStateCreateInfo dynamicState{};
	void SetRasterizer()
	{
		rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterizer.pNext = nullptr;
		rasterizer.flags = 0;
		// depthClampEnable: включает "зажим" глубины вместо отсечения фрагментов за пределами [0,1].
		// FALSE — стандартное поведение. TRUE полезно для shadow mapping, чтобы избежать артефактов.
		rasterizer.depthClampEnable = VK_FALSE;
		// rasterizerDiscardEnable: если TRUE — растеризация вообще не выполняется (геометрия отбрасывается).
		// Используется для проходов, где важны только side-эффекты вершинного шейдера (например, запись в SSBO).
		// Для обычного рендеринга — FALSE.
		rasterizer.rasterizerDiscardEnable = VK_FALSE;
		// polygonMode: как заливать треугольники. FILL — обычная заливка, LINE — каркас, POINT — вершины точками.
		// Для LINE и POINT требуется включить фичу fillModeNonSolid у устройства.
		// Для 2D — всегда FILL.
		rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
		// cullMode: отсечение граней. NONE — рисовать обе стороны, FRONT — отсекать лицевые, BACK — отсекать обратные.
		// Для 2D — NONE. Для 3D — обычно BACK.
		rasterizer.cullMode = VK_CULL_MODE_NONE;
		// frontFace: как определить лицевую грань по порядку обхода вершин. При cullMode = NONE это не важно
		// COUNTER_CLOCKWISE — против часовой стрелки, CLOCKWISE — по часовой.
		// если вершины идут CW, нужно поменять.
		rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
		// depthBiasEnable: включает смещение глубины для борьбы с z-fighting (обычно в shadow mapping).
		// Для 2D — FALSE.
		rasterizer.depthBiasEnable = VK_FALSE;
		// Параметры depth bias. Актуальны только при depthBiasEnable = TRUE. Для 2D — нули.
		rasterizer.depthBiasConstantFactor = 0.0f;
		rasterizer.depthBiasClamp = 0.0f;
		rasterizer.depthBiasSlopeFactor = 0.0f;
		// lineWidth: толщина линий при polygonMode = LINE. Требует фичи wideLines для значений > 1.0.
		rasterizer.lineWidth = 1.0f;
	}
	void SetMultisampling()
	{
		multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisampling.pNext = nullptr;
		multisampling.flags = 0;

		// rasterizationSamples: количество сэмплов на пиксель при растеризации.
		// ДОЛЖНО совпадать с samples у color attachment в render pass / dynamic rendering.
		// 1_BIT — MSAA выключен. 2/4/8/16/32/64_BIT — MSAA включён.
		// Значение должно быть поддержано устройством (см. framebufferColorSampleCounts).
		multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

		// sampleShadingEnable: включает per-sample shading.
		// FALSE — фрагментный шейдер выполняется один раз на пиксель, результат дублируется по сэмплам.
		// TRUE — шейдер выполняется для каждого сэмпла (дороже, но качественнее для краёв).
		// Требует фичи sampleRateShading у устройства.
		multisampling.sampleShadingEnable = VK_FALSE;

		// minSampleShading: минимальная доля сэмплов, затеняемых отдельно.
		// 0.0 — только один сэмпл затеняется (по умолчанию). 1.0 — все сэмплы.
		// Актуально только при sampleShadingEnable = TRUE.
		multisampling.minSampleShading = 0.0f;

		// pSampleMask: маска, определяющая, какие сэмплы участвуют в вычислении покрытия.
		// nullptr — используются все сэмплы (эквивалентно маске из одних единиц).
		// Ненулевой указатель — массив из ceil(rasterizationSamples / 32) элементов uint32_t.
		multisampling.pSampleMask = nullptr;

		// alphaToCoverageEnable: преобразует альфа-канал фрагмента в покрытие сэмплов.
		// TRUE — полезно для сглаживания листвы, травы, волос (alpha-tested геометрия).
		// FALSE — стандартное поведение.
		multisampling.alphaToCoverageEnable = VK_FALSE;

		// alphaToOneEnable: заменяет альфа-канал фрагмента на 1.0.
		// TRUE — используется редко (для специальных эффектов с прозрачностью).
		// FALSE — стандартное поведение.
		multisampling.alphaToOneEnable = VK_FALSE;
	}
	VkPipelineColorBlendAttachmentState makeBlendAttachment() const 
	{
		VkPipelineColorBlendAttachmentState attachment{};
		attachment.colorWriteMask = colorWriteMask;
		switch (blendPreset) {
		case BlendPreset::Opaque:
			attachment.blendEnable = VK_FALSE;
			break;
		}
		return attachment;
	}
	void SetColorBlending()
	{
		colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		colorBlending.pNext = nullptr;
		colorBlending.flags = 0;

		// logicOpEnable: включает логическую операцию вместо арифметического смешивания.
		// TRUE — доступно только если у устройства есть фича logicOp.
		// FALSE — стандартное поведение. Для 2D — всегда FALSE.
		colorBlending.logicOpEnable = VK_FALSE;

		// logicOp: какая именно логическая операция. Актуальна только при logicOpEnable = TRUE.
		// COPY — просто копировать новый цвет (по умолчанию, если бы логика была включена).
		colorBlending.logicOp = VK_LOGIC_OP_COPY;

		// attachmentCount: количество color attachment'ов, к которым применяется blending.
		// Для 2D — 1 (один color attachment). Для MRT (multiple render targets) — больше.
		colorBlending.attachmentCount = 1;

		// pAttachments: массив структур VkPipelineColorBlendAttachmentState,
		// по одной на каждый аттачмент. Это отдельная вложенная структура со своими полями.
		colorBlending.pAttachments = nullptr;  // заполняется отдельно

		// blendConstants: константы смешивания, используемые при VK_BLEND_FACTOR_*_CONSTANT.
		// Если вы не используете константные факторы — оставьте нули.
		colorBlending.blendConstants[0] = 0.0f;
		colorBlending.blendConstants[1] = 0.0f;
		colorBlending.blendConstants[2] = 0.0f;
		colorBlending.blendConstants[3] = 0.0f;
	}
	void SetInputAssembly()
	{
		inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		inputAssembly.pNext = nullptr;
		inputAssembly.flags = 0;

		// topology: как именно вершины собираются в примитивы.
		// TRIANGLE_LIST — каждые 3 вершины образуют отдельный треугольник.
		// TRIANGLE_STRIP — каждая новая вершина образует треугольник с двумя предыдущими.
		// TRIANGLE_FAN — все треугольники исходят из первой вершины.
		// LINE_LIST, LINE_STRIP, POINT_LIST — для линий и точек.
		inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

		// primitiveRestartEnable: позволяет вставить специальное значение индекса,
		// которое разрывает strip/fan и начинает новый примитив с этого индекса.
		// TRUE — полезно для отрисовки нескольких strip'ов одним вызовом.
		// FALSE — стандартное поведение. Требует фичи primitiveRestart у устройства,
		// если используется с strip/fan; для list — не требуется.
		inputAssembly.primitiveRestartEnable = VK_FALSE;
	}
	void SetVertexInputInfo()
	{
		vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertexInputInfo.pNext = nullptr;
		vertexInputInfo.flags = 0;

		// vertexBindingDescriptionCount: количество привязок вершинных буферов.
		// 0 — если геометрия генерируется в шейдере (как у кругов через gl_VertexIndex).
		// 1+ — если читаете вершины из VkBuffer через vkCmdBindVertexBuffers.
		vertexInputInfo.vertexBindingDescriptionCount = 0;

		// pVertexBindingDescriptions: массив структур VkVertexInputBindingDescription.
		// nullptr — если vertexBindingDescriptionCount = 0.
		// Каждая структура описывает один вершинный буфер: индекс привязки, stride, input rate.
		vertexInputInfo.pVertexBindingDescriptions = nullptr;

		// vertexAttributeDescriptionCount: количество атрибутов вершин.
		// 0 — если атрибуты не используются (геометрия генерируется).
		// 1+ — для позиций, нормалей, UV, цветов и т.д.
		vertexInputInfo.vertexAttributeDescriptionCount = 0;

		// pVertexAttributeDescriptions: массив структур VkVertexInputAttributeDescription.
		// nullptr — если vertexAttributeDescriptionCount = 0.
		// Каждая структура описывает один атрибут: location, binding, format, offset.
		vertexInputInfo.pVertexAttributeDescriptions = nullptr;
	}
	void SetVertexInput(const VkVertexInputBindingDescription& binding,
		const std::array<VkVertexInputAttributeDescription, 2>& attributes)
	{
		// 1. Сохраняем копии данных в поля класса
		vertexBindingDesc = binding;
		vertexAttrDescs = attributes;

		// 2. Настраиваем структуру пайплайна, указывая на наши поля
		vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertexInputInfo.pNext = nullptr;
		vertexInputInfo.flags = 0;
		vertexInputInfo.vertexBindingDescriptionCount = 1;

		// Передаем адрес (&) нашего поля
		vertexInputInfo.pVertexBindingDescriptions = &vertexBindingDesc;

		vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexAttrDescs.size());

		// Передаем указатель на данные массива, который теперь живет в классе
		vertexInputInfo.pVertexAttributeDescriptions = vertexAttrDescs.data();
	}
	void SetViewportState()
	{
		viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewportState.viewportCount = 1;
		viewportState.scissorCount = 1;
		viewportState.pViewports = nullptr;  // динамический
		viewportState.pScissors = nullptr;   // динамический
	}
	void SetDepthStencilState()
	{
		depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		depthStencil.pNext = nullptr;
		depthStencil.flags = 0;

		// depthTestEnable: включает тест глубины.
		// TRUE — фрагмент отбрасывается, если его глубина не проходит сравнение с буфером.
		// FALSE — все фрагменты проходят, глубина игнорируется. Для 2D — FALSE.
		depthStencil.depthTestEnable = VK_FALSE;

		// depthWriteEnable: включает запись глубины фрагмента в буфер.
		// TRUE — новые значения глубины записываются. FALSE — буфер не обновляется.
		// Для 2D — FALSE. Для depth prepass — TRUE при depthTestEnable = FALSE.
		depthStencil.depthWriteEnable = VK_FALSE;

		// depthCompareOp: как сравнивать глубину фрагмента с буфером.
		// LESS — фрагмент проходит, если его глубина МЕНЬШЕ (ближе к камере).
		// LESS_OR_EQUAL — стандарт для рендеринга с depth buffer.
		// ALWAYS — все фрагменты проходят (эквивалентно отключению теста).
		// Для 2D — ALWAYS (значение не важно при depthTestEnable = FALSE).
		depthStencil.depthCompareOp = VK_COMPARE_OP_ALWAYS;

		// depthBoundsTestEnable: включает тест границ глубины.
		// TRUE — фрагмент проходит, если его глубина в диапазоне [minDepthBounds, maxDepthBounds].
		// Требует фичи depthBounds у устройства. Для 2D — FALSE.
		depthStencil.depthBoundsTestEnable = VK_FALSE;

		// stencilTestEnable: включает тест трафарета.
		// TRUE — фрагмент проверяется по трафаретному буферу.
		// FALSE — трафарет игнорируется. Для 2D — FALSE.
		depthStencil.stencilTestEnable = VK_FALSE;

		// front, back: состояния трафарета для лицевых и обратных граней.
		// VkStencilOpState содержит failOp, passOp, depthFailOp, compareOp,
		// compareMask, writeMask, reference.
		// Актуальны только при stencilTestEnable = TRUE. Для 2D — дефолты.
		depthStencil.front = {};
		depthStencil.back = {};

		// minDepthBounds, maxDepthBounds: границы для depth bounds test.
		// Актуальны только при depthBoundsTestEnable = TRUE. Для 2D — [0.0, 1.0].
		depthStencil.minDepthBounds = 0.0f;
		depthStencil.maxDepthBounds = 1.0f;
	}
	void SetDynamicState()
	{
		dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamicState.pNext = nullptr;
		dynamicState.flags = 0;
		dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
		dynamicState.pDynamicStates = dynamicStates.data();
	}
	

	BasePipelineConfig()
	{
		SetRasterizer();
		SetMultisampling();
		SetColorBlending();
		SetInputAssembly();
		SetVertexInputInfo();
		SetViewportState();
		SetDepthStencilState();
		SetDynamicState();
	}
};

struct CircleConfig :BasePipelineConfig
{
	CircleConfig()
	{
		shaderPaths = {
			"CircleShader.vert.spv",
			"CircleShader.frag.spv"
		};
		inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
		rasterizer.cullMode = VK_CULL_MODE_NONE;   // уже так в базовой структуре, но для явности
		depthStencil.depthTestEnable = VK_FALSE;   // уже так в базовой структуре, но для явности
	}
};

struct QuadConfig : BasePipelineConfig {
	QuadConfig() {
		shaderPaths = { "QuadShader.vert.spv", "QuadShader.frag.spv" };
		inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST; // Квад = 2 треугольника (6 вершин)

		// Настраиваем Vertex Input
		SetVertexInput(getBindingDescription(), getAttributeDescriptions());
	}

	// Описывает, как данные вершины располагаются в буфере
	static VkVertexInputBindingDescription getBindingDescription() {
		VkVertexInputBindingDescription bindingDescription{};
		bindingDescription.binding = 0;
		bindingDescription.stride = sizeof(Vertex);
		bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		return bindingDescription;
	}

	// Описывает, как атрибуты (pos, color) извлекаются из памяти
	static std::array<VkVertexInputAttributeDescription, 2> getAttributeDescriptions() {
		std::array<VkVertexInputAttributeDescription, 2> attributeDescriptions{};

		// Атрибут 0: Позиция (vec2) - соответствует layout(location = 0) в шейдере
		attributeDescriptions[0].binding = 0;
		attributeDescriptions[0].location = 0;
		attributeDescriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
		attributeDescriptions[0].offset = offsetof(Vertex, pos);

		// Атрибут 1: Цвет (vec4) - соответствует layout(location = 1) в шейдере
		attributeDescriptions[1].binding = 0;
		attributeDescriptions[1].location = 1;
		attributeDescriptions[1].format = VK_FORMAT_R32G32B32A32_SFLOAT;
		attributeDescriptions[1].offset = offsetof(Vertex, color);

		return attributeDescriptions;
	}
};