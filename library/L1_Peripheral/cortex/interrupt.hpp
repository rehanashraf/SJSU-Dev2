#pragma once

#include <cstddef>
#include <array>

#include "L0_Platform/arm_cortex/m4/core_cm4.h"
#include "L1_Peripheral/interrupt.hpp"
#include "utility/log.hpp"

namespace sjsu
{
namespace cortex
{
class InterruptController final : public sjsu::InterruptController
{
 public:
  using NvicFunction         = void (*)(IRQn_Type);
  using NvicPriorityFunction = void (*)(IRQn_Type, uint32_t);

  struct VectorTable_t
  {
    std::array<IsrPointer, kNumberOfInterrupts> vector;
    void Print()
    {
      for (size_t i = 0; i < vector.size(); i++)
      {
        LOG_INFO("vector[%zu] = %p", i, vector[i]);
      }
    }

    static constexpr VectorTable_t GenerateDefaultTable()
    {
      VectorTable_t result = { 0 };
      // The Arm exceptions may be enabled by default and should simply be
      // called and do nothing.
      for (size_t i = 0; i < kArmIrqOffset; i++)
      {
        result.vector[i] = UnregisteredArmExceptions;
      }
      // For all other exceptions, give a handler that will disable the ISR if
      // it is enabled but has not been registered.
      for (size_t i = kArmIrqOffset; i < result.vector.size(); i++)
      {
        result.vector[i] = UnregisteredInterruptHandler;
      }
      return result;
    }
  };

  static constexpr int IrqToIndex(int irq)
  {
    return irq + kArmIrqOffset;
  }

  static constexpr int IndexToIrq(int index)
  {
    return index - kArmIrqOffset;
  }

  inline static VectorTable_t table = VectorTable_t::GenerateDefaultTable();
  inline static NvicFunction enable               = NVIC_EnableIRQ;
  inline static NvicFunction disable              = NVIC_DisableIRQ;
  inline static NvicPriorityFunction set_priority = NVIC_SetPriority;
  inline static SCB_Type * scb     = SCB;
  inline static int current_vector = cortex::Reset_IRQn;

  static constexpr int32_t kArmIrqOffset      = (-cortex::Reset_IRQn) + 1;
  static constexpr size_t kNumberOfInterrupts = 64;

  static void UnregisteredArmExceptions() {}
  static void UnregisteredInterruptHandler()
  {
    LOG_WARNING(
        "No interrupt service routine found for the vector %d! Disabling ISR",
        current_vector);
    disable(current_vector - kArmIrqOffset);
  }

  static IsrPointer * GetVector(int irq)
  {
    return &table.vector[IrqToIndex(irq)];
  }
  /// Program ends up here if an unexpected interrupt occurs or a specific
  /// handler is not present in the application code.
  static void LookupHandler()
  {
    int active_isr = (scb->ICSR & 0xFF);
    current_vector = active_isr;
    IsrPointer isr = table.vector[active_isr];
    isr();
  }

  void Register(RegistrationInfo_t register_info) const override
  {
    int irq         = register_info.interrupt_request_number;
    *GetVector(irq) = register_info.interrupt_service_routine;
    if (register_info.enable_interrupt && irq >= 0)
    {
      enable(irq);
    }
    if (register_info.priority > -1)
    {
      set_priority(irq, register_info.priority);
    }
  }

  void Deregister(int irq) const override
  {
    disable(irq);
    *GetVector(irq) = UnregisteredInterruptHandler;
  }
};
}  // namespace cortex
}  // namespace sjsu
