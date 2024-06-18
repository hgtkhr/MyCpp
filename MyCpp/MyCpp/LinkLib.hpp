#pragma once

#ifndef __MYCPP_LINKLIB_HPP__
#define __MYCPP_LINKLIB_HPP__

#ifndef MYCPP_NOAUTOLINKLIB

#define MYCPP_BASE_NAME_ "MyCpp"

#if defined( UNICODE ) || defined( _UNICODE )
#define MYCPP_CHARTYPE_ "_W"
#else
#define MYCPP_CHARTYPE_ "_A"
#endif

#if defined( _DLL )
#define MYCPP_LIBTYPE_ "_SD"
#else
#define MYCPP_LIBTYPE_ "_S"
#endif

#if defined( _DEBUG )
#define MYCPP_DEBUG_ "_D"
#else
#define MYCPP_DEBUG_ ""
#endif

#pragma comment( lib, MYCPP_BASE_NAME_ MYCPP_CHARTYPE_ MYCPP_LIBTYPE_ MYCPP_DEBUG_ ".lib" )

#undef MYCPP_BASE_NAME_
#undef MYCPP_CHARTYPE_
#undef MYCPP_LIBTYPE_
#undef MYCPP_DEBUG_

#endif

#endif // ! __MYCPP_LINKLIB_HPP__
