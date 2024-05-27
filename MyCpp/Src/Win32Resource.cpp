#include "MyCpp/Win32Resource.hpp"
#include "MyCpp/Win32System.hpp"
#include "MyCpp/IntCast.hpp"

#pragma comment( lib, "gdi32.lib" )
#pragma comment( lib, "user32.lib" )

namespace mycpp
{
	std::pair< void*, std::size_t > GetRawResourceData( handle_t module_handle, const char_t* type, const char_t* name, word language )
	{
		HMODULE hm = reinterpret_cast< HMODULE >( module_handle );
		HRSRC resourceInfo = ::FindResourceEx( hm, type, name, language );

		if ( resourceInfo != null )
		{
			dword size = ::SizeofResource( hm, resourceInfo );
			HGLOBAL resouceData = ::LoadResource( hm, resourceInfo );
			if ( resouceData != null )
			{
				void* ptr = ::LockResource( resouceData );
				if ( ptr != null )
					return std::make_pair( ptr, size );
			}
		}

		return null;
	}

	string_t GetStringResource( handle_t instance_handle, uint id )
	{
		HINSTANCE hi = reinterpret_cast< HMODULE >( instance_handle );
		vchar_t buffer( 1024 );

		adaptive_load( buffer, buffer.size(),
			[&hi, &id] (char_t* ptr, std::size_t n)
		{
			return LoadString( hi, id, ptr, numeric_cast< int >( n ) ) + 1;
		} );

		return buffer.data();
	}
}
