#include "dxgi_resource.h"

namespace dxvk {
  
  DxgiImageResource::DxgiImageResource(
          IDXGIDevicePrivate*             pDevice,
    const Rc<DxvkImage>&                  image,
          UINT                            usageFlags)
  : Base(pDevice, usageFlags), m_image(image) {
    
  }
  
  
  DxgiImageResource::DxgiImageResource(
          IDXGIDevicePrivate*             pDevice,
    const dxvk::DxvkImageCreateInfo*      pCreateInfo,
          VkMemoryPropertyFlags           memoryFlags,
          UINT                            usageFlags)
  : Base(pDevice, usageFlags) {
    m_image = pDevice->GetDXVKDevice()->createImage(
      *pCreateInfo, memoryFlags);
  }
  
  
  DxgiImageResource::~DxgiImageResource() {
    
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiImageResource::QueryInterface(REFIID riid, void** ppvObject) {
    COM_QUERY_IFACE(riid, ppvObject, IUnknown);
    COM_QUERY_IFACE(riid, ppvObject, IDXGIObject);
    COM_QUERY_IFACE(riid, ppvObject, IDXGIDeviceSubObject);
    COM_QUERY_IFACE(riid, ppvObject, IDXGIResource);
    COM_QUERY_IFACE(riid, ppvObject, IDXGIImageResourcePrivate);
    
    if (m_layer != nullptr)
      return m_layer->QueryInterface(riid, ppvObject);
    
    Logger::err("DxgiImageResource::QueryInterface: Unknown interface query");
    return E_NOINTERFACE;
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiImageResource::GetParent(REFIID riid, void** ppParent) {
    Logger::err("DxgiImageResource::GetParent: Unknown interface query");
    return E_NOINTERFACE;
  }
  
  
  Rc<DxvkImage> STDMETHODCALLTYPE DxgiImageResource::GetDXVKImage() {
    return m_image;
  }
  
  
  void STDMETHODCALLTYPE DxgiImageResource::SetResourceLayer(IUnknown* pLayer) {
    m_layer = pLayer;
  }
  
  
  
  
  DxgiBufferResource::DxgiBufferResource(
          IDXGIDevicePrivate*             pDevice,
    const dxvk::DxvkBufferCreateInfo*     pCreateInfo,
          VkMemoryPropertyFlags           memoryFlags,
          UINT                            usageFlags)
  : Base(pDevice, usageFlags) {
    m_buffer = pDevice->GetDXVKDevice()->createBuffer(
      *pCreateInfo, memoryFlags);
  }
  
  
  DxgiBufferResource::~DxgiBufferResource() {
    
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiBufferResource::QueryInterface(REFIID riid, void** ppvObject) {
    COM_QUERY_IFACE(riid, ppvObject, IUnknown);
    COM_QUERY_IFACE(riid, ppvObject, IDXGIObject);
    COM_QUERY_IFACE(riid, ppvObject, IDXGIDeviceSubObject);
    COM_QUERY_IFACE(riid, ppvObject, IDXGIResource);
    COM_QUERY_IFACE(riid, ppvObject, IDXGIBufferResourcePrivate);
    
    if (m_layer != nullptr)
      return m_layer->QueryInterface(riid, ppvObject);
    
    Logger::err("DxgiBufferResource::QueryInterface: Unknown interface query");
    return E_NOINTERFACE;
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiBufferResource::GetParent(REFIID riid, void** ppParent) {
    Logger::err("DxgiBufferResource::GetParent: Unknown interface query");
    return E_NOINTERFACE;
  }
  
  
  Rc<DxvkBuffer> STDMETHODCALLTYPE DxgiBufferResource::GetDXVKBuffer() {
    return m_buffer;
  }
  
  
  void STDMETHODCALLTYPE DxgiBufferResource::SetResourceLayer(IUnknown* pLayer) {
    m_layer = pLayer;
  }
  
}


extern "C" {
  
  DLLEXPORT HRESULT __stdcall DXGICreateImageResourcePrivate(
          IDXGIDevicePrivate*             pDevice,
    const dxvk::DxvkImageCreateInfo*      pCreateInfo,
          VkMemoryPropertyFlags           memoryFlags,
          UINT                            usageFlags,
          IDXGIImageResourcePrivate**     ppResource) {
    try {
      *ppResource = dxvk::ref(new dxvk::DxgiImageResource(
        pDevice, pCreateInfo, memoryFlags, usageFlags));
      return S_OK;
    } catch (const dxvk::DxvkError& e) {
      dxvk::Logger::err(e.message());
      return DXGI_ERROR_UNSUPPORTED;
    }
  }
  
  
  DLLEXPORT HRESULT __stdcall DXGICreateBufferResourcePrivate(
          IDXGIDevicePrivate*             pDevice,
    const dxvk::DxvkBufferCreateInfo*     pCreateInfo,
          VkMemoryPropertyFlags           memoryFlags,
          UINT                            usageFlags,
          IDXGIBufferResourcePrivate**    ppResource) {
    try {
      *ppResource = dxvk::ref(new dxvk::DxgiBufferResource(
        pDevice, pCreateInfo, memoryFlags, usageFlags));
      return S_OK;
    } catch (const dxvk::DxvkError& e) {
      dxvk::Logger::err(e.message());
      return DXGI_ERROR_UNSUPPORTED;
    }
  }
  
}