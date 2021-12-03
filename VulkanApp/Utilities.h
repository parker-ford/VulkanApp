#pragma once
#include <fstream>

#define GLFW_INCLUDE_VULKAN
#include<GLFW/glfw3.h>
#include <glm/glm.hpp>>
const int MAX_FRAME_DRAWS = 2;
const int MAX_OBJECTS = 2;

const std::vector<const char*> deviceExtensions = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

//vertex data representation
struct Vertex {
	glm::vec3 pos;
	glm::vec3 col;
};

//Indices (locations) of Queue Families (if they exist)
struct QueueFamilyIndices {
	int graphicsFamily = -1;				//Location of Graphics Queue Family
	int presentationFamily = -1;			//Location of Presentation Family
	//Check if Queue families are valid
	bool isValid() {
		return graphicsFamily >= 0 && presentationFamily >= 0;
	}
};

struct SwapChainDetails {
	VkSurfaceCapabilitiesKHR surfaceCapabilities;		//Surface properties, e.g. image size/extent
	std::vector<VkSurfaceFormatKHR> formats;			//Surface image formats, e.g. RGBA and size of each color
	std::vector<VkPresentModeKHR> presentationModes;		//How images should be presented to screen

};

struct SwapChainImage {
	VkImage image;
	VkImageView imageView;
};

static std::vector<char> readFile(const std::string& filename) {
	//open stream from given file
	// std ios binary tessl stream to read file as binary
	// std ios ate teslls stream to start reading from the end of a file
	std::ifstream file(filename, std::ios::binary | std::ios::ate);

	//check if file stream successfully opened
	if (!file.is_open()) {
		throw std::runtime_error("Failed to open a file");
	}

	//get current read position and sue to resize file buffer
	size_t fileSize = (size_t)file.tellg();
	std::vector<char> fileBuffer(fileSize);

	//move to the start of the file
	file.seekg(0);

	//read the file data into the buffer, stream filesize in total
	file.read(fileBuffer.data(), fileSize);

	//close stream
	file.close();

	return fileBuffer;
}

static uint32_t findMemoryTypeIndex(VkPhysicalDevice physicalDevice, uint32_t allowedTypes, VkMemoryPropertyFlags properties)
{
	//get properties of physical device memory
	VkPhysicalDeviceMemoryProperties memoryProperties;
	vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);

	for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; i++) {
		if ((allowedTypes & (1 << i))
			&& (memoryProperties.memoryTypes[i].propertyFlags & properties) == properties)
		{
			//this memory type is valid, return index
			return i;
		}
	}
}

static void createBuffer(VkPhysicalDevice physicalDevice, VkDevice device, VkDeviceSize bufferSize, VkBufferUsageFlags bufferUsage, VkMemoryPropertyFlags bufferProperties, VkBuffer* buffer, VkDeviceMemory* bufferMemory) {
	//information to create a buffer, doenst include assignming memory
	VkBufferCreateInfo bufferInfo = {};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size =bufferSize;												//size of 1 vertex * number of vertices
	bufferInfo.usage = bufferUsage;												//multiple types of buffer possible,
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;							//similar to swap chain images, can share vertex buffers

	VkResult result = vkCreateBuffer(device, &bufferInfo, nullptr, buffer);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("Failed to create a vertex buffer");
	}

	//Get buffer moemory requirements
	VkMemoryRequirements memRequirements;
	vkGetBufferMemoryRequirements(device, *buffer, &memRequirements);

	//allocate memory to buffer
	VkMemoryAllocateInfo memoryAllocInfo = {};
	memoryAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memoryAllocInfo.allocationSize = memRequirements.size;
	memoryAllocInfo.memoryTypeIndex = findMemoryTypeIndex(physicalDevice, memRequirements.memoryTypeBits, bufferProperties);																//index of memory type on physical device that has required bit flags
																																															//host visible bit: CPU can interact with memory, host coherent bit: allows placement of data straight into buffer after mapping othterwise would have to specify manually
	//allocate memory to vkdevicememory
	result = vkAllocateMemory(device, &memoryAllocInfo, nullptr, bufferMemory);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("failed to allocate vertex buffer memory");
	}

	//allocate memory to given vertex buffer
	vkBindBufferMemory(device, *buffer, *bufferMemory, 0);

}

static void copyBuffer(VkDevice device, VkQueue transferQueue, VkCommandPool transferCommandPool, VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize bufferSize) {

	//command buffer to hold transfer commands
	VkCommandBuffer transferCommandBuffer;

	//command buffer details
	VkCommandBufferAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandPool = transferCommandPool;
	allocInfo.commandBufferCount = 1;

	//allocate command buffer from pool
	vkAllocateCommandBuffers(device, &allocInfo, &transferCommandBuffer);

	//information to begin the command buffer record
	VkCommandBufferBeginInfo beginInfo = { };
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;				//only using the command buffer once

	//begin recording transfer commands
	vkBeginCommandBuffer(transferCommandBuffer, &beginInfo);

	//regen of data to copy from and to
	VkBufferCopy bufferCopyRegion = {};
	bufferCopyRegion.srcOffset = 0;
	bufferCopyRegion.dstOffset = 0;
	bufferCopyRegion.size = bufferSize;

	//Command to copy src buffer to dst buffer
	vkCmdCopyBuffer(transferCommandBuffer, srcBuffer, dstBuffer, 1, &bufferCopyRegion);

	//end commands
	vkEndCommandBuffer(transferCommandBuffer);

	//queue submission information
	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &transferCommandBuffer;

	//submit transfer command to transfer queue and wait until it finishes
	vkQueueSubmit(transferQueue, 1, &submitInfo, VK_NULL_HANDLE);
	vkQueueWaitIdle(transferQueue);

	//free temporary command buffer back to pool
	vkFreeCommandBuffers(device, transferCommandPool, 1, &transferCommandBuffer);
}