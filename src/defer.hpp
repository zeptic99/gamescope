#pragma once

namespace defer_detail
{
    template <typename Func>
    class DeferHelper
    {
    public:
        DeferHelper(Func func)
            : m_func(func)
        {
        }
        ~DeferHelper()
        {
            m_func();
        }

    private:
        Func m_func;
    };

    template <typename Func>
    DeferHelper<Func> CreateDeferHelper(Func func)
    {
        return DeferHelper<Func>(func);
    }
}

#define DEFER_1(x, y) x##y
#define DEFER_2(x, y) DEFER_1(x, y)
#define DEFER_3(x)    DEFER_2(x, __COUNTER__)
#define defer(code)   auto DEFER_3(_defer_) = defer_detail::CreateDeferHelper( [&](){ code; } )
