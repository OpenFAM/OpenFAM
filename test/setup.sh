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

memserver=
metaserver=
memrpcport=8787
metarpcport=8989
libfabricport=7500
provider=sockets

while :; do
	case $1 in
		-h|-\?|--help)
			tput bold
			echo "Synopys :"
			tput reset
			echo ""
			echo "	source setup.sh <options>"
			echo ""
			tput bold
			echo "Options :"
			tput reset
			echo ""
			echo "	--memserver     : IP address of memory server"
			echo ""
			echo "	--metaserver    : IP address of metadata server"
			echo ""
			echo "	--memrpcport    : RPC port for memory service"
			echo ""
			echo "	--metarpcport   : RPC port for metadata server"
			echo ""
			echo "	--libfabricport : Libfabric port"
			echo ""
			echo "	--provider      : Libfabric provider"
			echo ""
			;;
		--memserver=?*)
			memserver=${1#*=}
			;;
		--memserver=)
			echo 'ERROR: "--memserver" requires a non-empty option argument.'
			;;
		--metaserver=?*)
			metaserver=${1#*=}
			;;
		--metaserver=)
			echo 'ERROR: "--metaserver" requires a non-empty option argument.'
            ;;
		--memrpcport=?*)
			memrpcport=${1#*=}
			;;
		--memrpcport=)
			echo 'ERROR: "--memrpcport" requires a non-empty option argument.'
			;;
		--metarpcport=?*)
			metarpcport=${1#*=}
			;;
		--metarpcport=)
			echo 'ERROR: "--metarpcport" requires a non-empty option argument.'
            ;;
		--libfabricport=?*)
			libfabricport=${1#*=}
            ;;
		--libfabricport=)
			echo 'ERROR: "--libfabricport" requires a non-empty option argument.'
            ;;
		--provider=?*)
			provider==${1#*=}
            ;;
		--provider=)
			echo 'ERROR: "--provider" requires a non-empty option argument.'
            ;;
		-?*)
			echo "WARN: Unknown option (ignored): ${1}"
			;;
		*)
			break
	esac

	shift
done




#Start Memory server
pkill memoryserver
pkill metadataserver

cd src
if [ "$metaserver" ]; then
	echo "Starting Metadata Server..."
	./metadataserver -a $metaserver -r ${metarpcport} &
	sleep 1
else 
	echo "ERROR: --metaserver option needs to be provided"
fi


if [ "$memserver" ]; then
	echo "Starting Memory Server..."
	./memoryserver -m ${memserver} -r ${memrpcport} -l ${libfabricport} -p ${provider} &
else 
	echo "ERROR: --memserver option needs to be provided"
fi
	
cd ..

