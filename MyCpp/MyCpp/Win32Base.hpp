#pragma once
#include "MyCpp/Base.hpp"
#if !defined( NOMINMAX )
#define NOMINMAX
#endif
#if !defined( WIN32_LEAN_AND_MEAN )
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace mycpp
{
	typedef INT_PTR int_t;
	typedef UINT_PTR uint_t;
	typedef LONG_PTR long_t;
	typedef ULONG_PTR ulong_t;
}

#if defined( MYCPP_GLOBALTYPEDES )
using mycpp::int_t;
using mycpp::uint_t;
using mycpp::long_t;
using mycpp::ulong_t;
#endif
