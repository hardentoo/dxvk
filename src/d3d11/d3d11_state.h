#pragma once

#include <unordered_map>

#include "d3d11_blend.h"
#include "d3d11_depth_stencil.h"
#include "d3d11_rasterizer.h"

namespace dxvk {
  
  class D3D11Device;
  
  struct D3D11StateDescHash {
    size_t operator () (const D3D11_BLEND_DESC& desc) const;
    size_t operator () (const D3D11_DEPTH_STENCILOP_DESC& desc) const;
    size_t operator () (const D3D11_DEPTH_STENCIL_DESC& desc) const;
    size_t operator () (const D3D11_RASTERIZER_DESC& desc) const;
    size_t operator () (const D3D11_RENDER_TARGET_BLEND_DESC& desc) const;
  };
  
  
  struct D3D11StateDescEqual {
    bool operator () (const D3D11_BLEND_DESC& a, const D3D11_BLEND_DESC& b) const;
    bool operator () (const D3D11_DEPTH_STENCILOP_DESC& a, const D3D11_DEPTH_STENCILOP_DESC& b) const;
    bool operator () (const D3D11_DEPTH_STENCIL_DESC& a, const D3D11_DEPTH_STENCIL_DESC& b) const;
    bool operator () (const D3D11_RASTERIZER_DESC& a, const D3D11_RASTERIZER_DESC& b) const;
    bool operator () (const D3D11_RENDER_TARGET_BLEND_DESC& a, const D3D11_RENDER_TARGET_BLEND_DESC& b) const;
  };
  
  
  /**
   * \brief Unique state object set
   * 
   * When creating state objects, D3D11 first checks if
   * an object with the same description already exists
   * and returns it if that is the case. This class
   * implements that behaviour.
   */
  template<typename T>
  class D3D11StateObjectSet {
    using DescType = typename T::DescType;
  public:
    
    /**
     * \brief Retrieves a state object
     * 
     * Returns an object with the same description or
     * creates a new one if no such object exists.
     * \param [in] device The calling D3D11 device
     * \param [in] desc State object description
     * \returns Pointer to the state object
     */
    T* Create(D3D11Device* device, const DescType& desc) {
      std::lock_guard<std::mutex> lock(m_mutex);
      
      auto pair = m_objects.find(desc);
      
      if (pair != m_objects.end())
        return pair->second.ref();
      
      Com<T> result = new T(device, desc);
      m_objects.insert({ desc, result });
      return result.ref();
    }
    
  private:
    
    std::mutex                                 m_mutex;
    std::unordered_map<DescType, Com<T>,
      D3D11StateDescHash, D3D11StateDescEqual> m_objects;
    
  };
  
}
