#include "dxbc_compiler.h"

namespace dxvk {
  
  DxbcCompiler::DxbcCompiler(
    const DxbcProgramVersion& version,
    const Rc<DxbcIsgn>&       isgn,
    const Rc<DxbcIsgn>&       osgn)
  : m_gen(DxbcCodeGen::create(version, isgn, osgn)) { }
  
  
  DxbcCompiler::~DxbcCompiler() {
    
  }
  
  
  void DxbcCompiler::processInstruction(const DxbcInstruction& ins) {
    const DxbcOpcodeToken token = ins.token();
    
    switch (token.opcode()) {
      case DxbcOpcode::DclGlobalFlags:
        return this->dclGlobalFlags(ins);
      
      case DxbcOpcode::DclConstantBuffer:
        return this->dclConstantBuffer(ins);
        
      case DxbcOpcode::DclResource:
        return this->dclResource(ins);
        
      case DxbcOpcode::DclSampler:
        return this->dclSampler(ins);
        
      case DxbcOpcode::DclInput:
      case DxbcOpcode::DclInputSiv:
      case DxbcOpcode::DclInputSgv:
      case DxbcOpcode::DclInputPs:
      case DxbcOpcode::DclInputPsSiv:
      case DxbcOpcode::DclInputPsSgv:
      case DxbcOpcode::DclOutput:
      case DxbcOpcode::DclOutputSiv:
      case DxbcOpcode::DclOutputSgv:
        return this->dclInterfaceVar(ins);
      
      case DxbcOpcode::DclTemps:
          return this->dclTemps(ins);
      
      case DxbcOpcode::Add:
        return this->opAdd(ins);
      
      case DxbcOpcode::Mad:
        return this->opMad(ins);
      
      case DxbcOpcode::Mul:
        return this->opMul(ins);
      
      case DxbcOpcode::Mov:
        return this->opMov(ins);
      
      case DxbcOpcode::Dp2:
        return this->opDpx(ins, 2);
      
      case DxbcOpcode::Dp3:
        return this->opDpx(ins, 3);
      
      case DxbcOpcode::Dp4:
        return this->opDpx(ins, 4);
      
      case DxbcOpcode::Rsq:
        return this->opRsq(ins);
      
      case DxbcOpcode::Ret:
        return this->opRet(ins);
      
      case DxbcOpcode::Sample:
        return this->opSample(ins);
      
      default:
        Logger::err(str::format(
          "DxbcCompiler::processInstruction: Unhandled opcode: ",
          token.opcode()));
    }
  }
  
  
  Rc<DxvkShader> DxbcCompiler::finalize() {
    return m_gen->finalize();
  }
  
  
  void DxbcCompiler::dclGlobalFlags(const DxbcInstruction& ins) {
    // TODO fill with life
  }
  
  
  void DxbcCompiler::dclConstantBuffer(const DxbcInstruction& ins) {
    // dclConstantBuffer takes two operands:
    // 1. The buffer register ID
    // 2. The number of 4x32-bit constants
    auto op = ins.operand(0);
    
    if (op.token().indexDimension() != 2)
      throw DxvkError("DxbcCompiler::dclConstantBuffer: Invalid index dimension");
    
    const uint32_t index = op.index(0).immPart();
    const uint32_t size  = op.index(1).immPart();
    
    m_gen->dclConstantBuffer(index, size);
  }
  
  
  void DxbcCompiler::dclResource(const DxbcInstruction& ins) {
    // dclResource takes two operands:
    // 1. The resource register ID
    // 2. The resource return type
    auto op = ins.operand(0);
    
    if (op.token().indexDimension() != 1)
      throw DxvkError("DXBC: dclResource: Invalid index dimension");
    
    const uint32_t index = op.index(0).immPart();
    
    // Defines the type of the resource (texture2D, ...)
    auto resourceDim = static_cast<DxbcResourceDim>(
      bit::extract(ins.token().control(), 0, 4));
    
    // Defines the type of a read operation. DXBC has the ability
    // to define four different types whereas SPIR-V only allows
    // one, but in practice this should not be much of a problem.
    const uint32_t ofs = op.length();
    
    auto xType = static_cast<DxbcResourceReturnType>(
      bit::extract(ins.arg(ofs), 0, 3));
    auto yType = static_cast<DxbcResourceReturnType>(
      bit::extract(ins.arg(ofs), 4, 7));
    auto zType = static_cast<DxbcResourceReturnType>(
      bit::extract(ins.arg(ofs), 8, 11));
    auto wType = static_cast<DxbcResourceReturnType>(
      bit::extract(ins.arg(ofs), 12, 15));
    
    if ((xType != yType) || (xType != zType) || (xType != wType))
      Logger::warn("DXBC: dclResource: Ignoring resource return types");
    
    m_gen->dclResource(index, resourceDim, xType);
  }
  
  
  void DxbcCompiler::dclSampler(const DxbcInstruction& ins) {
    // dclSampler takes one operand:
    // 1. The sampler register ID
    // TODO implement sampler mode (default / comparison / mono)
    auto op = ins.operand(0);
    
    if (op.token().indexDimension() != 1)
      throw DxvkError("DxbcCompiler::dclSampler: Invalid index dimension");
    
    const uint32_t index = op.index(0).immPart();
    m_gen->dclSampler(index);
  }
  
  
  void DxbcCompiler::dclInterfaceVar(const DxbcInstruction& ins) {
    auto op = ins.operand(0);
    auto opcode = ins.token().opcode();
    
    switch (op.token().type()) {
      case DxbcOperandType::Input:
      case DxbcOperandType::Output: {
        uint32_t regId  = 0;
        uint32_t regDim = 0;
        
        if (op.token().indexDimension() == 1) {
          regId  = op.index(0).immPart();
        } else if (op.token().indexDimension() == 2) {
          regDim = op.index(0).immPart();
          regId  = op.index(1).immPart();
        } else {
          throw DxvkError(str::format(
            "DxbcCompiler::dclInterfaceVar: Invalid index dimension: ",
            op.token().indexDimension()));
        }
        
        const bool hasSv =
            opcode == DxbcOpcode::DclInputSgv
         || opcode == DxbcOpcode::DclInputSiv
         || opcode == DxbcOpcode::DclInputPsSgv
         || opcode == DxbcOpcode::DclInputPsSiv
         || opcode == DxbcOpcode::DclOutputSgv
         || opcode == DxbcOpcode::DclOutputSiv;
        
        DxbcSystemValue sv = DxbcSystemValue::None;
        
        if (hasSv)
          sv = ins.readEnum<DxbcSystemValue>(op.length());
        
        const bool hasInterpolationMode =
            opcode == DxbcOpcode::DclInputPs
         || opcode == DxbcOpcode::DclInputPsSiv;
        
        DxbcInterpolationMode im = DxbcInterpolationMode::Undefined;
        
        if (hasInterpolationMode) {
          im = static_cast<DxbcInterpolationMode>(
            bit::extract(ins.token().control(), 0, 3));
        }
        
        m_gen->dclInterfaceVar(
          op.token().type(), regId, regDim,
          op.token().componentMask(), sv, im);
      } break;
      
      default:
        throw DxvkError(str::format(
          "DxbcCompiler::dclInterfaceVar: Unhandled operand type: ",
          op.token().type()));
    }
  }
  
  
  void DxbcCompiler::dclTemps(const DxbcInstruction& ins) {
    // dclTemps takes one operand:
    // 1. The number of temporary registers to
    //    declare, as an immediate 32-bit integer.
    m_gen->dclTemps(ins.arg(0));
  }
  
  
  void DxbcCompiler::opAdd(const DxbcInstruction& ins) {
    auto dstOp  = ins.operand(0);
    auto srcOp1 = ins.operand(dstOp.length());
    auto srcOp2 = ins.operand(dstOp.length() + srcOp1.length());
    DxbcComponentMask mask = this->getDstOperandMask(dstOp);
    
    DxbcValue src1 = this->loadOperand(srcOp1, mask, DxbcScalarType::Float32);
    DxbcValue src2 = this->loadOperand(srcOp2, mask, DxbcScalarType::Float32);
    DxbcValue val  = m_gen->opAdd(src1, src2);
              val  = this->applyResultModifiers(val, ins.token().control());
    this->storeOperand(dstOp, val, mask);
  }
  
  
  void DxbcCompiler::opMad(const DxbcInstruction& ins) {
    auto dstOp  = ins.operand(0);
    auto srcOp1 = ins.operand(dstOp.length());
    auto srcOp2 = ins.operand(dstOp.length() + srcOp1.length());
    auto srcOp3 = ins.operand(dstOp.length() + srcOp1.length() + srcOp2.length());
    DxbcComponentMask mask = this->getDstOperandMask(dstOp);
    
    DxbcValue src1 = this->loadOperand(srcOp1, mask, DxbcScalarType::Float32);
    DxbcValue src2 = this->loadOperand(srcOp2, mask, DxbcScalarType::Float32);
    DxbcValue src3 = this->loadOperand(srcOp3, mask, DxbcScalarType::Float32);
    // TODO implement with native FMA instruction
    DxbcValue val  = m_gen->opMul(src1, src2);
              val  = m_gen->opAdd(val,  src3);
              val  = this->applyResultModifiers(val, ins.token().control());
    this->storeOperand(dstOp, val, mask);
  }
  
  
  void DxbcCompiler::opMul(const DxbcInstruction& ins) {
    auto dstOp  = ins.operand(0);
    auto srcOp1 = ins.operand(dstOp.length());
    auto srcOp2 = ins.operand(dstOp.length() + srcOp1.length());
    DxbcComponentMask mask = this->getDstOperandMask(dstOp);
    
    DxbcValue src1 = this->loadOperand(srcOp1, mask, DxbcScalarType::Float32);
    DxbcValue src2 = this->loadOperand(srcOp2, mask, DxbcScalarType::Float32);
    DxbcValue val  = m_gen->opMul(src1, src2);
              val  = this->applyResultModifiers(val, ins.token().control());
    this->storeOperand(dstOp, val, mask);
  }
  
  
  void DxbcCompiler::opDpx(const DxbcInstruction& ins, uint32_t n) {
    auto dstOp  = ins.operand(0);
    auto srcOp1 = ins.operand(dstOp.length());
    auto srcOp2 = ins.operand(dstOp.length() + srcOp1.length());
    
    DxbcComponentMask dstMask = this->getDstOperandMask(dstOp);
    DxbcComponentMask srcMask(n >= 1, n >= 2, n >= 3, n == 4);
    
    DxbcValue src1 = this->loadOperand(srcOp1, srcMask, DxbcScalarType::Float32);
    DxbcValue src2 = this->loadOperand(srcOp2, srcMask, DxbcScalarType::Float32);
    DxbcValue val  = m_gen->opDot(src1, src2);
              val  = this->applyResultModifiers(val, ins.token().control());
    this->storeOperand(dstOp, val, dstMask);
  }
  
  
  void DxbcCompiler::opRsq(const DxbcInstruction& ins) {
    auto dstOp = ins.operand(0);
    auto srcOp = ins.operand(dstOp.length());
    DxbcComponentMask mask = this->getDstOperandMask(dstOp);
    
    DxbcValue src = this->loadOperand(srcOp, mask, DxbcScalarType::Float32);
    DxbcValue val = m_gen->opRsqrt(src);
              val = this->applyResultModifiers(val, ins.token().control());
    this->storeOperand(dstOp, val, mask);
  }
  
  void DxbcCompiler::opMov(const DxbcInstruction& ins) {
    auto dstOp = ins.operand(0);
    auto srcOp = ins.operand(dstOp.length());
    DxbcComponentMask mask = this->getDstOperandMask(dstOp);
    
    DxbcValue value = this->loadOperand(srcOp, mask, DxbcScalarType::Float32);
              value = this->applyResultModifiers(value, ins.token().control());
    this->storeOperand(dstOp, value, mask);
  }
  
  
  void DxbcCompiler::opRet(const DxbcInstruction& ins) {
    m_gen->fnReturn();
  }
  
  
  void DxbcCompiler::opSample(const DxbcInstruction& ins) {
    // TODO support address offset
    // TODO support more sample ops
    auto dstOp   = ins.operand(0);
    auto coordOp = ins.operand(dstOp.length());
    auto texture = ins.operand(dstOp.length() + coordOp.length());
    auto sampler = ins.operand(dstOp.length() + coordOp.length() + texture.length());
    
    if ((texture.token().indexDimension() != 1)
     || (sampler.token().indexDimension() != 1))
      throw DxvkError("DXBC: opSample: Invalid operand index dimensions");
    
    uint32_t textureId = texture.index(0).immPart();
    uint32_t samplerId = sampler.index(0).immPart();
    
    DxbcValue coord = this->loadOperand(coordOp,
      DxbcComponentMask(true, true, true, true),
      DxbcScalarType::Float32);
    
    DxbcComponentMask mask = this->getDstOperandMask(dstOp);
    
    DxbcValue value = m_gen->texSample(textureId, samplerId, coord);
              value = this->selectOperandComponents(texture.token(), value, mask);
    this->storeOperand(dstOp, value, mask);
  }
  
  
  DxbcValue DxbcCompiler::getDynamicIndexValue(const DxbcOperandIndex& index) {
    DxbcValue immPart;
    DxbcValue relPart;
    
    if (index.hasImmPart())
      immPart = m_gen->defConstScalar(index.immPart());
    
    if (index.hasRelPart()) {
      relPart = this->loadOperand(index.relPart(),
        DxbcComponentMask(true, false, false, false),
        DxbcScalarType::Uint32);
    }
    
    if (immPart.valueId == 0)
      return relPart;
    else if (relPart.valueId == 0)
      return immPart;
    else
      return m_gen->opAdd(relPart, immPart);
  }
  
  
  DxbcComponentMask DxbcCompiler::getDstOperandMask(const DxbcOperand& operand) {
    const DxbcOperandToken token = operand.token();
    
    if (token.numComponents() == 1) {
      return DxbcComponentMask(true, false, false, false);
    } else if (token.numComponents() == 4) {
      switch (token.selectionMode()) {
        case DxbcComponentSelectionMode::Mask:
          return token.componentMask();
        
        case DxbcComponentSelectionMode::Select1:
          return token.componentSelection();
        
        default:
          throw DxvkError(str::format(
            "DxbcCompiler::getDstOperandMask: Invalid component selection mode: ",
            token.selectionMode()));
      }
    } else {
      throw DxvkError(str::format(
        "DxbcCompiler::getDstOperandMask: Invalid component count: ",
        token.numComponents()));
    }
  }
  
  
  DxbcPointer DxbcCompiler::getTempOperandPtr(const DxbcOperand& operand) {
    if (operand.token().indexDimension() != 1) {
      throw DxvkError(str::format(
        "DxbcCompiler::getTempOperandPtr: Invalid index dimension: ",
        operand.token().indexDimension()));
    }
    
    if (operand.token().indexRepresentation(0) != DxbcOperandIndexRepresentation::Imm32) {
      throw DxvkError(str::format(
        "DxbcCompiler::getTempOperandPtr: Invalid index representation: ",
        operand.token().indexRepresentation(0)));
    }
    
    return m_gen->ptrTempReg(operand.index(0).immPart());
  }
  
  
  DxbcPointer DxbcCompiler::getInterfaceOperandPtr(const DxbcOperand& operand) {
    const uint32_t indexDim = operand.token().indexDimension();
    
    // Vertex index ID is unused if the index dimension
    // is 1. The element index is always the last index.
//     const uint32_t vIndexId = 0;
    const uint32_t eIndexId = indexDim - 1;
    
    if (operand.token().indexRepresentation(eIndexId) != DxbcOperandIndexRepresentation::Imm32) {
      throw DxvkError(str::format(
        "DxbcCompiler::getInterfaceOperandPtr: Invalid element index representation: ",
        operand.token().indexRepresentation(eIndexId)));
    }
    
    if (indexDim == 1) {
      return m_gen->ptrInterfaceVar(
        operand.token().type(),
        operand.index(eIndexId).immPart());
    } else {
      // TODO implement index dimension 2
      throw DxvkError(str::format(
        "DxbcCompiler::getInterfaceOperandPtr: Invalid index dimension: ",
        indexDim));
    }
  }
  
  
  DxbcPointer DxbcCompiler::getConstantBufferPtr(const DxbcOperand& operand) {
    if (operand.token().indexDimension() != 2)
      throw DxvkError("DxbcCompiler::getConstantBufferPtr: Invalid index dimension");
    
    return m_gen->ptrConstantBuffer(
      operand.index(0).immPart(),
      this->getDynamicIndexValue(operand.index(1)));
  }
  
  
  DxbcPointer DxbcCompiler::getOperandPtr(const DxbcOperand& operand) {
    switch (operand.token().type()) {
      case DxbcOperandType::Temp:
        return this->getTempOperandPtr(operand);
      
      case DxbcOperandType::Input:
      case DxbcOperandType::Output:
        return this->getInterfaceOperandPtr(operand);
      
      case DxbcOperandType::ConstantBuffer:
        return this->getConstantBufferPtr(operand);
      
      default:
        throw DxvkError(str::format(
          "DxbcCompiler::getOperandPtr: Unhandled operand type: ",
          operand.token().type()));
    }
  }
  
  
  DxbcValue DxbcCompiler::selectOperandComponents(
    const DxbcOperandToken& opToken,
    const DxbcValue&        opValue,
          DxbcComponentMask dstMask) {
    // Four-component source operands can provide either a
    // swizzle to select multiple components, or a component
    // index that is used to select one single component.
    switch (opToken.selectionMode()) {
      case DxbcComponentSelectionMode::Swizzle:
        return m_gen->regSwizzle(opValue,
          opToken.componentSwizzle(), dstMask);
        
      case DxbcComponentSelectionMode::Select1:
        return m_gen->regExtract(opValue,
          opToken.componentSelection());
        
      case DxbcComponentSelectionMode::Mask:
        return m_gen->regExtract(opValue,
          opToken.componentMask());
      
      default:
        throw DxvkError(str::format(
          "DxbcCompiler::selectOperandComponents: Invalid selection mode: ",
          opToken.selectionMode()));
    }
  }
  
  
  DxbcValue DxbcCompiler::applyOperandModifiers(
          DxbcValue             value,
          DxbcOperandModifiers  modifiers) {
    if (modifiers.test(DxbcOperandModifier::Abs))
      value = m_gen->opAbs(value);
    
    if (modifiers.test(DxbcOperandModifier::Neg))
      value = m_gen->opNeg(value);
    return value;
  }
  
  
  DxbcValue DxbcCompiler::applyResultModifiers(
          DxbcValue             value,
          DxbcOpcodeControl     control) {
    if (control.saturateBit())
      value = m_gen->opSaturate(value);
    return value;
  }
  
  
  DxbcValue DxbcCompiler::loadOperand(
    const DxbcOperand&      operand,
          DxbcComponentMask dstMask,
          DxbcScalarType    dstType) {
    const DxbcOperandToken token = operand.token();
    
    DxbcValue result;
    
    if (token.type() == DxbcOperandType::Imm32) {
      if (token.numComponents() == 1) {
        result = m_gen->defConstScalar(operand.imm32(0));
      } else if (token.numComponents() == 4) {
        result = m_gen->defConstVector(
          operand.imm32(0), operand.imm32(1),
          operand.imm32(2), operand.imm32(3));
        result = m_gen->regExtract(result, dstMask);
      } else {
        throw DxvkError(str::format(
          "DxbcCompiler::loadOperand [imm32]: Invalid number of components: ",
          token.numComponents()));
      }
      
      result = m_gen->regCast(result, DxbcValueType(
        dstType, result.type.componentCount));
    } else {
      result = m_gen->regLoad(this->getOperandPtr(operand));
      
      // Cast register to requested type
      result = m_gen->regCast(result, DxbcValueType(
        dstType, result.type.componentCount));
      
      // Apply the source operand swizzle
      if (token.numComponents() == 4)
        result = this->selectOperandComponents(token, result, dstMask);
      
      // Apply source operand modifiers, if any
      DxbcOperandTokenExt token;
      
      if (operand.queryOperandExt(DxbcOperandExt::OperandModifier, token))
        result = this->applyOperandModifiers(result, token.data());
    }
    
    return result;
  }
  
  
  void DxbcCompiler::storeOperand(
    const DxbcOperand&      operand,
          DxbcValue         value,
          DxbcComponentMask mask) {
    const DxbcPointer ptr = this->getOperandPtr(operand);
    
    // The value to store is actually allowed to be scalar,
    // so we might need to create a vector from it.
    if (value.type.componentCount == 1)
      value = m_gen->regVector(value, mask.componentCount());
    
    // Cast source value to destination register type.
    // TODO verify that this actually works as intended.
    DxbcValueType dstType;
    dstType.componentType  = ptr.type.valueType.componentType;
    dstType.componentCount = value.type.componentCount;
    value = m_gen->regCast(value, dstType);
    
    m_gen->regStore(ptr, value, mask);
  }
  
}