#pragma once
#include <fstream>

const std::vector<const char*> deviceExtensions = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME
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