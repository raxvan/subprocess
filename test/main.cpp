
#include "subprocess.h"
#include <iostream>
#include <fstream>
#include <thread>

using namespace splib;

struct result
{
	std::string sout;
	std::string serr;
	int			rc;
};

void run(result& out, const subprocess::CreateData& cd)
{
	subprocess p;

	auto sout = [&](const char* buffer, const std::size_t sz) {
		out.sout += std::string(buffer, sz);
	};
	auto serr = [&](const char* buffer, const std::size_t sz) {
		out.serr += std::string(buffer, sz);
	};
	bool ok = p.start(cd, sout, serr);
	TTF_ASSERT(ok);
	out.rc = p.join();
}

void test_sucess_cmd()
{
	result				   r;
	subprocess::CreateData cd;
	TTF_ASSERT(cd.make_cmd("echo hello"));
	run(r, cd);
	TTF_ASSERT(r.rc == 0);
	TTF_ASSERT(r.sout == "hello\r\n");
	TTF_ASSERT(r.serr.empty());
}

void test_exit_code_cmd()
{
	result				   r;
	subprocess::CreateData cd;
	TTF_ASSERT(cd.make_cmd("exit 3"));
	run(r, cd);
	TTF_ASSERT(r.rc == 3);
	TTF_ASSERT(r.sout.empty());
	TTF_ASSERT(r.serr.empty());

	TTF_ASSERT(cd.make_cmd("exit -1"));
	run(r, cd);
	TTF_ASSERT(r.rc == -1);
	TTF_ASSERT(r.sout.empty());
	TTF_ASSERT(r.serr.empty());
}

void test_stderr_cmd()
{
	result				   r;
	subprocess::CreateData cd;
	TTF_ASSERT(cd.make_cmd("echo hello 1>&2"));
	run(r, cd);
	TTF_ASSERT(r.rc == 0);
	TTF_ASSERT(r.sout.empty());
	TTF_ASSERT(r.serr == "hello \r\n");
}

void test_reuse_cmd()
{
	std::string sout;
	std::string serr;

	auto fout = [&](const char* buffer, const std::size_t sz) {
		sout += std::string(buffer, sz);
	};
	auto ferr = [&](const char* buffer, const std::size_t sz) {
		serr += std::string(buffer, sz);
	};

	subprocess			   p;
	subprocess::CreateData cd;

	TTF_ASSERT(cd.make_cmd("echo hello"));

	p.start(cd, fout, ferr);
	auto rc = p.join();

	TTF_ASSERT(rc == 0);
	TTF_ASSERT(sout == "hello\r\n");
	TTF_ASSERT(serr.empty());

	sout.clear();
	TTF_ASSERT(cd.make_cmd("echo hello"));

	p.start(cd, fout, ferr);
	rc = p.join();

	TTF_ASSERT(rc == 0);
	TTF_ASSERT(sout == "hello\r\n");
	TTF_ASSERT(serr.empty());
}

void test_kill_cmd()
{
	std::string sout;
	std::string serr;

	auto fout = [&](const char* buffer, const std::size_t sz) {
		sout += std::string(buffer, sz);
	};
	auto ferr = [&](const char* buffer, const std::size_t sz) {
		serr += std::string(buffer, sz);
	};

	subprocess			   p;
	subprocess::CreateData cd;

	TTF_ASSERT(cd.make_ps("sleep 2 ; echo done > failed.txt"));

	TTF_ASSERT(p.start(cd, fout, ferr));

	std::thread t = std::thread([&]() {
		auto rc = p.join();
#ifdef SUBPROCESS_POSIX_SIGNALED_JOIN_ERROR
		TTF_ASSERT(rc != 0);
#endif
		std::cout << "Process joined the dead with rc:" << rc << std::endl;
	});

	ttf::utils::wait_miliseconds(500);

	p.kill();
	TTF_ASSERT(p.joinable() == false);

	ttf::utils::wait_miliseconds(2000);
	t.join();

	std::ifstream fin("failed.txt");
	TTF_ASSERT(fin.is_open() == false);

	TTF_ASSERT(sout.empty());
	TTF_ASSERT(serr.empty());
}

void test_sucess_shell()
{
	result				   r;
	subprocess::CreateData cd;
	TTF_ASSERT(cd.make_shell("echo hello"));
	run(r, cd);

	TTF_ASSERT(r.rc == 0);
	TTF_ASSERT(r.sout == "hello\n");
	TTF_ASSERT(r.serr.empty());
}

void test_exit_code_shell()
{
	result				   r;
	subprocess::CreateData cd;
	TTF_ASSERT(cd.make_shell("exit 3"));
	run(r, cd);
	TTF_ASSERT(r.rc == 3);
	TTF_ASSERT(r.sout.empty());
	TTF_ASSERT(r.serr.empty());
}

void test_stderr_shell()
{
	result				   r;
	subprocess::CreateData cd;
	TTF_ASSERT(cd.make_shell("echo hello >&2"));
	run(r, cd);

	TTF_ASSERT(r.rc == 0);
	TTF_ASSERT(r.sout.empty());
	TTF_ASSERT(r.serr == "hello\n");
}

void test_reuse_shell()
{
	std::string sout;
	std::string serr;

	auto fout = [&](const char* buffer, const std::size_t sz) {
		sout += std::string(buffer, sz);
	};
	auto ferr = [&](const char* buffer, const std::size_t sz) {
		serr += std::string(buffer, sz);
	};

	subprocess			   p;
	subprocess::CreateData cd;

	TTF_ASSERT(cd.make_shell("echo hello"));

	p.start(cd, fout, ferr);
	auto rc = p.join();

	TTF_ASSERT(rc == 0);
	TTF_ASSERT(sout == "hello\n");
	TTF_ASSERT(serr.empty());

	sout.clear();
	TTF_ASSERT(cd.make_shell("echo hello"));

	p.start(cd, fout, ferr);
	rc = p.join();

	TTF_ASSERT(rc == 0);
	TTF_ASSERT(sout == "hello\n");
	TTF_ASSERT(serr.empty());
}

void test_kill_shell()
{
	std::string sout;
	std::string serr;

	auto fout = [&](const char* buffer, const std::size_t sz) {
		sout += std::string(buffer, sz);
	};
	auto ferr = [&](const char* buffer, const std::size_t sz) {
		serr += std::string(buffer, sz);
	};

	subprocess			   p;
	subprocess::CreateData cd;

	TTF_ASSERT(cd.make_shell("sleep 2 && echo done > failed.txt"));

	p.start(cd, fout, ferr);

	std::thread t = std::thread([&]() {
		auto rc = p.join();
#ifdef SUBPROCESS_POSIX_SIGNALED_JOIN_ERROR
		TTF_ASSERT(rc != 0);
#endif
		std::cout << "Process joined the dead with rc:" << rc << std::endl;
	});

	ttf::utils::wait_miliseconds(500);

	p.kill();
	TTF_ASSERT(p.joinable() == false);

	ttf::utils::wait_miliseconds(2000);
	t.join();

	std::ifstream fin("failed.txt");
	TTF_ASSERT(fin.is_open() == false);

	TTF_ASSERT(sout.empty());
	TTF_ASSERT(serr.empty());
}

void test_main()
{

#ifdef _WIN32
	/*TEST_FUNCTION(test_sucess_cmd);
	TEST_FUNCTION(test_exit_code_cmd);
	TEST_FUNCTION(test_stderr_cmd);
	TEST_FUNCTION(test_reuse_cmd);*/
	TEST_FUNCTION(test_kill_cmd);
#endif

#ifdef __GNUC__
	TEST_FUNCTION(test_sucess_shell);
	TEST_FUNCTION(test_exit_code_shell);
	TEST_FUNCTION(test_stderr_shell);
	TEST_FUNCTION(test_reuse_shell);
	TEST_FUNCTION(test_kill_shell);
#endif
}

TEST_MAIN(test_main)
