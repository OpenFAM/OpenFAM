#!/usr/bin/python3
 # run_test.py
 # Copyright (c) 2022 Hewlett Packard Enterprise Development, LP. All rights reserved.
 # Redistribution and use in source and binary forms, with or without modification, are permitted provided
 # that the following conditions are met:
 # 1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
 # 2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer
 #    in the documentation and/or other materials provided with the distribution.
 # 3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products
 #    derived from this software without specific prior written permission.
 #
 #    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING,
 #    BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 #    SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 #    DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 #    INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 #    OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 #
 # See https://spdx.org/licenses/BSD-3-Clause
 #
 #

import subprocess
import sys
import os

openfam_test_exe=sys.argv[1]
openfam_test_arg=""

if len(sys.argv) > 2:
    openfam_test_arg=sys.argv[2]

command_line = [
    "",
    "",
    openfam_test_exe,
    openfam_test_arg,
]

value = os.environ.get("OPENFAM_TEST_COMMAND")
if value:
    command_line[0]= value

value = os.environ.get("OPENFAM_TEST_OPT")
if value:
    command_line[1]= value


command =""
for i in command_line:
    if i!="" and i!=None:
        command += i + " "


command = command.strip()
print("Test Command:", command)

result = subprocess.call(command)
result >>= 8

#If the return code from test is 77 return that back to gtest
#so that the corresponding test can be skipped
if result == 77:
    sys.exit(result)
#Other non zero values implies that test has failed.
elif (result) != 0:
    sys.exit(1)

#For success case return zero
sys.exit(0)
