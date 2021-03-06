#include "dxvk_shader.h"

namespace dxvk {
  
  DxvkShaderModule::DxvkShaderModule(
    const Rc<vk::DeviceFn>&     vkd,
          VkShaderStageFlagBits stage,
    const SpirvCodeBuffer&      code)
  : m_vkd(vkd), m_stage(stage) {
    VkShaderModuleCreateInfo info;
    info.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.pNext    = nullptr;
    info.flags    = 0;
    info.codeSize = code.size();
    info.pCode    = code.data();
    
    if (m_vkd->vkCreateShaderModule(m_vkd->device(),
          &info, nullptr, &m_module) != VK_SUCCESS)
      throw DxvkError("DxvkComputePipeline::DxvkComputePipeline: Failed to create shader module");
  }
  
  
  DxvkShaderModule::~DxvkShaderModule() {
    m_vkd->vkDestroyShaderModule(
      m_vkd->device(), m_module, nullptr);
  }
  
  
  VkPipelineShaderStageCreateInfo DxvkShaderModule::stageInfo() const {
    VkPipelineShaderStageCreateInfo info;
    
    info.sType                = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    info.pNext                = nullptr;
    info.flags                = 0;
    info.stage                = m_stage;
    info.module               = m_module;
    info.pName                = "main";
    info.pSpecializationInfo  = nullptr;
    return info;
  }
  
  
  DxvkShader::DxvkShader(
          VkShaderStageFlagBits   stage,
          uint32_t                slotCount,
    const DxvkResourceSlot*       slotInfos,
    const SpirvCodeBuffer&        code)
  : m_stage(stage), m_code(code) {
    for (uint32_t i = 0; i < slotCount; i++)
      m_slots.push_back(slotInfos[i]);
  }
  
  
  DxvkShader::~DxvkShader() {
    
  }
  
  
  void DxvkShader::defineResourceSlots(
          DxvkDescriptorSlotMapping& mapping) const {
    for (const auto& slot : m_slots)
      mapping.defineSlot(slot.slot, slot.type, m_stage);
  }
  
  
  Rc<DxvkShaderModule> DxvkShader::createShaderModule(
    const Rc<vk::DeviceFn>&          vkd,
    const DxvkDescriptorSlotMapping& mapping) const {
    // Iterate over the code and replace every resource slot
    // index with the corresponding mapped binding index.
    SpirvCodeBuffer spirvCode = m_code;
    
    for (auto ins : spirvCode) {
      if (ins.opCode() == spv::OpDecorate
       && ins.arg(2)   == spv::DecorationBinding) {
        
        const uint32_t oldBinding = ins.arg(3);
        const uint32_t newBinding = mapping.getBindingId(oldBinding);
        ins.setArg(3, newBinding);
      }
    }
    
    return new DxvkShaderModule(vkd, m_stage, spirvCode);
  }
  
  
  void DxvkShader::dump(std::ostream&& outputStream) const {
    m_code.store(std::move(outputStream));
  }
  
  
  void DxvkShader::read(std::istream&& inputStream) {
    m_code = SpirvCodeBuffer(std::move(inputStream));
  }
  
}