#include <VMGadgets.hpp>
#include <Translation.hpp>
#include <Parameter.hpp>

void VMGadgets::VMTimingTrap(MappedMemory& mapped_memory)
{
    // Load the imm 
    Translation::Ldi(0x000000007FFE0008, mapped_memory);

    // Push the value pointed at 0x000000007FFE0008 to the virtual stack
    Translation::Ldm(mapped_memory);

    // Load the imm 
    Translation::Ldi(0x000000007FFE0008, mapped_memory);

    // Push the value pointed at 0x000000007FFE0008 to the virtual stack
    Translation::Ldm(mapped_memory);

    // Sub both of them
    const auto inst = Virtual::Instruction(Virtual::Parameter(Virtual::Parameter::kNone), Virtual::Command::kVSub);
    mapped_memory.Write<Virtual::InstructionLength>(inst.AssembleInstruction());
}