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
export PATH="$CURRENTDIR/../third-party/build/bin/:$PATH"
export LD_LIBRARY_PATH="$CURRENTDIR/../third-party/build/lib:$LD_LIBRARY_PATH"

initialize=
memserver=
metaserver=
cisserver=
cisrpcport=8787
metarpcport=8788
memrpcport=8789
libfabricport=7500
provider=sockets
npe=1
print_help() {
	tput bold
	echo "SYNOPSIS :"
	tput reset
	echo ""
	echo "  source setup.sh <options>"
	echo ""
	tput bold
	echo "OPTIONS :"
	tput reset
	echo ""
    echo "  -n				: Number of PEs"
	echo ""
	echo "  --memserver     : IP address of memory server"
	echo "                    Note : This option is necessary to start the memory server"
	echo ""
	echo "  --metaserver    : IP address of metadata server"
	echo "                    Note : This option is necessary to start the metadata server"
	echo ""
	echo "  --cisserver    : IP address of CIS server"
	echo "                    Note : This option is necessary to start the CIS server"
	echo ""
	echo "  --memrpcport    : RPC port for memory service"
	echo ""
	echo "  --metarpcport   : RPC port for metadata server"
	echo ""
	echo "  --cisrpcport   : RPC port for CIS server"
	echo ""
	echo "  --libfabricport : Libfabric port"
	echo ""
	echo "  --provider      : Libfabric provider"
	echo ""
	echo "  --fam_path      : Location of FAM"
	echo ""
	echo "  --init          : Initializes NVMM (use this option for shared memory model)"
	echo ""
	exit
}

if [ $# -lt 1 ]; then
	print_help
fi

while :; do
	case $1 in
		-h|-\?|--help)
			print_help
			;;
		--init)
			initialize=true
			;;
		-n=?*)
			npe=${1#*=}
			;;
		-n=)
            echo 'ERROR: "-n" requires a non-empty option argument.'
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
		--cisserver=?*)
			cisserver=${1#*=}
            ;;
		--cisserver=)
			echo 'ERROR: "--cisserver" requires a non-empty option argument.'
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
		--cisrpcport=?*)
            cisrpcport=${1#*=}
            ;;
        --cisrpcport=)
            echo 'ERROR: "--cisrpcport" requires a non-empty option argument.'
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
		--fam_path=?*)
			fam_path=${1#*=}
            ;;
		--fam_path=)
			echo 'ERROR: "--fam_path" requires a non-empty option argument.'
            ;;
		-?*)
			echo "WARN: Unknown option (ignored): ${1}"
			;;
		*)
			break
	esac

	shift
done

#kill services if already running
pkill memory_server
pkill metadata_server
pkill cis_server

#set environment variables for running tests
export OPENFAM_TEST_COMMAND="$CURRENTDIR/../third-party/build/bin/mpirun"
export OPENFAM_TEST_OPT="-n $npe"

cd src

#itialize nvmm 
if [ "$initialize" ]; then
	if [ "$fam_path" ]; then
            fam_path_ARG="-f ${fam_path}"
    fi
    ./memory_server -i $fam_path_ARG
fi 

#Run Metadata Service
if [ "$metaserver" ]; then
	echo "Starting Metadata Server..."
	./metadata_server -a $metaserver -r ${metarpcport} &
	sleep 1
fi

#Run Memory Service
if [ "$memserver" ]; then
	echo "Starting Memory Server..."
	if [ "$fam_path" ]; then
         	fam_path_ARG="-f ${fam_path}"		
	fi
	./memory_server -a ${memserver} -r ${memrpcport} -l ${libfabricport} -p ${provider} $fam_path_ARG &
	sleep 1
fi

#Run CIS 
if [ "$cisserver" ]; then
	echo "Starting CISServer..."
	./cis_server -a ${cisserver} -r ${cisrpcport} &
fi
cd ..

