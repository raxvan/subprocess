#pragma once

#define SUBPROCESS_ENABLE_ASSERT
#define SUBPROCESS_ENABLE_ASSERT_IMPL /*define for builtin default assert handler; otherwise you need to implement `subprocess_assert_failed`*/

#include <string>
#include <functional>
#include <mutex>
#include <memory>

#if defined(SUBPROCESS_TESTING)

#	include <ttf.h>
#	define SUBPROCESS_ASSERT TTF_ASSERT

#elif defined(SUBPROCESS_WITH_DEVPLATFORM)

#	include <devtiny.h>
#	define SUBPROCESS_ASSERT DEV_ASSERT

#elif defined(SUBPROCESS_ENABLE_ASSERT)

namespace splib
{
	extern "C++" void subprocess_assert_failed(const char* file, const int line, const char* cond);
}

#	define SUBPROCESS_ASSERT(_COND)                                         \
		do                                                                   \
		{                                                                    \
			if (!(_COND))                                                    \
				splib::subprocess_assert_failed(__FILE__, __LINE__, #_COND); \
		} while (false)

#endif
//------------------------

#ifndef SUBPROCESS_ASSERT

#	define SUBPROCESS_ASSERT(...) \
		do                         \
		{                          \
		} while (false)

#endif

namespace splib
{

	class suprocess_impl;
	class pipe_impl;

}
