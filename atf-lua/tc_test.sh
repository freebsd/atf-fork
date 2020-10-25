#
# SPDX-License-Identifier: BSD-2-Clause-FreeBSD
#
# Copyright (c) 2017 Kyle Evans <kevans@FreeBSD.org>
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#
# $FreeBSD$

atf_test_case default_status
default_status_head()
{
    atf_set "descr" "Verifies that test cases get the correct default" \
        "status if they did not provide any"
}
default_status_body()
{
    h="$(atf_get_srcdir)/misc_helpers -s $(atf_get_srcdir)"
    atf_check -s eq:0 -o ignore -e ignore ${h} tc_pass_true
    atf_check -s eq:1 -o ignore -e ignore ${h} tc_pass_false
    atf_check -s eq:1 -o ignore -e match:'An error' ${h} tc_fail
}

atf_test_case missing_body
missing_body_head()
{
    atf_set "descr" "Verifies that test cases without a body are reported" \
        "as failed"
}

missing_body_body()
{
    cat >expout <<EOF
failed: Test case not implemented
EOF

    h="$(atf_get_srcdir)/misc_helpers -s $(atf_get_srcdir)"
    atf_check -s eq:1 -o file:expout -e ignore ${h} tc_missing_body
}

atf_test_case expect_exit_fail
expect_exit_fail_head()
{
    atf_set "descr" "Verifies that a failed exit expectation reports as" \
        "failed."
}

expect_exit_fail_body()
{
    cat >expout <<EOF
expected_exit(1): Expected exit
EOF

    h="$(atf_get_srcdir)/misc_helpers -s $(atf_get_srcdir)"
    atf_check -s eq:2 -o file:expout -e ignore -x \
        "EXIT_CODE=2 ${h} tc_expect_exit_fail"
}
atf_test_case expect_signal
expect_signal_head()
{
    atf_set "descr" "Verifies that a signalled exit expectation outputs" \
        "properly."
}

expect_signal_body()
{
# These should remain intact since they're supposed to match verbatim.
# NO_CHECK_STYLE_BEGIN
    cat >expout_any <<EOF
expected_signal: Expected signal
failed: Test case was expected to receive a termination signal but it continued execution
EOF
    cat >expout_sigusr1 <<EOF
expected_signal(30): Expected signal
failed: Test case was expected to receive a termination signal but it continued execution
EOF
# NO_CHECK_STYLE_END

    h="$(atf_get_srcdir)/misc_helpers -s $(atf_get_srcdir)"
    atf_check -s eq:1 -o file:expout_any -e ignore -x ${h} \
        tc_expect_signal_any
    atf_check -s eq:1 -o file:expout_sigusr1 -e ignore -x ${h} \
        tc_expect_signal_sigusr1
}

atf_test_case check_srcdir
check_srcdir_head()
{
    atf_set "descr" "Verifies the ability to grab srcdir from a test case."
}

check_srcdir_body()
{

    h="$(atf_get_srcdir)/misc_helpers -s $(atf_get_srcdir)"
    atf_check -s eq:0 -o ignore -e ignore -x \
        "ATF_SRCDIR=$(atf_get_srcdir) ${h} tc_check_srcdir"
}

atf_init_test_cases()
{

    atf_add_test_case default_status
    atf_add_test_case missing_body
    atf_add_test_case expect_exit_fail
    atf_add_test_case expect_signal
    atf_add_test_case check_srcdir
}

# vim: syntax=sh:expandtab:shiftwidth=4:softtabstop=4
