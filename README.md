# Vulkan-Memory
A little memory manager for Vulkan API, written in c++.

# A Simple Example
## The Allocator
Firstly, create a new *Allocator* object :
```c++
//previously initialized
vk::PhysicalDevice physicalDevice;
vk::Device device;

vkm::Allocator allocator(physicalDevice, device);
```
The *allocator* is automatically destroyed.

## The Pool
For now, the only *Pool* that can be created is the *BufferPool*. The *BufferPoolCreateInfo* structure informs the size of the pool and the usage of all buffers created by this.
Note : the size of the pool after creation is not necessary the size requested.
```c++
struct Uniform {
  float color;
}
vk::DeviceSize poolSize = sizeof(Uniform);
vkm::BufferPool pool(allocator, { poolSize, vk::BufferUsageFlagBits::eUniformBuffer }, vk::MemoryPropertyFlagBits::eHostVisible |        vk::MemoryPropertyFlagBits::eHostCoherent);
```
At the end of its use, you can simply *destroy* the pool.
```c++
pool.destroy();
```
