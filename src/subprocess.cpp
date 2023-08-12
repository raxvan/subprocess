
#include "../include/subprocess.h"


#ifdef SUBPROCESS_ENABLE_ASSERT_IMPL
#	include <iostream>
#	include <cassert>
#endif

#ifdef _WIN32
#	include "subprocess-win32-impl.h"
#endif

#ifdef __GNUC__
#	include "subprocess-posix-impl.h"
#endif

#include "subprocess-common-impl.h"

namespace splib
{

#ifdef SUBPROCESS_ENABLE_ASSERT_IMPL
	void subprocess_assert_failed(const char* file, const int line, const char* cond)
	{
		std::cerr << "SUBPROCESS_ASSERT failed in " << file << "(" << line << ")> " << cond << std::endl;
		assert(false);
	}
#endif

	bool subprocess::CreateData::make_cmd(const std::string_view& cmdline)
	{
		exe.clear();
		argv.clear();
		exe = "cmd";
		argv.push_back("/C");

		std::string arg;
		bool		inQuote = false;
		bool		escape = false;

		for (char ch : cmdline)
		{
			if (escape)
			{
				arg += ch;
				escape = false;
			}
			else if (ch == '^')
			{
				escape = true;
			}
			else if (ch == '\"')
			{
				inQuote = !inQuote;
			}
			else if (ch == ' ' && !inQuote)
			{
				if (!arg.empty())
				{
					argv.push_back(arg);
					arg.clear();
				}
			}
			else
			{
				arg += ch;
			}
		}

		if (!arg.empty())
			argv.push_back(arg);

		if (inQuote)
			return false;

		return true;
	}

	bool subprocess::CreateData::make_ps(const std::string_view& cmdline)
	{
		exe = "powershell";

		argv.clear();
		argv.push_back("-Command");
		argv.push_back(std::string(cmdline));
		return true;
	}

	bool subprocess::CreateData::make_shell(const std::string_view& cmdline)
	{
		exe = "/bin/sh";

		argv.clear();
		argv.push_back("sh");
		argv.push_back("-c");
		argv.push_back(std::string(cmdline));
		return true;
	}

	subprocess::subprocess() noexcept
	{
	}

	subprocess::~subprocess() noexcept
	{
		SUBPROCESS_ASSERT(joinable() == false);
	}
	bool subprocess::joinable() noexcept
	{
		std::lock_guard<std::mutex> lock(m_process_mutex);
		return m_process_handle != nullptr;
	}
	subprocess::subprocess(subprocess&& other) noexcept
	{
		std::lock_guard<std::mutex> lock(other.m_process_mutex);
		other.swap_no_lock(*this);
	}
	subprocess& subprocess::operator=(subprocess&& other) noexcept
	{
		subprocess tmp;
		{
			std::lock_guard<std::mutex> lock(other.m_process_mutex);
			other.swap_no_lock(tmp);
		}
		{
			std::lock_guard<std::mutex> lock(m_process_mutex);
			tmp.swap_no_lock(*this);
		}
		return (*this);
	}

	bool subprocess::stdin_write(const std::string& data) noexcept
	{
		return stdin_write(data.c_str(), data.size());
	}

	void subprocess::stdin_close() noexcept
	{
		std::lock_guard<std::mutex> lock(m_process_mutex);
		m_stdin_pipe.reset();
	}

	void subprocess::reset_no_lock() noexcept
	{
		m_stdin_pipe.reset();
		m_process_handle.reset();
	}

	void subprocess::swap(subprocess& other) noexcept
	{
		subprocess tmp;
		{
			std::lock_guard<std::mutex> lock(m_process_mutex);
			tmp.swap_no_lock(*this);
		}
		{
			std::lock_guard<std::mutex> lock(other.m_process_mutex);
			other.swap_no_lock(tmp);
		}
	}
	void subprocess::swap_no_lock(subprocess& other) noexcept
	{
		m_process_handle.swap(other.m_process_handle);
		m_stdin_pipe.swap(other.m_stdin_pipe);
	}

}
