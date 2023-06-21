#pragma once

#include "subprocess.h"

#include <windows.h>
#include <cstring>
#include <TlHelp32.h>
#include <stdexcept>

namespace splib
{

	namespace detail
	{
		struct safe_handle
		{
			HANDLE handle = INVALID_HANDLE_VALUE;

			inline ~safe_handle() noexcept
			{
				if (handle != INVALID_HANDLE_VALUE)
					CloseHandle(handle);
			}

			safe_handle() = default;
			safe_handle(safe_handle&) = delete;
			safe_handle& operator=(const safe_handle&) = delete;
		};

		class win32_stream_handle : public safe_handle
		{
		public:
			inline win32_stream_handle(subprocess::stdfunc_t&& f) noexcept
				: m_func(std::move(f))
			{
			}

			inline ~win32_stream_handle() noexcept
			{
				if (m_buffer_thread.joinable())
					m_buffer_thread.join();
			}

			void start(const std::size_t buffer_size) noexcept
			{
				SUBPROCESS_ASSERT(m_buffer_thread.joinable() == false);
				m_buffer_thread = std::thread([this, buffer_size]() {
					std::unique_ptr<char[]> buffer(new char[buffer_size]);
					DWORD					sz = 0;
					while (true)
					{
						BOOL ok = ReadFile(handle, static_cast<CHAR*>(buffer.get()), static_cast<DWORD>(buffer_size), &sz, nullptr);
						if (ok == FALSE)
							break;
						if (sz == 0)
							break;
						if (m_func != nullptr)
							m_func(buffer.get(), sz);
					}
				});
			}

		protected:
			subprocess::stdfunc_t m_func;
			std::thread			  m_buffer_thread;
		};

	}

	class suprocess_impl
	{
	public:
		inline suprocess_impl(subprocess::stdfunc_t&& stdout_func, subprocess::stdfunc_t&& stderr_func) noexcept
			: stdout_handle(std::move(stdout_func))
			, stderr_handle(std::move(stderr_func))
		{
		}

		detail::safe_handle			process_handle;
		detail::win32_stream_handle stdout_handle;
		detail::win32_stream_handle stderr_handle;

		DWORD pid = 0;
	};

	class pipe_impl : public detail::safe_handle
	{
	public:
		inline bool write(const char* data, const std::size_t sz) noexcept
		{
			SUBPROCESS_ASSERT(handle != INVALID_HANDLE_VALUE && data != nullptr && sz > 0);

			DWORD outsz;
			BOOL  ok = WriteFile(handle, data, static_cast<DWORD>(sz), &outsz, nullptr);
			if (ok && outsz > 0)
				return true;
			return true;
		}
	};

	bool subprocess::start(const CreateData& cd, stdfunc_t stdout_func, stdfunc_t stderr_func) noexcept
	{
		auto simpl = std::make_unique<suprocess_impl>(std::move(stdout_func), std::move(stderr_func));
		auto pimpl = std::make_unique<pipe_impl>();

		SECURITY_ATTRIBUTES security_attributes;

		security_attributes.nLength = sizeof(SECURITY_ATTRIBUTES);
		security_attributes.bInheritHandle = TRUE;
		security_attributes.lpSecurityDescriptor = nullptr;

		detail::safe_handle stdout_write;
		detail::safe_handle stderr_write;
		detail::safe_handle stdin_read;

		{
			if (CreatePipe(&stdin_read.handle, &(pimpl->handle), &security_attributes, 0) == false)
				return false;
			if (SetHandleInformation(pimpl->handle, HANDLE_FLAG_INHERIT, 0) == false)
				return false;
		}

		{
			if (CreatePipe(&(simpl->stdout_handle.handle), &stdout_write.handle, &security_attributes, 0) == false)
				return false;
			if (SetHandleInformation(simpl->stdout_handle.handle, HANDLE_FLAG_INHERIT, 0) == false)
				return false;
		}

		{
			if (CreatePipe(&(simpl->stderr_handle.handle), &stderr_write.handle, &security_attributes, 0) == false)
				return false;
			if (SetHandleInformation(simpl->stderr_handle.handle, HANDLE_FLAG_INHERIT, 0) == false)
				return false;
		}

		PROCESS_INFORMATION process_info;
		STARTUPINFO			startup_info;

		ZeroMemory(&process_info, sizeof(PROCESS_INFORMATION));
		ZeroMemory(&startup_info, sizeof(STARTUPINFO));

		startup_info.cb = sizeof(STARTUPINFO);
		startup_info.hStdInput = stdin_read.handle;
		startup_info.hStdOutput = stdout_write.handle;
		startup_info.hStdError = stderr_write.handle;

		startup_info.dwFlags |= STARTF_USESTDHANDLES;

		char*		raw_cmd = nullptr;
		const char* raw_cwd = nullptr;

		std::string command_tmp = cd.exe;

		for (const auto& a : cd.argv)
		{
			command_tmp += std::string_view(" ");
			command_tmp += a;
		}

		raw_cmd = &command_tmp[0];

		if (cd.cwd.size() > 0)
			raw_cwd = cd.cwd.c_str();

		SUBPROCESS_ASSERT(command_tmp.size() <= MAX_PATH);

		BOOL ok = CreateProcess(nullptr, raw_cmd, nullptr, nullptr, TRUE, 0,
			nullptr, // env
			raw_cwd, // cwd
			&startup_info, &process_info);

		if (ok == FALSE)
			return false;

		CloseHandle(process_info.hThread);

		simpl->process_handle.handle = process_info.hProcess;
		simpl->pid = process_info.dwProcessId;

		// start buffering threads:
		simpl->stdout_handle.start(cd.buffer_size);
		simpl->stderr_handle.start(cd.buffer_size);

		{
			std::lock_guard<std::mutex> lock(m_process_mutex);
			SUBPROCESS_ASSERT(m_process_handle == nullptr);

			m_process_handle.swap(simpl);
			m_stdin_pipe.swap(pimpl);
		}

		return true;
	}

	int subprocess::join() noexcept
	{
		HANDLE h;
		{
			std::lock_guard<std::mutex> lock(m_process_mutex);
			SUBPROCESS_ASSERT(m_process_handle != nullptr);
			h = m_process_handle->process_handle.handle;
		}
		SUBPROCESS_ASSERT(h != INVALID_HANDLE_VALUE);

		int result = -1;

		{
			WaitForSingleObject(h, INFINITE);

			DWORD rc;
			BOOL  ok = GetExitCodeProcess(h, &rc);
			if (ok == TRUE)
				result = static_cast<int>(rc);
		}

		{
			std::lock_guard<std::mutex> lock(m_process_mutex);
			this->reset_no_lock();
		}

		return result;
	}

	template <class F>
	inline void _iterate_child_processes(const DWORD id, const F& _func)
	{
		SUBPROCESS_ASSERT(id != 0);

		HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
		if (snapshot == INVALID_HANDLE_VALUE)
			return;

		PROCESSENTRY32 process;
		ZeroMemory(&process, sizeof(process));
		process.dwSize = sizeof(process);

		if (Process32First(snapshot, &process) == TRUE)
		{
			do
			{
				if (process.th32ParentProcessID != id)
					continue;

				HANDLE h = OpenProcess(PROCESS_TERMINATE, FALSE, process.th32ProcessID);
				if (h == INVALID_HANDLE_VALUE)
					continue;

				_func(h);
				CloseHandle(h);

			} while (Process32Next(snapshot, &process) == TRUE);
		}

		CloseHandle(snapshot);
	}

	void subprocess::kill() noexcept
	{
		std::lock_guard<std::mutex> lock(m_process_mutex);
		if (m_process_handle == nullptr)
			return;

		auto terminate = [](HANDLE h) {
			SUBPROCESS_ASSERT(h != INVALID_HANDLE_VALUE);
			auto ts = TerminateProcess(h, 2);
			SUBPROCESS_ASSERT(ts == TRUE);
		};

		_iterate_child_processes(m_process_handle->pid, terminate);

		terminate(m_process_handle->process_handle.handle);

		this->reset_no_lock();
	}

}
