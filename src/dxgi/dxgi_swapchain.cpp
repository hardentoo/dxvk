#include "dxgi_factory.h"
#include "dxgi_resource.h"
#include "dxgi_swapchain.h"

namespace dxvk {
  
  DxgiSwapChain::DxgiSwapChain(
          DxgiFactory*          factory,
          IUnknown*             pDevice,
          DXGI_SWAP_CHAIN_DESC* pDesc)
  : m_factory (factory),
    m_desc    (*pDesc) {
    
    // Retrieve a device pointer that allows us to
    // communicate with the underlying D3D device
    if (FAILED(pDevice->QueryInterface(__uuidof(IDXGIPresentDevicePrivate),
        reinterpret_cast<void**>(&m_presentDevice))))
      throw DxvkError("DxgiSwapChain::DxgiSwapChain: Invalid device");
    
    // Retrieve the adapter, which is going
    // to be used to enumerate displays.
    Com<IDXGIAdapter> adapter;
    
    if (FAILED(pDevice->QueryInterface(__uuidof(IDXGIDevicePrivate),
        reinterpret_cast<void**>(&m_device))))
      throw DxvkError("DxgiSwapChain::DxgiSwapChain: Invalid device");
    
    if (FAILED(m_device->GetAdapter(&adapter))
     || FAILED(adapter->QueryInterface(__uuidof(IDXGIAdapterPrivate),
        reinterpret_cast<void**>(&m_adapter))))
      throw DxvkError("DxgiSwapChain::DxgiSwapChain: Failed to retrieve adapter");
    
    // Initialize frame statistics
    m_stats.PresentCount         = 0;
    m_stats.PresentRefreshCount  = 0;
    m_stats.SyncRefreshCount     = 0;
    m_stats.SyncQPCTime.QuadPart = 0;
    m_stats.SyncGPUTime.QuadPart = 0;
    
    // Create SDL window handle
    m_window = SDL_CreateWindowFrom(m_desc.OutputWindow);
    
    if (m_window == nullptr) {
      throw DxvkError(str::format(
        "DxgiSwapChain::DxgiSwapChain: Failed to create window:\n",
        SDL_GetError()));
    }
    
    // Adjust initial back buffer size. If zero, these
    // shall be set to the current window size.
    VkExtent2D windowSize = this->getWindowSize();
    
    if (m_desc.BufferDesc.Width  == 0) m_desc.BufferDesc.Width  = windowSize.width;
    if (m_desc.BufferDesc.Height == 0) m_desc.BufferDesc.Height = windowSize.height;
    
    // Set initial window mode and fullscreen state
    if (FAILED(this->SetFullscreenState(!pDesc->Windowed, nullptr)))
      throw DxvkError("DxgiSwapChain::DxgiSwapChain: Failed to set initial fullscreen state");
    
    this->createPresenter();
    this->createBackBuffer();
    TRACE(this);
  }
  
  
  DxgiSwapChain::~DxgiSwapChain() {
    TRACE(this);
    // We do not release the SDL window handle here since
    // that would destroy the underlying window as well.
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiSwapChain::QueryInterface(REFIID riid, void** ppvObject) {
    COM_QUERY_IFACE(riid, ppvObject, IUnknown);
    COM_QUERY_IFACE(riid, ppvObject, IDXGIObject);
    COM_QUERY_IFACE(riid, ppvObject, IDXGIDeviceSubObject);
    COM_QUERY_IFACE(riid, ppvObject, IDXGISwapChain);
    
    Logger::warn("DxgiSwapChain::QueryInterface: Unknown interface query");
    return E_NOINTERFACE;
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiSwapChain::GetParent(REFIID riid, void** ppParent) {
    return m_factory->QueryInterface(riid, ppParent);
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiSwapChain::GetDevice(REFIID riid, void** ppDevice) {
    return m_device->QueryInterface(riid, ppDevice);
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiSwapChain::GetBuffer(UINT Buffer, REFIID riid, void** ppSurface) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (Buffer > 0) {
      Logger::err("DxgiSwapChain::GetBuffer: Buffer > 0 not supported");
      return DXGI_ERROR_INVALID_CALL;
    }
    
    return m_backBufferIface->QueryInterface(riid, ppSurface);
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiSwapChain::GetContainingOutput(IDXGIOutput** ppOutput) {
    if (ppOutput != nullptr)
      return DXGI_ERROR_INVALID_CALL;
    
    // We can use the display index returned by SDL to query the
    // containing output, since DxgiAdapter::EnumOutputs uses the
    // same output IDs.
    std::lock_guard<std::mutex> lock(m_mutex);
    int32_t displayId = SDL_GetWindowDisplayIndex(m_window);
    
    if (displayId < 0) {
      Logger::err("DxgiSwapChain::GetContainingOutput: Failed to query window display index");
      return DXGI_ERROR_DRIVER_INTERNAL_ERROR;
    }
    
    return m_adapter->EnumOutputs(displayId, ppOutput);
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiSwapChain::GetDesc(DXGI_SWAP_CHAIN_DESC* pDesc) {
    if (pDesc == nullptr)
      return DXGI_ERROR_INVALID_CALL;
    
    std::lock_guard<std::mutex> lock(m_mutex);
    *pDesc = m_desc;
    return S_OK;
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiSwapChain::GetFrameStatistics(DXGI_FRAME_STATISTICS* pStats) {
    if (pStats == nullptr)
      return DXGI_ERROR_INVALID_CALL;
    
    std::lock_guard<std::mutex> lock(m_mutex);
    *pStats = m_stats;
    return S_OK;
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiSwapChain::GetFullscreenState(
          BOOL*         pFullscreen,
          IDXGIOutput** ppTarget) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    HRESULT hr = S_OK;
    
    if (pFullscreen != nullptr)
      *pFullscreen = !m_desc.Windowed;
    
    if ((ppTarget != nullptr) && !m_desc.Windowed)
      hr = this->GetContainingOutput(ppTarget);
    
    return hr;
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiSwapChain::GetLastPresentCount(UINT* pLastPresentCount) {
    if (pLastPresentCount == nullptr)
      return DXGI_ERROR_INVALID_CALL;
    
    std::lock_guard<std::mutex> lock(m_mutex);
    *pLastPresentCount = m_stats.PresentCount;
    return S_OK;
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiSwapChain::Present(UINT SyncInterval, UINT Flags) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    try {
      // Submit pending rendering commands
      // before recording the present code.
      m_presentDevice->FlushRenderingCommands();
    
      // TODO implement sync interval
      // TODO implement flags
      m_presenter->presentImage();
      return S_OK;
    } catch (const DxvkError& err) {
      Logger::err(err.message());
      return DXGI_ERROR_DRIVER_INTERNAL_ERROR;
    }
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiSwapChain::ResizeBuffers(
          UINT        BufferCount,
          UINT        Width,
          UINT        Height,
          DXGI_FORMAT NewFormat,
          UINT        SwapChainFlags) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    VkExtent2D windowSize = this->getWindowSize();
    
    m_desc.BufferDesc.Width = Width != 0 ? Width : windowSize.width;
    m_desc.BufferDesc.Height = Height != 0 ? Height : windowSize.height;
    
    m_desc.Flags = SwapChainFlags;
    
    if (BufferCount != 0)
      m_desc.BufferCount = BufferCount;
    
    if (NewFormat != DXGI_FORMAT_UNKNOWN)
      m_desc.BufferDesc.Format = NewFormat;
    
    try {
      m_presenter->recreateSwapchain(
        m_desc.BufferDesc.Width,
        m_desc.BufferDesc.Height,
        m_desc.BufferDesc.Format);
      this->createBackBuffer();
      return S_OK;
    } catch (const DxvkError& err) {
      Logger::err(err.message());
      return DXGI_ERROR_DRIVER_INTERNAL_ERROR;
    }
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiSwapChain::ResizeTarget(const DXGI_MODE_DESC* pNewTargetParameters) {
    if (pNewTargetParameters == nullptr)
      return DXGI_ERROR_INVALID_CALL;
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Applies to windowed mode
    SDL_SetWindowSize(m_window,
      pNewTargetParameters->Width,
      pNewTargetParameters->Height);
    
    // Applies to fullscreen mode
    SDL_DisplayMode displayMode;
    displayMode.format       = SDL_PIXELFORMAT_RGBA32;
    displayMode.w            = pNewTargetParameters->Width;
    displayMode.h            = pNewTargetParameters->Height;
    displayMode.refresh_rate = pNewTargetParameters->RefreshRate.Numerator
                             / pNewTargetParameters->RefreshRate.Denominator;
    displayMode.driverdata   = nullptr;
    
    // TODO test mode change flag
    
    if (SDL_SetWindowDisplayMode(m_window, &displayMode)) {
      Logger::err(str::format(
        "DxgiSwapChain::ResizeTarget: Failed to set display mode:\n",
        SDL_GetError()));
      return DXGI_ERROR_DRIVER_INTERNAL_ERROR;
    }
    
    try {
      m_presenter->recreateSwapchain(
        m_desc.BufferDesc.Width,
        m_desc.BufferDesc.Height,
        m_desc.BufferDesc.Format);
      return S_OK;
    } catch (const DxvkError& err) {
      Logger::err(err.message());
      return DXGI_ERROR_DRIVER_INTERNAL_ERROR;
    }
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiSwapChain::SetFullscreenState(
          BOOL          Fullscreen,
          IDXGIOutput*  pTarget) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Unconditionally reset the swap chain to windowed mode first.
    // This required if the application wants to move the window to
    // a different display while remaining in fullscreen mode.
    if (SDL_SetWindowFullscreen(m_window, 0)) {
      Logger::err(str::format(
        "DxgiSwapChain::SetFullscreenState: Failed to set windowed mode:\n",
        SDL_GetError()));
      return DXGI_ERROR_NOT_CURRENTLY_AVAILABLE;
    }
    
    m_desc.Windowed = !Fullscreen;
    
    if (Fullscreen) {
      // If a target output is specified, we need to move the
      // window to that output first while in windowed mode.
      if (pTarget != nullptr) {
        DXGI_OUTPUT_DESC outputDesc;
        
        if (FAILED(pTarget->GetDesc(&outputDesc))) {
          Logger::err("DxgiSwapChain::SetFullscreenState: Failed to query output properties");
          return DXGI_ERROR_NOT_CURRENTLY_AVAILABLE;
        }
        
        SDL_SetWindowPosition(m_window,
          outputDesc.DesktopCoordinates.left,
          outputDesc.DesktopCoordinates.top);
      }
      
      // Now that the window is located at the target location,
      // SDL should fullscreen it on the requested display. We
      // only use borderless fullscreen for now, may be changed.
      if (SDL_SetWindowFullscreen(m_window, SDL_WINDOW_FULLSCREEN_DESKTOP)) {
        Logger::err(str::format(
          "DxgiSwapChain::SetFullscreenState: Failed to set fullscreen mode:\n",
          SDL_GetError()));
        return DXGI_ERROR_NOT_CURRENTLY_AVAILABLE;
      }
    }
    
    return S_OK;
  }
  
  
  void DxgiSwapChain::createPresenter() {
    m_presenter = new DxgiPresenter(
      m_device->GetDXVKDevice(),
      m_desc.OutputWindow,
      m_desc.BufferDesc.Width,
      m_desc.BufferDesc.Height,
      m_desc.BufferDesc.Format);
  }
  
  
  void DxgiSwapChain::createBackBuffer() {
    VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_1_BIT;
    
    if (FAILED(GetSampleCount(m_desc.SampleDesc.Count, &sampleCount)))
      throw DxvkError("DxgiSwapChain::createBackBuffer: Invalid sample count");
    
    const Rc<DxvkImage> backBuffer = m_presenter->createBackBuffer(
      m_desc.BufferDesc.Width, m_desc.BufferDesc.Height,
      m_adapter->LookupFormat(m_desc.BufferDesc.Format).actual,
      sampleCount);
    
    const Com<IDXGIImageResourcePrivate> resource
      = new DxgiImageResource(m_device.ptr(), backBuffer,
          DXGI_USAGE_BACK_BUFFER | m_desc.BufferUsage);
    
    // Wrap the back buffer image into an interface
    // that the device can use to access the image.
    if (FAILED(m_presentDevice->WrapSwapChainBackBuffer(resource.ptr(), &m_desc, &m_backBufferIface)))
      throw DxvkError("DxgiSwapChain::createBackBuffer: Failed to create back buffer interface");
  }
  
  
  VkExtent2D DxgiSwapChain::getWindowSize() const {
    int winWidth = 0;
    int winHeight = 0;
    
    SDL_GetWindowSize(m_window, &winWidth, &winHeight);
    
    VkExtent2D result;
    result.width  = winWidth;
    result.height = winHeight;
    return result;
  }
  
  
  HRESULT DxgiSwapChain::GetSampleCount(UINT Count, VkSampleCountFlagBits* pCount) const {
    switch (Count) {
      case  1: *pCount = VK_SAMPLE_COUNT_1_BIT;  return S_OK;
      case  2: *pCount = VK_SAMPLE_COUNT_2_BIT;  return S_OK;
      case  4: *pCount = VK_SAMPLE_COUNT_4_BIT;  return S_OK;
      case  8: *pCount = VK_SAMPLE_COUNT_8_BIT;  return S_OK;
      case 16: *pCount = VK_SAMPLE_COUNT_16_BIT; return S_OK;
    }
    
    return E_INVALIDARG;
  }
  
}
