#include <Virtual.hpp>

Virtual::InstructionLength Virtual::Instruction::AssembleInstruction() const
{
    return (m_parameter.AssembleParameter() << 16) | static_cast<CommandWidth>(m_command);
}