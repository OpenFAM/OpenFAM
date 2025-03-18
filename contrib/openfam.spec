#
# spec file for package openfam-radixtree
#
# Copyright (c) 2024 Hewlett Packard Enterprise Development, LP.
#
# All modifications and additions to the file contributed by third parties
# remain the property of their copyright owners, unless otherwise agreed
# upon. The license for this file, and modifications and additions to the
# file, is the same license as for the pristine package itself (unless the
# license for the pristine package is not an Open Source License, in which
# case the license is the MIT License). An "Open Source License" is a
# license that conforms to the Open Source Definition (Version 1.9)
# published by the Open Source Initiative.

# Please submit bugfixes or comments via https://bugs.opensuse.org/
#

%define git_sha 19942692089f8fa1a1b691d3f56ffd6ea4fe7ea0

Name:           openfam
Version:        3.2_pre20240516
Release:        0
Summary:        OpenFAM Reference Implementation 
License:        BSD
Group:          Development/Libraries/C and C++
URL:            https://github.com/opencube-horizon/openfam
Source0:        https://github.com/opencube-horizon/openfam/archive/%{git_sha}.tar.gz
BuildRequires:  cmake
BuildRequires:  pkg-config
BuildRequires:  gcc-c++
%if 0%{?suse_version}
BuildRequires:  libboost_system-devel
BuildRequires:  libboost_filesystem-devel
BuildRequires:  libboost_atomic-devel
BuildRequires:  libboost_context-devel
%else
BuildRequires:  boost-devel
%endif
BuildRequires:  libpmem-devel
BuildRequires:  yaml-cpp-devel
BuildRequires:  openfam-nvmm-devel
BuildRequires:  openfam-radixtree-devel
BuildRequires:  protobuf-devel
BuildRequires:  grpc-devel
BuildRequires:  pmix-devel
BuildRequires:  libfabric-devel

%description
OpenFAM is is an API designed for clusters that contain disaggregated memory.
The primary purpose of the reference implementation at this site is to enable
experimentation with the OpenFAM API, with the broader goal of gathering
feedback from the community on how the API should evolve. The reference
implementation, thus, is designed to run on standard commercially available
servers.

%package -n libopenfam
Summary:        OpenFAM libraries
Group:          System/Libraries

%package        server
Summary:        OpenFAM servers
Group:          Applications/System
Requires:       libopenfam = %{version}

%package        devel
Summary:        Development files for %{name}
Group:          Development/Libraries/C and C++
Requires:       libopenfam = %{version}

%description -n libopenfam
libopenfam provides the OpenFAM libraries.

%description    server
The %{name}-server package contains the OpenFAM
metadata_server, memory_server and cis_server.

%description    devel
The %{name}-devel package contains libraries and header files for
developing applications that use %{name}.

%prep
%setup -q -n %{name}-%{git_sha}

%build
%cmake -DWITH_TESTS=OFF -DWITH_EXAMPLES=OFF -DWITH_PMI2=OFF
%cmake_build

%install
%cmake_install

%post -n libopenfam -p /sbin/ldconfig
%postun -n libopenfam -p /sbin/ldconfig

%files -n libopenfam
%{_libdir}/*.so.*

%files server
%{_bindir}/*_server

%files devel
%{_includedir}/*
%{_libdir}/cmake/OpenFAM
%{_libdir}/*.so
%license LICENSE
%doc README.md

%changelog
