#pragma once

#include "subprocess.h"

#include <thread>
#include <chrono>

#include <fcntl.h>
#include <spawn.h>
#include <unistd.h>
#include <sys/wait.h>

namespace splib
{

	namespace detail
	{
		struct pipe_handle
		{
			int handles[2] = { -1, -1 };

			inline ~pipe_handle() noexcept
			{
			}
			void close_pipe() noexcept
			{
				if (handles[1] != -1)
					close(handles[1]);
				if (handles[0] != -1)
					close(handles[0]);
			}

			pipe_handle() = default;
			pipe_handle(pipe_handle&) = delete;
			pipe_handle& operator=(const pipe_handle&) = delete;
		};

		class posix_stream_handle : public pipe_handle
		{
		public:
			inline posix_stream_handle(subprocess::stdfunc_t&& f) noexcept
				: func(std::move(f))
			{
			}

			subprocess::stdfunc_t func;
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
		inline ~suprocess_impl()
		{
			::write(m_close_pipe.handles[1], ".", 1);

			if (m_buffer_thread.joinable())
				m_buffer_thread.join();

			stdout_handle.close_pipe();
			stderr_handle.close_pipe();
			m_close_pipe.close_pipe();
		}

		void start(const std::size_t buffer_size) noexcept
		{
			if (pipe(m_close_pipe.handles) != 0)
			{
				return;
			}

			SUBPROCESS_ASSERT(m_buffer_thread.joinable() == false);
			m_buffer_thread = std::thread([this, buffer_size]() {
				std::unique_ptr<char[]> buffer(new char[buffer_size]);

				while (true)
				{
					bool ok = this->stream_buffering(buffer.get(), buffer_size);
					if (ok == false)
						break;
				}
			});
		}

		bool stream_buffering(char* buffer, std::size_t max_buffer_size)
		{
			SUBPROCESS_ASSERT(buffer != nullptr && max_buffer_size > 0);

			auto hout = stdout_handle.handles[0];
			auto herr = stderr_handle.handles[0];
			auto hexit = m_close_pipe.handles[0];

			if (hout == -1 || herr == -1 || hexit == -1)
				return false;

			fd_set set;
			FD_ZERO(&set);
			FD_SET(hout, &set);
			FD_SET(herr, &set);
			FD_SET(hexit, &set);

			auto hmax = std::max(hexit, std::max(hout, herr));

			if (select(hmax + 1, &set, NULL, NULL, nullptr) == -1)
				return false;

			bool data_read = false;

			if (FD_ISSET(hexit, &set))
				return false;

			if (FD_ISSET(hout, &set))
			{
				auto num = read(hout, buffer, max_buffer_size);
				if (num <= 0)
					return false;
				if (stdout_handle.func != nullptr)
					stdout_handle.func(buffer, std::size_t(num));
				data_read = true;
			}

			if (FD_ISSET(herr, &set))
			{
				auto num = read(herr, buffer, max_buffer_size);
				if (num <= 0)
					return false;
				if (stderr_handle.func != nullptr)
					stderr_handle.func(buffer, std::size_t(num));
				data_read = true;
			}

			return data_read;
		}

	public:
		detail::posix_stream_handle stdout_handle;
		detail::posix_stream_handle stderr_handle;

		pid_t pid = 0;

	protected:
		std::thread			m_buffer_thread;
		detail::pipe_handle m_close_pipe;
	};

	class pipe_impl : public detail::pipe_handle
	{
	public:
		inline ~pipe_impl()
		{
			close_pipe();
		}
		inline bool write(const char* data, const std::size_t sz) noexcept
		{
			SUBPROCESS_ASSERT(handles[1] != -1 && data != nullptr && sz > 0);
			auto num = ::write(handles[1], data, sz);
			return num > 0;
		}
	};

	bool subprocess::start(const CreateData& cd, stdfunc_t stdout_func, stdfunc_t stderr_func) noexcept
	{

		// run_cmd("ls");

		auto simpl = std::make_unique<suprocess_impl>(std::move(stdout_func), std::move(stderr_func));
		auto pimpl = std::make_unique<pipe_impl>();

		if (pipe(simpl->stdout_handle.handles) != 0)
		{
			return false;
		}
		if (pipe(simpl->stderr_handle.handles) != 0)
		{
			return false;
		}
		if (pipe(pimpl->handles) != 0)
		{
			return false;
		}

		posix_spawn_file_actions_t action;
		posix_spawn_file_actions_init(&action);

		posix_spawn_file_actions_adddup2(&action, pimpl->handles[0], STDIN_FILENO);
		posix_spawn_file_actions_adddup2(&action, simpl->stdout_handle.handles[1], STDOUT_FILENO);
		posix_spawn_file_actions_adddup2(&action, simpl->stderr_handle.handles[1], STDERR_FILENO);

		std::vector<std::string> argbuffers;
		std::vector<char*>		 argv;

		for (const auto& a : cd.argv)
		{
			argbuffers.push_back(a);
		}
		for (auto& a : argbuffers)
		{
			argv.push_back(&a[0]);
		}
		argv.push_back(nullptr);

		const char* exe = cd.exe.c_str();

		if (posix_spawn(&(simpl->pid), exe, &action, nullptr, argv.data(), nullptr) != 0)
		{
			return false;
		}

		simpl->start(cd.buffer_size);

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
		pid_t pid;
		{
			std::lock_guard<std::mutex> lock(m_process_mutex);
			SUBPROCESS_ASSERT(m_process_handle != nullptr);
			pid = m_process_handle->pid;
		}
		SUBPROCESS_ASSERT(pid != 0);

		int status = -1;
		int result = -1;
		do
		{
			if (waitpid(pid, &status, 0) == -1)
			{
				result = -1;
				break;
			}

			result = WEXITSTATUS(status);
		} while (!WIFEXITED(status) && !WIFSIGNALED(status));

#ifdef SUBPROCESS_POSIX_SIGNALED_JOIN_ERROR
		if (WIFSIGNALED(status) && result == 0)
		{
			if (status > 0)
				result = -status;
			else if (status < 0)
				result = status;
			else
				result = -1;
		}
#endif

		{
			std::lock_guard<std::mutex> lock(m_process_mutex);
			this->reset_no_lock();
		}

		return result;
	}

	void subprocess::kill() noexcept
	{
		{
			std::lock_guard<std::mutex> lock(m_process_mutex);
			if (m_process_handle == nullptr)
				return;

			auto id = m_process_handle->pid;

			::kill(-id, SIGTERM);
			::kill(id, SIGTERM);
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(16));
		// give 16 ms time for the process to terminate, then kill

		{
			std::lock_guard<std::mutex> lock(m_process_mutex);
			if (m_process_handle == nullptr)
				return;

			auto id = m_process_handle->pid;
			::kill(-id, SIGKILL);
			::kill(id, SIGKILL);

			this->reset_no_lock();
		}
	}

}
