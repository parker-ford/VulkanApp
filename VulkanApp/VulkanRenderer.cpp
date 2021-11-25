#include "VulkanRenderer.h"
//test git
VulkanRenderer::VulkanRenderer()
{
}

int VulkanRenderer::init(GLFWwindow* newWindow)
{
	window = newWindow;

	try {
		createInstance();
		setupDebugMessenger();
		createSurface();
		getPhysicalDevice();
		createLogicalDevice();
		createSwapChain();
		createRenderPass();
		createGraphicsPipeline();
		createFrameBuffers();
		createCommandPool();
		//Create a mesh
		//vertex data
		std::vector<Vertex> meshVertices1 = {
			{{-0.1, -0.4, 0.0}, {1.0, 0.0, 0.0}},			//0
			{{-0.1, 0.4, 0.0}, {0.0, 1.0, 0.0}},			//1
			{{-0.9, 0.4, 0.0}, {0.0, 0.0, 1.0}},			//2
			{{-0.9, -0.4, 0.0}, {1.0, 1.0, 0.0}},			//3
		};

		std::vector<Vertex> meshVertices2 = {
			{{0.9, -0.4, 0.0}, {1.0, 0.0, 0.0}},			//0
			{{0.9, 0.4, 0.0}, {0.0, 1.0, 0.0}},				//1
			{{0.1, 0.4, 0.0}, {0.0, 0.0, 1.0}},				//2
			{{0.1, -0.4, 0.0}, {1.0, 1.0, 0.0}},			//3
		};

		//index data
		std::vector<uint32_t> meshIndices = {
			0,1,2,
			2,3,0
		};

		Mesh firstMesh = Mesh(mainDevice.physicalDevice, mainDevice.logicalDevice, graphicsQueue, graphicsCommandPool, &meshVertices1, &meshIndices);
		Mesh secondMesh = Mesh(mainDevice.physicalDevice, mainDevice.logicalDevice, graphicsQueue, graphicsCommandPool, &meshVertices2, &meshIndices);

		meshList.push_back(firstMesh);
		meshList.push_back(secondMesh);

		createCommandBuffers();
		recordCommands();
		createSynchronization();
	}
	catch (const std::runtime_error& e) {
		printf("ERROR: %s\n", e.what());
		return EXIT_FAILURE;
	}


	return 0;
}

void VulkanRenderer::draw()
{
	// --Get next image--
	vkWaitForFences(mainDevice.logicalDevice, 1, &drawFences[currentFrame], VK_TRUE, std::numeric_limits<uint64_t>::max());
	vkResetFences(mainDevice.logicalDevice, 1, &drawFences[currentFrame]);
	//1. get the next available image to draw to and set something to signal when wer're finished with the image (semaphore)
	uint32_t imageIndex;
	vkAcquireNextImageKHR(mainDevice.logicalDevice, swapchain, std::numeric_limits<uint64_t>::max(), imageAvailable[currentFrame], VK_NULL_HANDLE, &imageIndex);


	//--submit command buffer to render--
	//2. submit command buffer to queue to be executed, make sure it waits for the image to be signlaed as available before drawing
	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = &imageAvailable[currentFrame];
	VkPipelineStageFlags waitStages[] = {
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
	};
	submitInfo.pWaitDstStageMask = waitStages;									//stages to check semaphore at
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffers[imageIndex];					//command buffer to submit
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = &renderFinished[currentFrame];								//sempahore to signale when command buffer finsishes

	//submit command buffer to queue
	VkResult result = vkQueueSubmit(graphicsQueue, 1, &submitInfo, drawFences[currentFrame]);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("failed to submit command buffer to queue");
	}

	//--Present rendered image to screen
	//3. present image to screen when it has signalled finsished rendering
	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &renderFinished[currentFrame];
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &swapchain;
	presentInfo.pImageIndices = &imageIndex;

	result = vkQueuePresentKHR(presentationQueue, &presentInfo);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("Failed to present the image");
	}

	currentFrame = (currentFrame + 1) % MAX_FRAME_DRAWS;
}

void VulkanRenderer::cleanup()
{
	//wait until no actions being run before destroying
	vkDeviceWaitIdle(mainDevice.logicalDevice);

	for (size_t i = 0; i < meshList.size(); i++) {
		meshList[i].destroyBuffers();
	}

	for (size_t i = 0; i < MAX_FRAME_DRAWS; i++) {
		vkDestroySemaphore(mainDevice.logicalDevice, renderFinished[i], nullptr);
		vkDestroySemaphore(mainDevice.logicalDevice, imageAvailable[i], nullptr);
		vkDestroyFence(mainDevice.logicalDevice, drawFences[i], nullptr);
	}
	vkDestroyCommandPool(mainDevice.logicalDevice, graphicsCommandPool, nullptr);
	for (auto framebuffer : swapChainFramebuffers) {
		vkDestroyFramebuffer(mainDevice.logicalDevice, framebuffer, nullptr);
	}
	vkDestroyPipeline(mainDevice.logicalDevice, graphicsPipeline, nullptr);
	vkDestroyPipelineLayout(mainDevice.logicalDevice, pipelineLayout, nullptr);
	vkDestroyRenderPass(mainDevice.logicalDevice, renderPass, nullptr);
	for (auto image : swapChainImages) {
		vkDestroyImageView(mainDevice.logicalDevice, image.imageView, nullptr);
	}
	vkDestroySwapchainKHR(mainDevice.logicalDevice, swapchain, nullptr);
	vkDestroySurfaceKHR(instance, surface, nullptr);
	if (enableValidationLayers) {
		DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
	}
	vkDestroyDevice(mainDevice.logicalDevice, nullptr); 
	vkDestroyInstance(instance, nullptr);
}

VulkanRenderer::~VulkanRenderer()
{
}

void VulkanRenderer::createInstance()
{
	//Information about the app itself
	//Most data here doesnt effect the program. It is for developer convienience
	VkApplicationInfo appInfo = {};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "Vulkan App";						//Custom name of app
	appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);			//Custom version of app
	appInfo.pEngineName = "No Engine";								//Custom engine name
	appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);				//Custom engine version
	appInfo.apiVersion = VK_API_VERSION_1_0;						//Vulkan version

	//Creation information for a VkInstance
	VkInstanceCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &appInfo;
	

	//Create list to hold instance extensions
	std::vector<const char*> instanceExtensions = std::vector<const char*>();

	uint32_t glfwExtensionCount = 0;								//GLFW may require multiple extensions
	const char** glfwExtensions;									//Extensions passes as array of cstrings, so need pointer (the array) to pointer (cstring)

	//Get GLFW extensions
	glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

	//Add GLFW extensions to list of extensions
	for (size_t i = 0; i < glfwExtensionCount; i++) {
		instanceExtensions.push_back(glfwExtensions[i]);
	}

	if (enableValidationLayers) {
		instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	}

	//Check instance extensions are supported
	if (!checkInstanceExtensionSupport(&instanceExtensions)) {
		throw std::runtime_error("VkInstance does not support required extensions");
	}

	createInfo.enabledExtensionCount = static_cast<uint32_t>(instanceExtensions.size());
	createInfo.ppEnabledExtensionNames = instanceExtensions.data();

	
	//Check if validation layers are supported
	if (enableValidationLayers && !checkValidationSupport()) {
		throw std::runtime_error("Validation layers requested, but not available");
	}
	//Enabling or disabling validation layers in the instance
	VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
	if (enableValidationLayers) {
		createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
		createInfo.ppEnabledLayerNames = validationLayers.data();

		populateDebugMessengerCreateInfo(debugCreateInfo);
		createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
	}
	else {
		createInfo.enabledLayerCount = 0;
		createInfo.ppEnabledLayerNames = nullptr;
	}

	//create instance
	VkResult result = vkCreateInstance(&createInfo, nullptr, &instance);

	if (result != VK_SUCCESS) {
		throw std::runtime_error("Failed to create a Vulkan Instance");
	}
}

void VulkanRenderer::setupDebugMessenger()
{
	if (!enableValidationLayers) return;

	VkDebugUtilsMessengerCreateInfoEXT createInfo{};
	populateDebugMessengerCreateInfo(createInfo);
	if (CreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &debugMessenger) != VK_SUCCESS) {
		throw std::runtime_error("failed to set up debug messenger!");
	}
}

void VulkanRenderer::populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo)
{
	createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	createInfo.pfnUserCallback = debugCallback;
}

VkResult VulkanRenderer::CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger)
{
	auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
	if (func != nullptr) {
		return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
	}
	else {
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}
}

void VulkanRenderer::DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator)
{
	auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
	if (func != nullptr) {
		func(instance, debugMessenger, pAllocator);
	}
}

void VulkanRenderer::createLogicalDevice()
{
	//Get the queue family indices for the chosen Physical Device
	QueueFamilyIndices indices = getQueueFamilies(mainDevice.physicalDevice);

	//Vector for queue creation information and set for family indices
	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
	std::set<int> queueFamilyIndices = { indices.graphicsFamily, indices.presentationFamily };

	//Queues the logical device needs to create and info to do so
	for (int queueFamilyIndex : queueFamilyIndices) {
		VkDeviceQueueCreateInfo queueCreateInfo = {};
		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.queueFamilyIndex = queueFamilyIndex;												//The index of the family to create a queue from
		queueCreateInfo.queueCount = 1;																		//Number of queues to create
		float priority = 1.0f;
		queueCreateInfo.pQueuePriorities = &priority;														//Vulkan needs to know how to handle multiple queues, 1 = highest priority, 0 = lowest.

		queueCreateInfos.push_back(queueCreateInfo);
	}

	//Information to create Logical device
	VkDeviceCreateInfo deviceCreateInfo = {};
	deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());					//Number of Queue Create Infos
	deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();											//List of queue create infos so device can create queues
	deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());				//Number of enabled logical device extensions
	deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();										//List of enabled logical device extensions
	
	//physical device features the logical device will be using
	VkPhysicalDeviceFeatures deviceFeatures = {};

	deviceCreateInfo.pEnabledFeatures = &deviceFeatures;													//Physical Device features logical device will use

	//Create the logical device for the given physical device
	VkResult result = vkCreateDevice(mainDevice.physicalDevice, &deviceCreateInfo, nullptr, &mainDevice.logicalDevice);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("Failed to create a logical Device!");
	}

	//Queues are created at the same time as the device
	//We want handle to queues
	//From given logical device, of given queue family, of given queue index, (0 since only one queue), place reference in given  VkQueue
	vkGetDeviceQueue(mainDevice.logicalDevice, indices.graphicsFamily, 0, &graphicsQueue);
	vkGetDeviceQueue(mainDevice.logicalDevice, indices.presentationFamily, 0, &presentationQueue);

}

void VulkanRenderer::createSurface()
{
	//Create surface (creating a surface create info struct, runs the create surface function, returns result
	VkResult result = glfwCreateWindowSurface(instance, window, nullptr, &surface);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("Failed to create a surface");
	}
}

void VulkanRenderer::createSwapChain()
{
	//Get Swap Chain Details so wer can pick best settings
	SwapChainDetails swapChainDetails = getSwapChainDetails(mainDevice.physicalDevice);

	//1. Choose best surface format
	VkSurfaceFormatKHR surfaceFormat = chooseBestSurfaceFormat(swapChainDetails.formats);
	//2. best presentation mode
	VkPresentModeKHR presentMode = chooseBestPresentationMode(swapChainDetails.presentationModes);
	//3. choose swap chain image resolution
	VkExtent2D extent = chooseSwapExtent(swapChainDetails.surfaceCapabilities);

	//how many images are in  the swap chain.. Get 1 ore than the minumum to allow triple buffering
	//if maximagecount is 0 then it is limitless
	uint32_t imageCount = swapChainDetails.surfaceCapabilities.minImageCount + 1;
	if (swapChainDetails.surfaceCapabilities.maxImageCount > 0 && swapChainDetails.surfaceCapabilities.maxImageCount < imageCount) {
		imageCount = swapChainDetails.surfaceCapabilities.maxImageCount;
	}

	VkSwapchainCreateInfoKHR swapChainCreateInfo = {};
	swapChainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapChainCreateInfo.surface = surface;														//swapchain surface
	swapChainCreateInfo.imageFormat = surfaceFormat.format;										//Swapchain format
	swapChainCreateInfo.imageColorSpace = surfaceFormat.colorSpace;								//Swapchain color space
	swapChainCreateInfo.presentMode = presentMode;												//swapchian presentation mode
	swapChainCreateInfo.imageExtent = extent;													//swapchain image extent
	swapChainCreateInfo.minImageCount = imageCount;												//minimum images in swapchain
	swapChainCreateInfo.imageArrayLayers = 1;													//Number of layers for each image in chain
	swapChainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;						//what attachment images will be used as
	swapChainCreateInfo.preTransform = swapChainDetails.surfaceCapabilities.currentTransform;	//transform to perform on swapchain
	swapChainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;						//how to handle blending images with external graphics
	swapChainCreateInfo.clipped = VK_TRUE;														//wether to clip parts of image not in view, eg covered by another window
	
	//get queue family indices
	QueueFamilyIndices indices = getQueueFamilies(mainDevice.physicalDevice);

	//if graphics and presentation families are different, then swapchain must let images be shared between families
	if (indices.graphicsFamily != indices.presentationFamily) {
		uint32_t queueFamilyIndices[] = {
			(uint32_t)indices.graphicsFamily,
			(uint32_t)indices.presentationFamily
		};

		swapChainCreateInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;					//Image share handling
		swapChainCreateInfo.queueFamilyIndexCount = 2;										//Number of queues to share images btween
		swapChainCreateInfo.pQueueFamilyIndices = queueFamilyIndices;						// array of queues to share between
	}
	else {
		swapChainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		swapChainCreateInfo.queueFamilyIndexCount = 0;
		swapChainCreateInfo.pQueueFamilyIndices = nullptr;
	}

	swapChainCreateInfo.oldSwapchain = VK_NULL_HANDLE;

	//Create swapchain

	VkResult result = vkCreateSwapchainKHR(mainDevice.logicalDevice, &swapChainCreateInfo, nullptr, &swapchain);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("Failed to create a swapchain");
	}

	//store for later reference
	swapChainImageFormat = surfaceFormat.format;
	swapChainExtent = extent;

	
	//Get swap chain images
	uint32_t swapChainImageCount;
	vkGetSwapchainImagesKHR(mainDevice.logicalDevice, swapchain, &swapChainImageCount, nullptr);
	std::vector<VkImage> images(swapChainImageCount);
	vkGetSwapchainImagesKHR(mainDevice.logicalDevice, swapchain, &swapChainImageCount, images.data());

	for (VkImage image : images) {
		//store image handle
		SwapChainImage swapChainImage = {};
		swapChainImage.image = image;
		
		// Create image view
		swapChainImage.imageView = createImageView(image, swapChainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT);
		
		//add to swap chain image list
		swapChainImages.push_back(swapChainImage);
	}
}

void VulkanRenderer::createRenderPass()
{

	//Color attatchment of render pass
	VkAttachmentDescription colorAttachment = {};
	colorAttachment.format = swapChainImageFormat;										//Format to use for attachment
	colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;									//number of samples to write for multisampling
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;								//Describes what to do with attachment before rendering
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;								//describes what to do with attachment after rendering
	colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;					//describes what to do with stencil before rendering
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;					//describes what to do with stencil after rendering

	//framebuffer data will be store as an image, but images can be given different data layouts to give optimal use for certain operations
	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;							//Image data layout before render pass starts
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;						//Image data layout after render pass (to change to)

	//attachment reference uses an attachment index that refers to index the attachment list passed to renderpasscreateinfo
	VkAttachmentReference colorAttachmentReference = {};
	colorAttachmentReference.attachment = 0;
	colorAttachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;


	//Information about a particular subpass the render pass is using
	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;						//pipeline type subpass is to be bound to
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorAttachmentReference;

	//Need to determine when layout transitions occur using subpass dependcies
	std::array<VkSubpassDependency, 2> subpassDependencies;

	//coonversion from VK_IMAGE_IMAGE_UNDEFINDED to VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
	//transistion must happen after...
	subpassDependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;							//Subpass index (VK_SUBPASS_EXTERNAL = special value meaning outside of renderpass)
	subpassDependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;			//pipelinme stage
	subpassDependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;					//stage access mask (memoryt access)

	//but must happen before...
	subpassDependencies[0].dstSubpass = 0;
	subpassDependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	subpassDependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT 
										 | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	subpassDependencies[0].dependencyFlags = 0;

	//coonversion from VK_IMAGE_IMAGE_UNDEFINDED to VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
	//transistion must happen after...
	subpassDependencies[1].srcSubpass = 0;							
	subpassDependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;;			
	subpassDependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT
										 | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;				

	//but must happen before...
	subpassDependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
	subpassDependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	subpassDependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
	subpassDependencies[1].dependencyFlags = 0;

	//create info for render pass
	VkRenderPassCreateInfo renderPassCreateInfo = {};
	renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassCreateInfo.attachmentCount = 1;
	renderPassCreateInfo.pAttachments = &colorAttachment;
	renderPassCreateInfo.subpassCount = 1;
	renderPassCreateInfo.pSubpasses = &subpass;
	renderPassCreateInfo.dependencyCount = static_cast<uint32_t>(subpassDependencies.size());
	renderPassCreateInfo.pDependencies = subpassDependencies.data();

	VkResult result = vkCreateRenderPass(mainDevice.logicalDevice, &renderPassCreateInfo, nullptr, &renderPass);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("failed to create a render pass");
	}
}

void VulkanRenderer::createGraphicsPipeline()
{

	//read in SPIR-V code of shaders
	auto vertexShaderCode = readFile("Shaders/vert.spv");
	auto fragmentShaderCode = readFile("Shaders/frag.spv");


	//build shader modules to link to graphics pipeline
	VkShaderModule vertexShaderModule = createShaderModule(vertexShaderCode);
	VkShaderModule fragmentShaderModule = createShaderModule(fragmentShaderCode);

	// -- shader stage creation information --

	//Vertex Stage creation information
	VkPipelineShaderStageCreateInfo vertexShaderCreateInfo = {};
	vertexShaderCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertexShaderCreateInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;								//Shader stage name
	vertexShaderCreateInfo.module = vertexShaderModule;										//shader module to be used by stage
	vertexShaderCreateInfo.pName = "main";													//entry point into shader

	//Fragment stage creation information
	VkPipelineShaderStageCreateInfo fragmentShaderCreateInfo = {};
	fragmentShaderCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragmentShaderCreateInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;							//Shader stage name
	fragmentShaderCreateInfo.module = fragmentShaderModule;									//shader module to be used by stage
	fragmentShaderCreateInfo.pName = "main";												//entry point into shader

	//put shader stage creation info into array
	//graphics piel;ine creation info requires array of shader stage creates
	VkPipelineShaderStageCreateInfo shaderStages[] = { vertexShaderCreateInfo, fragmentShaderCreateInfo };

	// how the data for a single vertex (including info such as position, color, texture, coords, normals) is as a whole
	VkVertexInputBindingDescription bindingDescription = {};
	bindingDescription.binding = 0;															//can bind multiple streams of data, this defines which one
	bindingDescription.stride = sizeof(Vertex);												//size of singe vertex object
	bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;								//how to move between data after each vertex

	//how the data for an attribute is defined within a vertex
	std::array<VkVertexInputAttributeDescription, 2> attributeDescriptions;

	//positions attribute
	attributeDescriptions[0].binding = 0;													//which binding the data is at (should be same as above)
	attributeDescriptions[0].location = 0;													//location in shader where data will be read from
	attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;							//format the data will take (helps define the size of the data)
	attributeDescriptions[0].offset = offsetof(Vertex, pos);									//where this attribute is defined in the data for a single vertex

	//color attribute
	attributeDescriptions[1].binding = 0;													
	attributeDescriptions[1].location = 1;													
	attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;							
	attributeDescriptions[1].offset = offsetof(Vertex, col);									


	//--vertex input-- 
	VkPipelineVertexInputStateCreateInfo vertexInputCreateInfo = {};
	vertexInputCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputCreateInfo.vertexBindingDescriptionCount = 1;
	vertexInputCreateInfo.pVertexBindingDescriptions = &bindingDescription;								//list of vertex binding descriptions (data spacing/stride info)
	vertexInputCreateInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
	vertexInputCreateInfo.pVertexAttributeDescriptions = attributeDescriptions.data();							//list of vertex attribute descriptions (data format and where to bind to/from)

	//--input assembly --
	VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;							//Primitive type to assemble vertices as
	inputAssembly.primitiveRestartEnable = VK_FALSE;										//Allow overrideing of strip tobology to start new primitives

	//--viewport and scissor--
	//create a viewport info struct
	VkViewport viewport = {};
	viewport.x = 0.0f;																		//x start coordinate
	viewport.y = 0.0f;																		//y start coordinate
	viewport.width = (float)swapChainExtent.width;											//width of viewport
	viewport.height = (float)swapChainExtent.height;										//height of viewport
	viewport.minDepth = 0.0f;																//min frame buffer depth
	viewport.maxDepth = 1.0f;																//max frame buffer depth

	//create a scissor info struct
	VkRect2D scissor = {};
	scissor.offset = { 0,0 };																//offset to use region from
	scissor.extent = swapChainExtent;														//extent to descrive region to use, starting at offset

	VkPipelineViewportStateCreateInfo viewportStateCreateInfo = {};
	viewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportStateCreateInfo.viewportCount = 1;
	viewportStateCreateInfo.pViewports = &viewport;
	viewportStateCreateInfo.scissorCount = 1;
	viewportStateCreateInfo.pScissors = &scissor;

	//--Dynamic state--
	//dynamic states to enable
	//std::vector<VkDynamicState> dynamicStateEnables;
	//dynamicStateEnables.push_back(VK_DYNAMIC_STATE_VIEWPORT);
	//dynamicStateEnables.push_back(VK_DYNAMIC_STATE_SCISSOR);

	//dynamic state creation info
	//VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo = {};
	//dynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	//dynamicStateCreateInfo.dynamicStateCount = static_cast<uint32_t>(dynamicStateEnables.size());
	//dynamicStateCreateInfo.pDynamicStates = dynamicStateEnables.data();

	//--rasterizer--
	VkPipelineRasterizationStateCreateInfo rasterizerCreateInfo = {};
	rasterizerCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizerCreateInfo.depthClampEnable = VK_FALSE;												//clips fragments past a far plane(need to enable a device feature)
	rasterizerCreateInfo.rasterizerDiscardEnable = VK_FALSE;										//whether to discard data and skip rasterizer. never creates framnets, oinly suitable for pipline without framebuffer output
	rasterizerCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;										//how polygons are filled(change for wireframe or points)(other than fill needs a gpu feature)
	rasterizerCreateInfo.lineWidth = 1.0f;															//how thick lines should be when drawn(gpu feature)
	rasterizerCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;											//which face of a trianlge to cull
	rasterizerCreateInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;										//winding to determine which side is front
	rasterizerCreateInfo.depthBiasEnable = VK_FALSE;												//whether to add depth bias to fragments

	//--multisampling--
	VkPipelineMultisampleStateCreateInfo multisamplingCreateInfo = {};
	multisamplingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisamplingCreateInfo.sampleShadingEnable = VK_FALSE;											//enable multisampling or not
	multisamplingCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;							//number of samples per fragment

	//--blending--
	//Blending decides how to belnd a new color being written to a fragment, with the old value

	//blend attatchment state (how blending is handled)]
	VkPipelineColorBlendAttachmentState colorState = {};
	colorState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT					//colors to apply blending to
		| VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorState.blendEnable = VK_TRUE;																//enable blending

	//blending uses equation: (srcColorBlendFactor * newColor) colorBlendOp (dstColorBlendFactor * oldColor)
	colorState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	colorState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	colorState.colorBlendOp = VK_BLEND_OP_ADD;

	//summarised: (VK_BLEND_FACTOR_SRC_ALPHA * new color) + (VK_BLEND_FACTOR * old color)

	colorState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	colorState.alphaBlendOp = VK_BLEND_OP_ADD;
	//summarised: 1*newalpha + 0*oldalpha = newalpha

	VkPipelineColorBlendStateCreateInfo colorBlendingCreateInfo = {};
	colorBlendingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlendingCreateInfo.logicOpEnable = VK_FALSE;												//alternative to calculations is to use logical operations
	colorBlendingCreateInfo.attachmentCount = 1;
	colorBlendingCreateInfo.pAttachments = &colorState;

	// --pipelinhe layout (todo: apply future descriptor set layout) --
	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
	pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutCreateInfo.setLayoutCount = 0;
	pipelineLayoutCreateInfo.pSetLayouts = nullptr;
	pipelineLayoutCreateInfo.pushConstantRangeCount = 0;
	pipelineLayoutCreateInfo.pPushConstantRanges = nullptr;

	//Create Pipeline Layout
	VkResult result = vkCreatePipelineLayout(mainDevice.logicalDevice, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("failed to create pipeline layout");
	}

	//--depth stencil testing--
	//TODO set up depth stencil testing

	//--graphics pipeline creation--

	VkGraphicsPipelineCreateInfo pipelineCreateInfo = {};
	pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineCreateInfo.stageCount = 2;
	pipelineCreateInfo.pStages = shaderStages;
	pipelineCreateInfo.pVertexInputState = &vertexInputCreateInfo;
	pipelineCreateInfo.pInputAssemblyState = &inputAssembly;
	pipelineCreateInfo.pViewportState = &viewportStateCreateInfo;
	pipelineCreateInfo.pDynamicState = nullptr;
	pipelineCreateInfo.pRasterizationState = &rasterizerCreateInfo;
	pipelineCreateInfo.pMultisampleState = &multisamplingCreateInfo;
	pipelineCreateInfo.pColorBlendState = &colorBlendingCreateInfo;
	pipelineCreateInfo.pDepthStencilState = nullptr;
	pipelineCreateInfo.layout = pipelineLayout;												//pipeline layout pipeline should use
	pipelineCreateInfo.renderPass = renderPass;												//rener pass description the pipline should use
	pipelineCreateInfo.subpass = 0;															//subpass of render pass to use with the pipeline

	//pipeline derivatives : can create multiple pipelines that derive from on another for optimization
	pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;									//existing pipeline to derive from
	pipelineCreateInfo.basePipelineIndex = -1;												//or index of pipeline being created to derive in case creating multiple at once

	//create graphics pipeline
	result = vkCreateGraphicsPipelines(mainDevice.logicalDevice, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &graphicsPipeline);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("failed to create a graphics pipeline");
	}
	//Destroy shader modules, no longer needed after pipeline created
	vkDestroyShaderModule(mainDevice.logicalDevice, fragmentShaderModule, nullptr);
	vkDestroyShaderModule(mainDevice.logicalDevice, vertexShaderModule, nullptr);

}

void VulkanRenderer::createFrameBuffers()
{
	//resize framebuffer count to equal swap chain image count
	swapChainFramebuffers.resize(swapChainImages.size());

	//create a framebuffer for each swapchain image
	for (size_t i = 0; i < swapChainFramebuffers.size(); i++) {

		std::array<VkImageView, 1> attachments = {
			swapChainImages[i].imageView
		};

		VkFramebufferCreateInfo frambebufferCreateInfo = {};
		frambebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		frambebufferCreateInfo.renderPass = renderPass;												//render pass layout the frambuffer will be used with
		frambebufferCreateInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
		frambebufferCreateInfo.pAttachments = attachments.data();									//list of attachments, 1 to 1 with redner pass
		frambebufferCreateInfo.width = swapChainExtent.width;
		frambebufferCreateInfo.height = swapChainExtent.height;
		frambebufferCreateInfo.layers = 1;															//frame buffer layers

		VkResult result = vkCreateFramebuffer(mainDevice.logicalDevice, &frambebufferCreateInfo, nullptr, &swapChainFramebuffers[i]);
		if (result != VK_SUCCESS) {
			throw std::runtime_error("failed to create a framebuffer");
		}
	}
}

void VulkanRenderer::createCommandPool()
{

	//Get indices of queue familys from device
	QueueFamilyIndices queueFamilyIndices = getQueueFamilies(mainDevice.physicalDevice);

	VkCommandPoolCreateInfo poolInfo = {};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily; // queue family type that buffers from this command pool will use

	//create a graphics framily command pool
	VkResult result = vkCreateCommandPool(mainDevice.logicalDevice, &poolInfo, nullptr, &graphicsCommandPool);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("Failed to create a command pool");
	}
}

void VulkanRenderer::createCommandBuffers()
{
	//resize command buffer count to have one for each framebuffer
	commandBuffers.resize(swapChainFramebuffers.size());

	VkCommandBufferAllocateInfo cbAllocInfo = {};
	cbAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	cbAllocInfo.commandPool = graphicsCommandPool;
	cbAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

	cbAllocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());

	VkResult result = vkAllocateCommandBuffers(mainDevice.logicalDevice, &cbAllocInfo, commandBuffers.data());
	if (result != VK_SUCCESS) {
		throw std::runtime_error("Failed to allocate Command Buffers!");
	}
}

void VulkanRenderer::createSynchronization()
{
	imageAvailable.resize(MAX_FRAME_DRAWS);
	renderFinished.resize(MAX_FRAME_DRAWS);
	drawFences.resize(MAX_FRAME_DRAWS);

	// semaphore creation information
	VkSemaphoreCreateInfo semaphoreCreateInfo = {};
	semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	//fence creation information
	VkFenceCreateInfo fenceCreateInfo = {};
	fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	for (size_t i = 0; i < MAX_FRAME_DRAWS; i++) {
		if (vkCreateSemaphore(mainDevice.logicalDevice, &semaphoreCreateInfo, nullptr, &imageAvailable[i]) != VK_SUCCESS ||
			vkCreateSemaphore(mainDevice.logicalDevice, &semaphoreCreateInfo, nullptr, &renderFinished[i]) != VK_SUCCESS ||
			vkCreateFence(mainDevice.logicalDevice, &fenceCreateInfo, nullptr, &drawFences[i]) != VK_SUCCESS) {
		
			throw std::runtime_error("failed to create a semaphore or fence");
		}
	}
}

void VulkanRenderer::recordCommands()
{
	//information about how to begin each command buffer
	VkCommandBufferBeginInfo bufferBeginInfo = {};
	bufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	bufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;					//buffer can be resubmitted if already submitted

	//information about how to begin a render pass, only needed for graphical applications
	VkRenderPassBeginInfo renderPassBeginInfo = {};
	renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassBeginInfo.renderPass = renderPass;											//render pass to begin
	renderPassBeginInfo.renderArea.offset = { 0,0 };										//start point of render pass in pixels
	renderPassBeginInfo.renderArea.extent = swapChainExtent;								//size of region to run render pass on
	VkClearValue clearValues[] = {
		{0.6f, 0.65f, 0.4f, 1.0f}
	};
	renderPassBeginInfo.pClearValues = clearValues;											//List of clear values (TODO: depth attachment clear value)
	renderPassBeginInfo.clearValueCount = 1;

	for (size_t i = 0; i < commandBuffers.size(); i++) {

		renderPassBeginInfo.framebuffer = swapChainFramebuffers[i];

		//start recording commands into command buffer
		VkResult result = vkBeginCommandBuffer(commandBuffers[i], &bufferBeginInfo);
		if (result != VK_SUCCESS) {
			throw std::runtime_error("Failed to start recording a command buffer");
		}
			//Begin render pass
			vkCmdBeginRenderPass(commandBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

				//Bind pipeline to be used in render pass
				vkCmdBindPipeline(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

				for (size_t j = 0; j < meshList.size(); j++) {

					VkBuffer vertexBuffers[] = { meshList[j].getVertexBuffer() };					//buffers to bind
					VkDeviceSize offsets[] = { 0 };												//offsets into buffers being bound
					vkCmdBindVertexBuffers(commandBuffers[i], 0, 1, vertexBuffers, offsets);	//command to bind vertex buffer before drawing with them

					vkCmdBindIndexBuffer(commandBuffers[i], meshList[j].getIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);

					//Execute our pipeline
					vkCmdDrawIndexed(commandBuffers[i], meshList[j].getIndexCount(), 1, 0, 0, 0);
				}

			//end render pass
			vkCmdEndRenderPass(commandBuffers[i]);

		//stop recording to command buffer
		result = vkEndCommandBuffer(commandBuffers[i]);
		if (result != VK_SUCCESS) {
			throw std::runtime_error("Failed to stop recording a command buffer");
		}
	}
}

void VulkanRenderer::getPhysicalDevice()
{
	//Enumerate Physical devices the vkInstance can acces
	uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

	//If no devices available, then none support vulkan
	if (deviceCount == 0) {
		throw std::runtime_error("Cant find GPUs that support Vulkan instance");
	}

	//Get List of Physical Devices
	std::vector<VkPhysicalDevice> deviceList(deviceCount);
	vkEnumeratePhysicalDevices(instance, &deviceCount, deviceList.data());

	for (const auto &device : deviceList) {
		if (checkDeviceSuitable(device)) {
			mainDevice.physicalDevice = device;
			break;
		}
	}
	

}

bool VulkanRenderer::checkInstanceExtensionSupport(std::vector<const char*>* checkExtensions)
{
	//Need to get number of extensions to create array of correct size to hold extensions
	uint32_t extensionCount = 0;
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

	//Create a list of VkExtensionProperties using count
	std::vector<VkExtensionProperties> extensions(extensionCount);
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());

	//Check if given extensions are in list of available extensions
	for (const auto& checkExtension : *checkExtensions) {
		bool hasExtension = false;
		for (const auto& extension : extensions) {
			if (strcmp(checkExtension, extension.extensionName)) {
				hasExtension = true;
				break;
			}
		}
		if (!hasExtension) {
			return false;
		}

	}
	return true;
}

bool VulkanRenderer::checkDeviceExtensionSupport(VkPhysicalDevice device)
{
	//Get device extension count
	uint32_t extensionsCount = 0;
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionsCount, nullptr);

	//If no extension found return fail
	if (extensionsCount == 0) {
		return false;
	}

	//Populate list of extensions
	std::vector<VkExtensionProperties> extensions(extensionsCount);
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionsCount, extensions.data());

	//Check for extension
	for (const auto& deviceExtension : deviceExtensions) {
		bool hasExtension = false;
		for (const auto& extension : extensions) {
			if (strcmp(deviceExtension, extension.extensionName)) {
				hasExtension = true;
				break;
			}
		}

		if (!hasExtension) {
			return false;
		}
	}

	return true;

}

bool VulkanRenderer::checkDeviceSuitable(VkPhysicalDevice device)
{
	/*
	//Information about the device itself (ID, name, tpye, vendor, etc)
	VkPhysicalDeviceProperties devicePorperties;
	vkGetPhysicalDeviceProperties(device, &devicePorperties);

	//Information about what the device can do (geo shader, tess shader, wide lines, etc)
	VkPhysicalDeviceFeatures deviceFeatures;
	vkGetPhysicalDeviceFeatures(device, &deviceFeatures);
	*/
	QueueFamilyIndices indices = getQueueFamilies(device);

	bool extensionsSupported = checkDeviceExtensionSupport(device);
	
	bool swapChainValid = false;
	if (extensionsSupported) {
		SwapChainDetails  swapChainDetails = getSwapChainDetails(device);
		swapChainValid = !swapChainDetails.presentationModes.empty() && !swapChainDetails.formats.empty();
	}

	return indices.isValid() && extensionsSupported && swapChainValid;
}

bool VulkanRenderer::checkValidationSupport()
{
	uint32_t layerCount;
	vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

	std::vector<VkLayerProperties> availableLayers(layerCount);
	vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

	for (const char* layerName : validationLayers) {
		bool layerFound = false;

		for (const auto& layerProperties : availableLayers) {
			if (strcmp(layerName, layerProperties.layerName) == 0) {
				layerFound = true;
				break;
			}
		}

		if (!layerFound) {
			return false;
		}
	}

	return true;
}

QueueFamilyIndices VulkanRenderer::getQueueFamilies(VkPhysicalDevice device)
{
	QueueFamilyIndices indices;
	
	//Get all queue family property info for the given device
	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);


	std::vector<VkQueueFamilyProperties> queueFamilyList(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilyList.data());


	//Go through each queue family and check if it has at least 1 of the required types fo queue
	int i = 0;
	for (const auto& queueFamily : queueFamilyList) {
		// First Check if queue family has at least 1 queue in that family (could have no queues)
		//Queue can be multiple types defined through bitfield. Need to bitwise AND with VK_QUEUE_*_BIT to check if has required type
		if (queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
			indices.graphicsFamily = i;					//if queue family is valid, then get index
		}

		//Check if queue family supports presentation
		VkBool32 presentationSupport = false;
		vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentationSupport);
		// check if queue is presentation type (can be both graphics and presentation)
		if (queueFamily.queueCount > 0 && presentationSupport) {
			indices.presentationFamily = i;
		}
		//Check if queue family indices are in a valid state stop searching if so
		if (indices.isValid()) {
			break;
		}

		i++;
	}

	return indices;
}

SwapChainDetails VulkanRenderer::getSwapChainDetails(VkPhysicalDevice device)
{
	SwapChainDetails swapChainDetails;
	

	// -- CAPABILITIES --
	//Get the surface capabilities for the given surface on the given physical device
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &swapChainDetails.surfaceCapabilities);

	// -- FORMATS --
	uint32_t formatCount = 0;
	vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
	
	//if formats, get list of formats
	if (formatCount != 0) {
		swapChainDetails.formats.resize(formatCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, swapChainDetails.formats.data());
	}

	// --PRESENTATION MODES --
	uint32_t presentationCount = 0;
	vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentationCount, nullptr);

	//if presentation modes, get list of presentation modes
	if (presentationCount != 0) {
		swapChainDetails.presentationModes.resize(presentationCount);
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentationCount, swapChainDetails.presentationModes.data());
	}

	return swapChainDetails;

}

//Best format is subjective, ours will be:
// Format		:	VK_FORMAT_R8G8B8A8_UNORM || VK_FORMAT_B8G8R8A8_UNORM
// ColorSpace	:	VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
VkSurfaceFormatKHR VulkanRenderer::chooseBestSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats)
{
	//if only one format available and is undefined, then all formats are available
	if (formats.size() == 1 && formats[0].format == VK_FORMAT_UNDEFINED) {
		return { VK_FORMAT_R8G8B8A8_UNORM,  VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
	}

	//if restricted search for optimal format
	for (const auto& format : formats) {
		if ((format.format == VK_FORMAT_R8G8B8A8_UNORM || format.format == VK_FORMAT_B8G8R8A8_UNORM) && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
			return format;
		}
	}

	//if cant return optimal, return first format
	return formats[0];
}

VkPresentModeKHR VulkanRenderer::chooseBestPresentationMode(const std::vector<VkPresentModeKHR> presentationModes)
{

	//Look for mailbox presentation mode
	for (const auto& presentationMode : presentationModes) {
		if (presentationMode == VK_PRESENT_MODE_MAILBOX_KHR) {
			return presentationMode;
		}
	}

	//if cant find, this presentation mode must be present
	return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D VulkanRenderer::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& surfaceCapabilities)
{
	//if current extent is at numeric limit, then extent can vary, otherwise it is the size of the window
	if (surfaceCapabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
		return surfaceCapabilities.currentExtent;
	}
	//set manually
	else {
		//get window size
		int width, height;
		glfwGetFramebufferSize(window, &width, &height);

		//create new extent
		VkExtent2D newExtent = {};
		newExtent.width = static_cast<uint32_t>(width);
		newExtent.height = static_cast<uint32_t>(height);

		//surface also defines max and min, make sure within boundaries
		newExtent.width = std::max(surfaceCapabilities.minImageExtent.width , std::min(surfaceCapabilities.maxImageExtent.width, newExtent.width));
		newExtent.height = std::max(surfaceCapabilities.minImageExtent.height, std::min(surfaceCapabilities.maxImageExtent.height, newExtent.height));

		return newExtent;
	}
}

VkImageView VulkanRenderer::createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags)
{
	VkImageViewCreateInfo viewCreateInfo = {};
	viewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewCreateInfo.image = image;												//Image to create view for
	viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;							//Tyoe of image
	viewCreateInfo.format = format;												//format of image data
	viewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;				//allows remapping of rgba components to rgba values
	viewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
	viewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
	viewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

	//subresources allow the view to view only a part of the image
	viewCreateInfo.subresourceRange.aspectMask = aspectFlags;					//which aspect of image to view (eg COLOR_BIT for viewing color);
	viewCreateInfo.subresourceRange.baseMipLevel = 0;							//start mipmap level to view from
	viewCreateInfo.subresourceRange.levelCount = 1;								//number of mipmap levels to view
	viewCreateInfo.subresourceRange.baseArrayLayer = 0;							//start array level to view from
	viewCreateInfo.subresourceRange.layerCount = 1;								//number of array levels to view

	//Create image view and return it
	VkImageView imageView;
	VkResult result = vkCreateImageView(mainDevice.logicalDevice, &viewCreateInfo, nullptr, &imageView);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("failed to create an image view");
	}

	return imageView;
}

VkShaderModule VulkanRenderer::createShaderModule(const std::vector<char>& code)
{
	VkShaderModuleCreateInfo shaderModuleCreateInfo = {};
	shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	shaderModuleCreateInfo.codeSize = code.size();
	shaderModuleCreateInfo.pCode = reinterpret_cast<const uint32_t *> (code.data());


	VkShaderModule shaderModule;
	VkResult result = vkCreateShaderModule(mainDevice.logicalDevice, &shaderModuleCreateInfo, nullptr, &shaderModule);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("failed to create a shader module");
	}

	return shaderModule;
}

VKAPI_ATTR VkBool32 VKAPI_CALL VulkanRenderer::debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData)
{

	//TODO: check if message is of some level of severity:
	if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
		//Message is important enough to show
		std::cerr << "Validation layer: " << pCallbackData->pMessage << std::endl;
	}

	return VK_FALSE;
}

