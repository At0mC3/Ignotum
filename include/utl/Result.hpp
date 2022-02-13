
#ifndef IGNOTUM_RESULT_HPP
#define IGNOTUM_RESULT_HPP

#include <cstdio>
#include <exception>

namespace utl
{
    template<typename ResultType, typename ErrorType>
    class Result
    {
    private:
        union{
            ResultType m_result;
            ErrorType m_error;
        };
        bool m_is_error;
    public:
        explicit Result<ResultType, ErrorType>(ResultType result) : m_result(result), m_is_error(false) {}
        explicit Result<ResultType, ErrorType>(ErrorType result) : m_error(result), m_is_error(true) {}

        explicit Result<ResultType, ErrorType>(ResultType&& result) : m_result(std::move(result)), m_is_error(false) {}
        explicit Result<ResultType, ErrorType>(ErrorType&& result) : m_error(std::move(result)), m_is_error(true) {}
    public:
        ~Result() = default;

        [[maybe_unused]] [[nodiscard]] bool IsOK() const { return !m_is_error; }

        [[maybe_unused]] [[nodiscard]] bool IsErr() const { return m_is_error; }

        ResultType Expect(const char* msg) const
        {
            if(m_is_error)
            {
                std::puts(msg);
                std::terminate();
            }

            return m_result;
        }

        [[maybe_unused]] [[nodiscard]] ErrorType GetError() const { return m_error; }
    };

    template<typename ResultType, typename ErrorType>
    Result<ResultType, ErrorType> Ok(ResultType result)
    {
        return Result<ResultType, ErrorType>(result);
    }

    template<typename ResultType, typename ErrorType>
    Result<ResultType, ErrorType> Err(ErrorType result)
    {
        return Result<ResultType, ErrorType>(result);
    }
}

#endif //IGNOTUM_RESULT_HPP
