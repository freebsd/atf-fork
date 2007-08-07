#
# Automated Testing Framework (atf)
#
# Copyright (c) 2007 The NetBSD Foundation, Inc.
# All rights reserved.
#
# This code is derived from software contributed to The NetBSD Foundation
# by Julio M. Merino Vidal, developed as part of Google's Summer of Code
# 2007 program.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. All advertising materials mentioning features or use of this
#    software must display the following acknowledgement:
#        This product includes software developed by the NetBSD
#        Foundation, Inc. and its contributors.
# 4. Neither the name of The NetBSD Foundation nor the names of its
#    contributors may be used to endorse or promote products derived
#    from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND
# CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
# INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
# IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
# GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
# IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
# OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
# IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#

# TODO: Bring in the checks in the bootstrap testsuite for atf_check.

create_helper()
{
    atf_info "Creating helper.sh"
    cat >helper.sh <<EOF
main_head()
{
    atf_set "descr" "Helper test case"
}
main_body()
{
EOF

    cat >>helper.sh

    cat >>helper.sh <<EOF
}

atf_init_test_cases()
{
    atf_add_test_case main
}
EOF
    atf-compile -o helper helper.sh
}

info_ok_head()
{
    atf_set "descr" "Verifies that atf_check prints an informative" \
                    "message even when the command is successful"
}
info_ok_body()
{
    create_helper <<EOF
atf_check 'true' 0 null null
EOF
    atf_check './helper -r3 3>resout' 0 stdout stderr
    grep 'Checking command.*true' stdout >/dev/null || \
        atf_fail "atf_check does not print an informative message"

    create_helper <<EOF
atf_check 'false' 1 null null
EOF
    atf_check './helper -r3 3>resout' 0 stdout stderr
    grep 'Checking command.*false' stdout >/dev/null || \
        atf_fail "atf_check does not print an informative message"
}

expout_mismatch_head()
{
    atf_set "descr" "Verifies that atf_check prints a diff of the" \
                    "stdout and the expected stdout if the two do no" \
                    "match"
}
expout_mismatch_body()
{
    create_helper <<EOF
cat >expout <<SECONDEOF
foo
SECONDEOF
atf_check 'echo bar' 0 expout null
EOF
    atf_check './helper -r3 3>resout' 1 stdout stderr
    grep 'Checking command.*echo bar' stdout >/dev/null || \
        atf_fail "atf_check does not print an informative message"
    grep 'stdout:' stdout >/dev/null || \
        atf_fail "atf_check does not print the stdout header"
    grep 'stderr:' stdout >/dev/null && \
        atf_fail "atf_check prints the stdout header"
    grep '^-foo' stdout >/dev/null || \
        atf_fail "atf_check does not print the stdout's diff"
    grep '^+bar' stdout >/dev/null || \
        atf_fail "atf_check does not print the stdout's diff"
}

experr_mismatch_head()
{
    atf_set "descr" "Verifies that atf_check prints a diff of the" \
                    "stderr and the expected stderr if the two do no" \
                    "match"
}
experr_mismatch_body()
{
    create_helper <<EOF
cat >experr <<SECONDEOF
foo
SECONDEOF
atf_check 'echo bar 1>&2' 0 null experr
EOF
    atf_check './helper -r3 3>resout' 1 stdout stderr
    grep 'Checking command.*echo bar' stdout >/dev/null || \
        atf_fail "atf_check does not print an informative message"
    grep 'stdout:' stdout >/dev/null && \
        atf_fail "atf_check prints the stdout header"
    grep 'stderr:' stdout >/dev/null || \
        atf_fail "atf_check does not print the stderr header"
    grep '^-foo' stdout >/dev/null || \
        atf_fail "atf_check does not print the stderr's diff"
    grep '^+bar' stdout >/dev/null || \
        atf_fail "atf_check does not print the stderr's diff"
}

null_stdout_head()
{
    atf_set "descr" "Verifies that atf_check prints a the stdout it got" \
                    "when it was supposed to be null"
}
null_stdout_body()
{
    create_helper <<EOF
atf_check 'echo "These are the contents"' 0 null null
EOF
    atf_check './helper -r3 3>resout' 1 stdout stderr
    grep 'Checking command.*echo.*These.*contents' stdout >/dev/null || \
        atf_fail "atf_check does not print an informative message"
    grep 'stdout:' stdout >/dev/null || \
        atf_fail "atf_check does not print the stdout header"
    grep 'stderr:' stdout >/dev/null && \
        atf_fail "atf_check prints the stderr header"
    grep 'These are the contents' stdout >/dev/null || \
        atf_fail "atf_check does not print stdout's contents"
}

null_stderr_head()
{
    atf_set "descr" "Verifies that atf_check prints a the stderr it got" \
                    "when it was supposed to be null"
}
null_stderr_body()
{
    create_helper <<EOF
atf_check 'echo "These are the contents" 1>&2' 0 null null
EOF
    atf_check './helper -r3 3>resout' 1 stdout stderr
    grep 'Checking command.*echo.*These.*contents' stdout >/dev/null || \
        atf_fail "atf_check does not print an informative message"
    grep 'stdout:' stdout >/dev/null && \
        atf_fail "atf_check prints the stdout header"
    grep 'stderr:' stdout >/dev/null || \
        atf_fail "atf_check does not print the stderr header"
    grep 'These are the contents' stdout >/dev/null || \
        atf_fail "atf_check does not print stderr's contents"
}

no_isolated_head()
{
    atf_set "descr" "Verifies that atf_check fails if isolated=no"
}
no_isolated_body()
{
    cat >helper.sh <<EOF
main_head()
{
    atf_set "descr" "Helper test case"
    atf_set "isolated" "no"
}
main_body()
{
    atf_check 'true' 0 null null
}

atf_init_test_cases()
{
    atf_add_test_case main
}
EOF
    atf-compile -o helper helper.sh

    atf_check './helper' 1 ignore stderr
    atf_check 'grep "isolated=no" stderr' 0 ignore null
}

change_cwd_head()
{
    atf_set "descr" "Verifies that atf_check uses the correct work" \
                    "directory even if changing the current one"
}
change_cwd_body()
{
    create_helper <<EOF
mkdir foo
chmod 555 foo
cd foo
atf_check 'echo Hello' 0 stdout null
cd -
test -f stdout || atf_fail "Used incorrect work directory"
echo Hello >bar
cmp -s stdout bar || atf_fail "Used incorrect work directory"
EOF
    atf_check './helper -r3 3>resout' 0 stdout stderr
    atf_check 'grep -i passed resout' 0 ignore null
}

equal_head()
{
    atf_set "descr" "Verifies that atf_check_equal works"
}
equal_body()
{
    create_helper <<EOF
atf_check_equal a a
EOF
    atf_check './helper -r3 3>resout' 0 ignore ignore

    create_helper <<EOF
atf_check_equal a b
EOF
    atf_check './helper -r3 3>resout' 1 ignore ignore
    atf_check 'grep "a != b (a != b)" resout' 0 ignore null

    create_helper <<EOF
x=a
y=a
atf_check_equal '\$x' '\$y'
EOF
    atf_check './helper -r3 3>resout' 0 ignore ignore

    create_helper <<EOF
x=a
y=b
atf_check_equal '\$x' '\$y'
EOF
    atf_check './helper -r3 3>resout' 1 ignore ignore
    atf_check 'grep "\$x != \$y (a != b)" resout' 0 ignore null
}

atf_init_test_cases()
{
    atf_add_test_case info_ok
    atf_add_test_case expout_mismatch
    atf_add_test_case experr_mismatch
    atf_add_test_case null_stdout
    atf_add_test_case null_stderr
    atf_add_test_case no_isolated
    atf_add_test_case change_cwd
    atf_add_test_case equal
}

# vim: syntax=sh:expandtab:shiftwidth=4:softtabstop=4
