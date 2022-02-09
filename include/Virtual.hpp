#ifndef INCLUDE_VIRTUAL_
#define INCLUDE_VIRTUAL_

#include <variant>
#include <cstdint>
#include <Parameter.hpp>

namespace Virtual
{
    [[maybe_unused]] typedef std::uint16_t CommandWidth;
    // The command is in charge of describing what the instruction will do
    enum class Command : CommandWidth
    {
        kLdr = 0,
        kLdm,
        kLdImm,
    };

    enum class RegisterMap : std::uint8_t
    {
        RAX = 0,
        RCX,
        RDX,
        RBX,
    };

    typedef std::uint32_t InstructionLength;

    struct Instruction
    {
        Parameter m_parameter;
        Command m_command;

        Instruction(Parameter parameter, Command command) : m_parameter(parameter), m_command(command) { }
        [[nodiscard]] InstructionLength AssembleInstruction() const;
    };
}

#endif