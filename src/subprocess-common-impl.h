#pragma once

#include "subprocess.h"

namespace splib
{

	bool subprocess::stdin_write(const char* data, const std::size_t sz) noexcept
	{
		if (data == nullptr || sz == 0)
			return false;

		std::lock_guard<std::mutex> lock(m_process_mutex);
		if (m_stdin_pipe == nullptr)
			return false;

		return m_stdin_pipe->write(data, sz);
	}

}
