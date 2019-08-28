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
Note : the size of the *Pool* after creation is not necessary the size requested but the *Pool* assure that the size cannot be smaller.
```c++
struct Uniform {
  float color;
}

vk::DeviceSize poolSize = sizeof(Uniform);

vkm::BufferPool pool(allocator, { poolSize, vk::BufferUsageFlagBits::eUniformBuffer }, vk::MemoryPropertyFlagBits::eHostVisible |  vk::MemoryPropertyFlagBits::eHostCoherent);
```
At the end of its use, you can simply *destroy* the pool.
```c++
pool.destroy();
```

## The Buffer
The *BufferCreateInfo* structure is based on the same *vk::BufferCreateInfo* without flags and usages (usages are already defined in the pool). You can destroy the buffer like *Pool* objects but before the pool that allocated it.
```c++
vkm::Buffer buffer(pool, { sizeof(Uniform), vk::SharingMode::eExclusive, 0, nullptr });

//using buffer

buffer.destroy();
//pool.destroy();
```
