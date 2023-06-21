#pragma once

#include "subprocess_config.h"

namespace splib
{

	class subprocess
	{
	public:
		struct CreateData
		{
			std::string				 cwd;
			std::string				 exe;
			std::vector<std::string> argv;

			bool make_shell(const std::string_view& cmdline);
			bool make_cmd(const std::string_view& cmdline);
			bool make_ps(const std::string_view& cmdline);

			std::size_t buffer_size = 131072;
			//^ buffer size for stdout/stderr pipes
		};

		using stdfunc_t = std::function<void(const char*, std::size_t)>;

	public:
		subprocess() noexcept;
		~subprocess() noexcept;

		subprocess(subprocess&& other) noexcept;
		subprocess& operator=(subprocess&& other) noexcept;

		subprocess(const subprocess&) = delete;
		subprocess& operator=(const subprocess&) = delete;

	public:
		bool start(const CreateData& cd, stdfunc_t stdout_func, stdfunc_t stderr_func) noexcept;
		//^ start process and return true if successful. stdout/stderr functions are called from a separate thread

		bool joinable() noexcept;
		//^ returns true if process is started and not joined

		int join() noexcept;
		//^wait for process to finish and return exit code. stdout/stderr functions are cleared

		void stdin_close() noexcept;
		//^ close stdin pipe. stdin_write will return false after calling this

		bool stdin_write(const std::string& data) noexcept;
		bool stdin_write(const char* bytes, size_t n) noexcept;
		//^ write returns false after calling stdin_close or if process is not started

		void kill() noexcept;
		//^ kill process if started, does nothing otherwise. stdout/stderr functions are cleared

		void swap(subprocess& other) noexcept;

	protected:
		void swap_no_lock(subprocess& other) noexcept;
		void reset_no_lock() noexcept;

	protected:
		std::mutex m_process_mutex;

		std::unique_ptr<suprocess_impl> m_process_handle;
		std::unique_ptr<pipe_impl>		m_stdin_pipe;
	};

}
