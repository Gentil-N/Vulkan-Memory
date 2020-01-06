#include "vmm.hpp"
#include <iostream>




namespace vmm {




	Pool::Pool() : _blocks(), _memory(), _size(), _memoryTypeIndex(), _device() {

	}


	Pool::Pool(vk::Device device, vk::DeviceSize size, uint32_t memoryTypeIndex)
		: _device(device), _size(size), _memoryTypeIndex(memoryTypeIndex) {

		_create();
	}


	void Pool::create(vk::Device device, vk::DeviceSize size, uint32_t memoryTypeIndex) {

		_device = device;
		_size = size;
		_memoryTypeIndex = memoryTypeIndex;
		_create();
	}


	void Pool::destroy() {

		_blocks.clear();
		_device.freeMemory(_memory);
		_size = 0;
		_memoryTypeIndex = 0;
	}


	vk::DeviceSize Pool::alloc(vk::DeviceSize size, vk::DeviceSize alignment) {

		for (size_t blockIndex = 0; blockIndex < _blocks.size(); ++blockIndex) {

			if (!_blocks[blockIndex].isFree)//si non-libre
				continue;

			vk::DeviceSize shift = _blocks[blockIndex].offset % alignment == 0
				? 0 : alignment - _blocks[blockIndex].offset % alignment;

			if (_blocks[blockIndex].size - shift < size) {
				++blockIndex;//on peut sauter celui d'après : la "fusion" permet de ne pas avoir de blocks libres adjacents
				continue;
			}

			else if (_blocks[blockIndex].size - shift > size) {

				_blocks.insert(
					_blocks.begin() + blockIndex + 1,
					{ _blocks[blockIndex].size - (shift + size), _blocks[blockIndex].offset + shift + size, true }
				);
			}

			_blocks[blockIndex].size = size;
			_blocks[blockIndex].isFree = false;

			if (shift > 0) {
				Block shiftBlock = { shift, _blocks[blockIndex].offset, true };
				_blocks[blockIndex].offset += shift;
				_blocks.insert(_blocks.begin() + blockIndex, shiftBlock);
				return _blocks[blockIndex + 1].offset;
			}
			else
				return _blocks[blockIndex].offset;

		}
		return std::numeric_limits<vk::DeviceSize>::max();
	}


	void Pool::free(vk::DeviceSize offset) {

		for (size_t blockIndex = 0; blockIndex < _blocks.size(); ++blockIndex) {

			if (_blocks[blockIndex].isFree)
				continue;

			else if (_blocks[blockIndex].offset < offset)
				continue;

			else if (_blocks[blockIndex].offset > offset)//on l'a dépassé, c'est mort !
				break;

			_blocks[blockIndex].isFree = true;

			if (
				blockIndex > 0 && blockIndex < _blocks.size() - 1 &&
				_blocks[blockIndex - 1].isFree && _blocks[blockIndex + 1].isFree) {

				_blocks[blockIndex - 1].size += _blocks[blockIndex].size + _blocks[blockIndex + 1].size;
				_blocks.erase(_blocks.begin() + blockIndex);
				_blocks.erase(_blocks.begin() + blockIndex);
			}

			else if (blockIndex > 0 && _blocks[blockIndex - 1].isFree) {

				_blocks[blockIndex - 1].size += _blocks[blockIndex].size;
				_blocks.erase(_blocks.begin() + blockIndex);
			}

			else if (blockIndex < _blocks.size() - 1 && _blocks[blockIndex + 1].isFree) {

				_blocks[blockIndex].size += _blocks[blockIndex + 1].size;
				_blocks.erase(_blocks.begin() + blockIndex + 1);
			}
			return;
		}

		throw std::exception("failed to deallocate memory");
	}


	void Pool::_create() {

		_memory = _device.allocateMemory({ _size, _memoryTypeIndex });
		_blocks = { { _size, 0, true } };
	}


	void Pool::print() {

		for (size_t i = 0; i < _blocks.size(); ++i) {
			std::cout << "Block n." << i << " -> offset = " << _blocks[i].offset << " ; size = " << _blocks[i].size
				<< " ; state = " << (_blocks[i].isFree ? "free" : "reserved") << std::endl;
		}
	}




	MemoryObject::MemoryObject() : _allocation() {

	}


	void MemoryObject::singleCopy(const void* data, vk::DeviceSize offset, vk::DeviceSize size) {
		void* mapped = _allocation.pool->getDevice().mapMemory(
			_allocation.pool->getMemory(), _allocation.offset + offset, size);
		memcpy(mapped, data, size);
		_allocation.pool->getDevice().unmapMemory(_allocation.pool->getMemory());
	}


	void MemoryObject::destroy() {

		_allocation.pool->free(_allocation.offset);
		_allocation.offset = std::numeric_limits<size_t>::max();
		_allocation.pool = nullptr;
	}




	Buffer::Buffer() : MemoryObject(), _buffer() {

	}


	Buffer::Buffer(vk::Buffer buffer, Allocation allocation) : _buffer(buffer) {

		_allocation = allocation;
	}


	void Buffer::destroy() {

		_allocation.pool->getDevice().destroyBuffer(_buffer);
		MemoryObject::destroy();
	}


	Buffer::operator vk::Buffer() {
		return _buffer;
	}




	Image::Image() : MemoryObject(), _image() {

	}

	Image::Image(vk::Image image, Allocation allocation) : _image(image) {

		_allocation = allocation;
	}

	void Image::destroy() {

		_allocation.pool->getDevice().destroyImage(_image);
		MemoryObject::destroy();
	}

	Image::operator vk::Image() {
		return _image;
	}




	Allocator::Allocator() : _physicalDevice(), _device(), _pools(), _physicalDeviceMemoryProperties() {

	}


	Allocator::Allocator(vk::Device device, vk::PhysicalDevice physicalDevice)
		: _physicalDevice(physicalDevice), _device(device), _pools(),
		_physicalDeviceMemoryProperties(_physicalDevice.getMemoryProperties()) {

	}


	void Allocator::create(vk::Device device, vk::PhysicalDevice physicalDevice) {

		_physicalDevice = physicalDevice;
		_device = device;
		_physicalDeviceMemoryProperties = _physicalDevice.getMemoryProperties();
	}


	void Allocator::destroy() {

		for (Pool* pool : _pools) {
			pool->destroy();
			delete pool;
		}
		_pools.clear();
	}


	Buffer Allocator::createBuffer(const vk::BufferCreateInfo& bufferInfo, vk::MemoryPropertyFlags memoryProperties) {

		vk::Buffer vkBuffer = _device.createBuffer(bufferInfo);
		vk::MemoryRequirements memRequirements = _device.getBufferMemoryRequirements(vkBuffer);
		uint32_t memoryTypeIndex = _findMemoryType(memRequirements.memoryTypeBits, memoryProperties);

		Allocation allocation = _alloc(memRequirements, memoryTypeIndex);
		_device.bindBufferMemory(vkBuffer, allocation.pool->getMemory(), allocation.offset);

		return { vkBuffer, allocation };
	}


	Image Allocator::createImage(const vk::ImageCreateInfo& imageInfo, vk::MemoryPropertyFlags memoryProperties) {

		vk::Image vkImage = _device.createImage(imageInfo);
		vk::MemoryRequirements memRequirements = _device.getImageMemoryRequirements(vkImage);
		uint32_t memoryTypeIndex = _findMemoryType(memRequirements.memoryTypeBits, memoryProperties);

		Allocation allocation = _alloc(memRequirements, memoryTypeIndex);
		_device.bindImageMemory(vkImage, allocation.pool->getMemory(), allocation.offset);

		return { vkImage, allocation };
	}


	void Allocator::clean() {

		for (size_t i = _pools.size() - 1; i < std::numeric_limits<size_t>::max(); --i) {

			if (_pools[i]->hasUniqueFreeBlock()) {//s'il n'a qu'un seul bloc libre
				_pools[i]->destroy();//le détruire
				delete _pools[i];
				_pools.erase(_pools.begin() + i);//l'enlever de la liste
			}
		}
	}


	uint32_t Allocator::_findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties) const {

		for (uint32_t i = 0; i < _physicalDeviceMemoryProperties.memoryTypeCount; i++)

			if (
				(typeFilter & (1 << i)) &&
				(_physicalDeviceMemoryProperties.memoryTypes[i].propertyFlags & properties) == properties)

				return i;

		throw std::exception("failed to find suitable memory type");
	}


	Allocation Allocator::_alloc(vk::MemoryRequirements memRequirements, uint32_t memoryTypeIndex) {

		constexpr size_t lastNotCreatedPoolIndex = std::numeric_limits<size_t>::max();

		if (memRequirements.size > _POOL_SIZE) //si plus grand que la taille normale
			_pools.push_back(new Pool(_device, memRequirements.size, memoryTypeIndex));

		else {

			for (size_t i = 0; i < _pools.size(); ++i) {

				if (_pools[i]->getMemoryTypeIndex() == memoryTypeIndex) {//si même type de mémoire
					vk::DeviceSize offset = _pools[i]->alloc(memRequirements.size, memRequirements.alignment);
					if (offset == std::numeric_limits<vk::DeviceSize>::max())//si pas de place dans le pool
						continue;
					return { offset, memRequirements.size, _pools[i] };
				}
			}

			if (lastNotCreatedPoolIndex != std::numeric_limits<size_t>::max()) {//s'il y a un pool libre
				_pools[lastNotCreatedPoolIndex]->create(_device, _POOL_SIZE, memoryTypeIndex);
				vk::DeviceSize offset = _pools[lastNotCreatedPoolIndex]->alloc(
					memRequirements.size, memRequirements.alignment);
				return { offset, memRequirements.size, _pools[lastNotCreatedPoolIndex] };
			}

			_pools.push_back(new Pool(_device, _POOL_SIZE, memoryTypeIndex));
		}

		vk::DeviceSize offset = _pools.back()->alloc(memRequirements.size, memRequirements.alignment);
		return { offset, memRequirements.size, _pools.back() };
	}


	void Allocator::print() {

		std::cout << "Vulkan Memory (" << _pools.size() << " pools)" << std::endl;
		for (size_t i = 0; i < _pools.size(); ++i) {
			std::cout << "Pool n." << i << std::endl;
			_pools[i]->print();
		}
		std::cout << std::endl;
	}


	vk::DeviceSize Allocator::_POOL_SIZE = static_cast<vk::DeviceSize>(Size::Mb) * 50; //50 Mo par pool
}