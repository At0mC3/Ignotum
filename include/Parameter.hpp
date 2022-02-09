
#ifndef IGNOTUM_PARAMETER_HPP
#define IGNOTUM_PARAMETER_HPP

#include <cstdint>

namespace Virtual
{
    class Parameter
    {
    private:
        typedef std::uint16_t ParameterWidth;
    public:
        enum Definition : ParameterWidth
        {
            kNone = 0,
        };
    private:
        Parameter::Definition m_parameter{kNone};
        ParameterWidth m_raw_parameter{0};
        bool m_is_raw{false};
    public:
        explicit Parameter(Parameter::Definition parameter) : m_parameter(parameter) { }
        explicit Parameter(ParameterWidth parameter) : m_raw_parameter(parameter), m_is_raw(true) { }
        [[nodiscard]] ParameterWidth AssembleParameter() const;
    };
}

#endif //IGNOTUM_PARAMETER_HPP
