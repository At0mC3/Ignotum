#include <Parameter.hpp>

Virtual::Parameter::ParameterWidth Virtual::Parameter::AssembleParameter() const
{
    return m_is_raw ? m_raw_parameter : static_cast<ParameterWidth>(m_parameter);
}