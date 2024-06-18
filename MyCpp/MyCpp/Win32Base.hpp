#pragma once

#ifndef __MYCPP_WIN32BASE_HPP__
#define __MYCPP_WIN32BASE_HPP__

#include "MyCpp/Base.hpp"
#if !defined( NOMINMAX )
#define NOMINMAX
#endif
#if !defined( WIN32_LEAN_AND_MEAN )
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace MyCpp
{
	typedef INT_PTR int_t;
	typedef UINT_PTR uint_t;
	typedef LONG_PTR long_t;
	typedef ULONG_PTR ulong_t;
	typedef DWORD_PTR dword_t;
}

#if defined( MYCPP_GLOBALTYPEDES )
using MyCpp::int_t;
using MyCpp::uint_t;
using MyCpp::long_t;
using MyCpp::ulong_t;
#endif

#endif // ! __MYCPP_WIN32BASE_HPP__
