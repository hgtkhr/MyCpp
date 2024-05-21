#pragma once
#include <utility>
#include "MyCpp/Win32SafeHandle.hpp"

namespace MyCpp
{
	// Define HCURSOR as an independent type since HCURSOR is a typedef of HICON and type ambiguity resolution is not possible.
	// Design hcursor_wrapper to behave as a HCURSOR.
	class hcursor_wrapper
	{
	public:
		hcursor_wrapper( HCURSOR hCursor )
			: m_hcursor( hCursor )
		{}

		hcursor_wrapper() = default;
		hcursor_wrapper( const hcursor_wrapper& other ) = default;
		hcursor_wrapper( hcursor_wrapper&& other ) noexcept = default;

		operator HCURSOR () const
		{
			return m_hcursor;
		}

		HCURSOR get_handle() const
		{
			return m_hcursor;
		}

		HCURSOR* operator & ()
		{
			return &m_hcursor;
		}

		const HCURSOR* operator & () const
		{
			return &m_hcursor;
		}

		hcursor_wrapper& operator = ( HCURSOR other )
		{
			m_hcursor = other;
		}

		hcursor_wrapper& operator = ( const hcursor_wrapper& other ) = default;
		hcursor_wrapper& operator = ( hcursor_wrapper&& other ) noexcept = default;
	private:
		HCURSOR m_hcursor = null;
	};

	// The return value of LoadCursor() or LoadImage() should be HCURSOR_T.
	// Otherwise, smart-pointer's delteter will not work correctly.
	typedef hcursor_wrapper HCURSOR_T;

	template <>
	struct safe_handle_closer< HBITMAP >
	{
		typedef HBITMAP pointer;

		void operator () ( HBITMAP h )
		{
			if ( h != null )
				::DeleteObject( reinterpret_cast< HGDIOBJ >( h ) );
		}
	};

	template <>
	struct safe_handle_closer< HACCEL >
	{
		typedef HACCEL pointer;

		void operator () ( HACCEL h )
		{
			if ( h != null )
				::DestroyAcceleratorTable( h );
		}
	};

	template <>
	struct safe_handle_closer< HCURSOR_T >
	{
		typedef HCURSOR_T pointer;

		void operator () ( HCURSOR_T h )
		{
			if ( h != null )
				::DestroyCursor( h );
		}
	};

	template <>
	struct safe_handle_closer< HICON >
	{
		typedef HICON pointer;

		void operator () ( HICON h )
		{
			if ( h != null )
				::DestroyIcon( h );
		}
	};

	template <>
	struct safe_handle_closer< HMENU >
	{
		typedef HMENU pointer;

		void operator () ( HMENU h )
		{
			if ( h != null )
				::DestroyMenu( h );
		}
	};

	inline constexpr char_t* resource_id( int id )
	{
		return MAKEINTRESOURCE( id );
	}

	inline constexpr word lang_id( word id_primary, word id_secondary )
	{
		return MAKELANGID( id_primary, id_secondary );
	}

	std::pair< void*, std::size_t > GetRawResourceData( HMODULE module, const char_t* type, const char_t* name, word language = lang_id( LANG_NEUTRAL, SUBLANG_DEFAULT ) );
	string_t GetStringResource( HINSTANCE instance, uint id );
}

#if defined( MYCPP_GLOBALTYPEDES )
using MyCpp::HCURSOR_T;
#endif

#undef LoadCursor

inline HCURSOR_T LoadCursor( HINSTANCE hInstance, LPCSTR lpCursorName )
{
	return ::LoadCursorA( hInstance, lpCursorName );
}

inline HCURSOR_T LoadCursor( HINSTANCE hInstance, LPCWSTR lpCursorName )
{
	return ::LoadCursorW( hInstance, lpCursorName );
}

