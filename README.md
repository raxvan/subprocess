# Subprocess Library for C++

This library offers an intuitive and effective way to interact with subprocesses in C++. It is designed to be straightforward, versatile, and easy to use, so you can control and communicate with subprocesses directly from your C++ application.

## Features

- Start and manage subprocesses from within your application.
- Interact with the standard input/output/error streams of these processes.
- Synchronize and wait for subprocesses to finish.
- Capture and handle exit codes.

## Getting Started

To get started with the library, you should first clone the repository:

```bash
git clone https://github.com/your-username/subprocess-cpp.git
```

## Usage:

Header:
```
#include "include/subprocess.h"


subprocess p;
subprocess::CreateData cd;

cd.make_shell("echo hello");

auto sout = [&](const char* buffer, const std::size_t sz) {
	std::cout << std::string(buffer, sz);
};
auto serr = [&](const char* buffer, const std::size_t sz) {
	std::cerr << std::string(buffer, sz);
};
if(p.start(cd, sout, serr))
{
	//started successfully

	int rc = p.join();
	std::cout << "process terminated with " << rc << std::endl;
}
else
{
	std::cerr << "failed to start process!" << std::endl
}


```

Implementation:
You can either include `src/subprocess.cpp` in a cpp file or add it to your project
