#pragma once

#include "dxbc_common.h"
#include "dxbc_decoder.h"
#include "dxbc_reader.h"

namespace dxvk {
  
  /**
   * \brief Shader code chunk
   * 
   * Stores the DXBC shader code itself, as well
   * as some meta info about the shader, i.e. what
   * type of shader this is.
   */
  class DxbcShex : public RcObject {
    
  public:
    
    DxbcShex(DxbcReader reader);
    ~DxbcShex();
    
    DxbcProgramVersion version() const {
      return m_version;
    }
    
    DxbcDecoder begin() const {
      return DxbcDecoder(m_code.data(), m_code.size());
    }
    
    DxbcDecoder end() const {
      return DxbcDecoder();
    }
    
  private:
    
    DxbcProgramVersion    m_version;
    std::vector<uint32_t> m_code;
    
  };
  
}