--
-- SPDX-License-Identifier: BSD-2-Clause-FreeBSD
--
-- Copyright (c) 2018 Kyle Evans <kevans@FreeBSD.org>
--
-- Redistribution and use in source and binary forms, with or without
-- modification, are permitted provided that the following conditions
-- are met:
-- 1. Redistributions of source code must retain the above copyright
--    notice, this list of conditions and the following disclaimer.
-- 2. Redistributions in binary form must reproduce the above copyright
--    notice, this list of conditions and the following disclaimer in the
--    documentation and/or other materials provided with the distribution.
--
-- THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
-- ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
-- IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
-- ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
-- FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
-- DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
-- OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
-- HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
-- LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
-- OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
-- SUCH DAMAGE.
--
-- $FreeBSD$
--

local atf = require("atf")

atf.TestCase "check_basic" {
	body = function()
		atf.check_equal(true, true)
		atf.check_equal(false, false)
		atf.check_equal(1, 1)
		atf.check_equal("string", "string")
	end,
}

atf.TestCase "check_fail_bool" {
	body = function()
		atf.expect_fail("Intentional failure")
		atf.check_equal(true, false)
	end,
}

atf.TestCase "check_fail_num" {
	body = function()
		atf.expect_fail("Intentional failure")
		atf.check_equal(3, 1)
	end,
}

atf.TestCase "check_fail_str" {
	body = function()
		atf.expect_fail("Intentional failure")
		atf.check_equal("test", "tes")
	end,
}

atf.TestCase "check_fail_table" {
	body = function()
		atf.expect_fail("Intentional failure")
		atf.check_equal({}, {})
	end,
}

atf.TestCase "check_fail_obj" {
	body = function()
		atf.expect_fail("Intentional failure")

		local _meta = {
			__eq = function(lhs, rhs)
				return lhs["val"] == rhs["val"]
			end,
		}

		local a_obj = setmetatable({val = 3}, _meta)
		local b_obj = setmetatable({val = 4}, _meta)
		atf.check_equal(a_obj, b_obj)
	end,
}

atf.TestCase "check_obj" {
	body = function()
		local _meta = {
			__eq = function(lhs, rhs)
				return lhs["val"] == rhs["val"]
			end,
		}

		local a_obj = setmetatable({val = 3}, _meta)
		local b_obj = setmetatable({val = 3}, _meta)
		atf.check_equal(true, a_obj == b_obj)
		atf.check_equal(a_obj, b_obj)
	end,
}

atf.TestCase "expect_exit_any_success" {
	body = function()
		atf.expect_exit("Expected exit")

		os.exit(0)
	end,
}


atf.TestCase "expect_exit_any_fail" {
	body = function()
		atf.expect_exit("Expected exit")

		os.exit(1)
	end,
}

atf.TestCase "expect_exit_specific_ok" {
	body = function()
		atf.expect_exit("Expected exit", 1)

		os.exit(1)
	end,
}

atf.TestCase "expect_timeout" {
	head = function()
		atf.set("timeout", "2")
	end,
	body = function()
		atf.expect_timeout("Planned timeout")

		local _end = os.clock() + 4
		-- Spin!
		while os.clock() < _end do end
	end,
}

atf.TestCase "fail" {
	body = function()
		atf.expect_fail("Explicit failure")
		atf.fail("See? I wasn't kidding.")
	end,
}

atf.TestCase "pass" {
	body = function()
		atf.pass()
	end,
}

atf.TestCase "without_cleanup" {
	body = function()
		atf.check_equal(nil, atf.get("has.cleanup"))
	end,
}

atf.TestCase "with_cleanup" {
	body = function()
		atf.check_equal("true", atf.get("has.cleanup"))
	end,
	cleanup = function()
	end,
}

