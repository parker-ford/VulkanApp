#include "Mesh.h"

Mesh::Mesh() {

}

Mesh::Mesh(VkPhysicalDevice newPhysicalDevice, VkDevice newDevice, std::vector<Vertex>* vertices) {

	vertexCount = vertices->size();
	physicalDevice = newPhysicalDevice;
	device = newDevice;
	createVertexBuffer(vertices);
}

int Mesh::getVertexCount()
{
	return vertexCount;
}

VkBuffer Mesh::getVertexBuffer()
{
	return vertexBuffer;
}

void Mesh::destroyVertexBuffer()
{
	vkDestroyBuffer(device, vertexBuffer, nullptr);
	vkFreeMemory(device, vertexBufferMemory, nullptr);
}

Mesh::~Mesh() {

}

VkBuffer Mesh::createVertexBuffer(std::vector<Vertex>* vertices)
{
	//information to create a buffer, doenst include assignming memory
	VkBufferCreateInfo bufferInfo = {};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = sizeof(Vertex) * vertices->size();						//size of 1 vertex * number of vertices
	bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;						//multiple types of buffer possible, we want vertex buffer
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;							//similar to swap chain images, can share vertex buffers

	VkResult result = vkCreateBuffer(device, &bufferInfo, nullptr, &vertexBuffer);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("Failed to create a vertex buffer");
	}

	//Get buffer moemory requirements
	VkMemoryRequirements memRequirements;
	vkGetBufferMemoryRequirements(device, vertexBuffer, &memRequirements);

	//allocate memory to buffer
	VkMemoryAllocateInfo memoryAllocInfo = {};
	memoryAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memoryAllocInfo.allocationSize = memRequirements.size;
	memoryAllocInfo.memoryTypeIndex = findMemoryTypeIndex(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);		//index of memory type on physical device that has required bit flags
																																											//host visible bit: CPU can interact with memory, host coherent bit: allows placement of data straight into buffer after mapping othterwise would have to specify manually
	//allocate memory to vkdevicememory
	result = vkAllocateMemory(device, &memoryAllocInfo, nullptr, &vertexBufferMemory);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("failed to allocate vertex buffer memory");
	}

	//allocate memory to given vertex buffer
	vkBindBufferMemory(device, vertexBuffer, vertexBufferMemory, 0);

	//map memory to vertex buffer
	void* data;																	//1. create pointer to a point in normal memory
	vkMapMemory(device, vertexBufferMemory, 0, bufferInfo.size, 0, &data);		//2. map vertex buffer memory to that point
	memcpy(data, vertices->data(), (size_t)bufferInfo.size);					//3. copy memory from vertice vector to the point
	vkUnmapMemory(device, vertexBufferMemory);									//4. unmap the vertex buffer memory
}

uint32_t Mesh::findMemoryTypeIndex(uint32_t allowedTypes, VkMemoryPropertyFlags properties)
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
