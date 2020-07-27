 #
 # setup.sh
 # Copyright (c) 2019 Hewlett Packard Enterprise Development, LP. All rights
 # reserved. Redistribution and use in source and binary forms, with or without
 # modification, are permitted provided that the following conditions are met:
 # 1. Redistributions of source code must retain the above copyright notice,
 # this list of conditions and the following disclaimer.
 # 2. Redistributions in binary form must reproduce the above copyright notice,
 # this list of conditions and the following disclaimer in the documentation
 # and/or other materials provided with the distribution.
 # 3. Neither the name of the copyright holder nor the names of its contributors
 # may be used to endorse or promote products derived from this software without
 # specific prior written permission.
 #
 #    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 # IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 # IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 # ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 # LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 # CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 # SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 #    INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 # CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 # ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 # POSSIBILITY OF SUCH DAMAGE.
 #
 # See https://spdx.org/licenses/BSD-3-Clause
 #
 #

#!/bin/bash
CURRENTDIR=`pwd`
export PATH="$CURRENTDIR/../../third-party/build/bin/:$PATH"
export LD_LIBRARY_PATH="$CURRENTDIR/../../third-party/build/lib:$LD_LIBRARY_PATH"

if [ $# -lt 1 ]
then
echo "Error: memory server not specified."
echo "usage: source setup.sh <memory_server>"
exit 1
fi

#Start Memory server
pkill memoryserver
pkill metadataserver
cd src
echo "Starting Metadata Server..."
./metadataserver -a ${1} -r ${5:-8989} &
sleep 1
echo "Starting Memory Server..."
./memoryserver -m ${1} -r ${2:-8787} -l ${3:-7500} -p ${4:-sockets} &
cd ..

