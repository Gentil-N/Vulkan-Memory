# Vulkan-Memory
A little memory manager for Vulkan API, written in c++.

#A Simple Example
##The Allocator
Firstly, create a new *Allocator* object :
'''c++
//previously initialized
vk::PhysicalDevice physicalDevice;
vk::Device device;

vkm::Allocator allocator(physicalDevice, device);
'''
The *allocator* is automatically destroyed
