#pragma once
#include <vulkan\vulkan.hpp>




namespace vmm {




	enum class Size : uint32_t {

		Kb = 1000,
		Mb = 1000000,
		Gb = 1000000000
	};




	struct Block {

		vk::DeviceSize
			size = 0,
			offset = 0;

		bool isFree = false;
	};




	class Pool {


	public:

		Pool();

		Pool(vk::Device device, vk::DeviceSize size, uint32_t memoryTypeIndex);

		void create(vk::Device device, vk::DeviceSize size, uint32_t memoryTypeIndex);

		void destroy();

		vk::DeviceSize/*offset*/ alloc(vk::DeviceSize size, vk::DeviceSize alignment);

		void free(vk::DeviceSize offset);

		inline bool hasUniqueFreeBlock() const noexcept {
			return _blocks.size() == 1 ? _blocks[0].isFree : false;
		}

		inline uint32_t getMemoryTypeIndex() const noexcept {
			return _memoryTypeIndex;
		}

		inline vk::DeviceMemory getMemory() const noexcept {
			return _memory;
		}

		inline vk::Device getDevice() const noexcept {
			return _device;
		}

		void print();

	private:

		void _create();

		std::vector<Block> _blocks;

		vk::DeviceMemory _memory;

		vk::DeviceSize _size;

		uint32_t _memoryTypeIndex;

		vk::Device _device;
	};




	struct Allocation {

		vk::DeviceSize offset = 0, size = 0;

		Pool* pool = nullptr;
	};




	class MemoryObject {


	public:

		MemoryObject();

		inline void* map() {
			return _allocation.pool->getDevice().mapMemory(
				_allocation.pool->getMemory(), _allocation.offset, _allocation.size);
		}

		inline void unmap() {
			_allocation.pool->getDevice().unmapMemory(_allocation.pool->getMemory());
		}

		inline void copy(void* mapped, const void* data, vk::DeviceSize offset, vk::DeviceSize size) {
			memcpy(static_cast<char*>(mapped) + offset, data, size);
		}

		void singleCopy(const void* data, vk::DeviceSize offset, vk::DeviceSize size);

		virtual void destroy();

		inline vk::DeviceSize size() const noexcept {
			return _allocation.size;
		}

	protected:

		Allocation _allocation;
	};




	class Buffer : public MemoryObject {


	public:

		Buffer();

		Buffer(vk::Buffer buffer, Allocation allocation);

		void destroy() override;

		operator vk::Buffer();

		inline vk::Buffer getVkBuffer() const noexcept {
			return _buffer;
		}

	private:

		vk::Buffer _buffer;
	};




	class Image : public MemoryObject {


	public:

		Image();

		Image(vk::Image image, Allocation allocation);

		void destroy() override;

		operator vk::Image();

		inline vk::Image getVkImage() const noexcept {
			return _image;
		}

	private:

		vk::Image _image;
	};




	class Allocator {


	public:

		Allocator();

		Allocator(vk::Device device, vk::PhysicalDevice physicalDevice);

		void create(vk::Device device, vk::PhysicalDevice physicalDevice);

		void destroy();

		Buffer createBuffer(const vk::BufferCreateInfo& bufferInfo, vk::MemoryPropertyFlags memoryProperties);

		Image createImage(const vk::ImageCreateInfo& imageInfo, vk::MemoryPropertyFlags memoryProperties);

		void clean();

		void print();

	private:

		uint32_t _findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties) const;

		Allocation _alloc(vk::MemoryRequirements memRequirements, uint32_t memoryTypeIndex);

		static vk::DeviceSize _POOL_SIZE;

		vk::PhysicalDevice _physicalDevice;

		vk::PhysicalDeviceMemoryProperties _physicalDeviceMemoryProperties;

		vk::Device _device;

		std::vector<Pool*> _pools;
	};
}