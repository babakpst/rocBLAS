#!/usr/bin/env bash

declare -a input_args
input_args+="$@"

#use readlink rather than realpath for CentOS 6.10 support
ROCBLAS_SRC_PATH=`dirname "$(readlink -m $0)"`

/bin/ln -fs ../../.githooks/pre-commit "$(dirname "$0")/.git/hooks/"

# #################################################
# helper functions
# #################################################
function display_help()
{
cat <<EOF
rocBLAS build & installation helper script.

  Usage:
    $0 (build rocBLAS and put library files at <builddir>/release/rocblas-install)
    $0 <options> (modify default behavior according to the following flags)

  Options:
    -a, --architecture               Set GPU architecture target, e.g. "all, gfx000, gfx900, gfx906:xnack-;gfx908:xnack-".

    --address-sanitizer              Build with address sanitizer enabled.

    -b, --branch <arg>               Specify the Tensile repository branch or tag to use. (eg. develop, mybranch or <commit hash>)

    --build_dir <builddir>           Specify the directory path to build and save library files, dependencies and executables.
                                     Relative paths are relative to the current directory. (Default is ./build)

    -c, --clients                    Build the library clients benchmark and gtest.
                                     (Generated binaries will be located at <builddir>/release/clients/staging)

    --clients_no_fortran             When building clients, build them without Fortran API testing or Fortran examples

    --clients-only                   Skip building the library and only build the clients with a pre-built library.

    --cmake-arg <argument>           Forward the given argument to CMake when configuring the build.

    --cmake_install                  Install minimum cmake version if required.

    --codecoverage                   Build with code coverage profiling enabled, excluding release mode.

    --cleanup                        Remove intermediary build files after build and reduce disk usage.

    -d, --dependencies               Build and install external dependencies.
                                     Dependencies are to be installed in /usr/local. This should be done only once.

    -f, --fork <username>            Specify the username to fork the Tensile GitHub repository. (e.g., ROCmSoftwarePlatform or MyUserName)

    -g, --debug                      Build-in Debug mode, equivalent to set CMAKE_BUILD_TYPE=Debug.
                                     (Default build type is Release)

    -h, --help                       Print this help message

    -i, --install                    Generate and install library package after build.

    -j, --jobs <num>                 Specify the number of parallel jobs to launch, increases memory usage. (Default logical core count)

    -k, --relwithdebinfo             Build-in release debug mode, equivalent to set CMAKE_BUILD_TYPE=RelWithDebInfo.
                                     (Default build type is Release)

    -l, --logic <arg>                Specify the Tesile logic target. (e.g., asm_full, asm_lite, etc)

    --[no-]lazy-library-loading      Enable on-demand loading of Tensile Library files, speeds up the rocblas initialization. (Default is enabled)

    --library-path <blasdir>         Specify path to a pre-built rocBLAS library, when building clients only using '--clients-only' flag.
                                     (Default is /opt/rocm/rocblas)

    --[no-]merge-architectures       Merge TensileLibrary files for different architectures into single file. (Default is disabled)

    --msgpack                        Build Tensile backend to use MessagePack.

    -n, --no-tensile                 Build a subset of the rocBLAS library which does not require Tensile.

    --no-msgpack                     Build Tensile backend not to use MessagePack.

    -o, --cov <version>              Specify the Tensile code_object version. (e.g. V2 or V3)

    -r, --relocatable                Add RUNPATH (based on ROCM_RPATH) and remove ldconf entry.

    -s, --static                     Build rocblas as a static library.

    --skipldconf                     Skip ld.so.conf entry.

    -t, --test_local_path <path>     Specify a local path for Tensile instead of remote GIT repo.

    -u, --use-custom-version <arg>   Ignore the Tensile version and just use the Tensile tag.

    --upgrade_tensile_venv_pip       Upgrade PIP version during Tensile installation.

    --rm-legacy-include-dir          Remove legacy include dir Packaging added for file/folder reorg backward compatibility.
EOF
#           --prefix              Specify an alternate CMAKE_INSTALL_PREFIX for cmake
}

# This function is helpful for dockerfiles that do not have sudo installed, but the default user is root
# true is a system command that completes successfully, function returns success
# prereq: ${ID} must be defined before calling
supported_distro( )
{
  if [ -z ${ID+foo} ]; then
    printf "supported_distro(): \$ID must be set\n"
    exit 2
  fi

  case "${ID}" in
    ubuntu|centos|rhel|fedora|sles|opensuse-leap)
        true
        ;;
    *)  printf "This script is currently supported on Ubuntu, CentOS, RHEL, SLES, OpenSUSE-Leap, and Fedora\n"
        exit 2
        ;;
  esac
}

# This function is helpful for dockerfiles that do not have sudo installed, but the default user is root
check_exit_code( )
{
  if (( $1 != 0 )); then
    exit $1
  fi
}

# This function is helpful for dockerfiles that do not have sudo installed, but the default user is root
elevate_if_not_root( )
{
  local uid=$(id -u)

  if (( ${uid} )); then
    sudo $@
    check_exit_code "$?"
  else
    $@
    check_exit_code "$?"
  fi
}

# Take an array of packages as input, and install those packages with 'apt' if they are not already installed
install_apt_packages( )
{
  package_dependencies="$@"
  printf "\033[32mInstalling following packages from distro package manager: \033[33m${package_dependencies}\033[32m \033[0m\n"
  elevate_if_not_root apt-get -y --no-install-recommends install ${package_dependencies}
}

# Take an array of packages as input, and install those packages with 'yum' if they are not already installed
install_yum_packages( )
{
  package_dependencies="$@"
  printf "\033[32mInstalling following packages from distro package manager: \033[33m${package_dependencies}\033[32m \033[0m\n"
  elevate_if_not_root yum -y --nogpgcheck install ${package_dependencies}
}

# Take an array of packages as input, and install those packages with 'dnf' if they are not already installed
install_dnf_packages( )
{
  package_dependencies="$@"
  printf "\033[32mInstalling following packages from distro package manager: \033[33m${package_dependencies}\033[32m \033[0m\n"
  elevate_if_not_root dnf install -y ${package_dependencies}
}

install_zypper_packages( )
{
    package_dependencies="$@"
    printf "\033[32mInstalling following packages from distro package manager: \033[33m${package_dependencies}\033[32m \033[0m\n"
    elevate_if_not_root zypper install -y ${package_dependencies}
}

install_msgpack_from_source( )
{
    if [[ ! -d "${build_dir}/deps/msgpack-c" ]]; then
      pushd .
      mkdir -p ${build_dir}/deps
      cd ${build_dir}/deps
      git clone -b cpp-3.0.1 https://github.com/msgpack/msgpack-c.git
      cd msgpack-c
      CXX=${cxx} CC=${cc} ${cmake_executable} -DMSGPACK_BUILD_TESTS=OFF -DMSGPACK_BUILD_EXAMPLES=OFF .
      make
      elevate_if_not_root make install
      popd
    fi
}

# Take an array of packages as input, and delegate the work to the appropriate distro installer
# prereq: ${ID} must be defined before calling
# prereq: ${build_clients} must be defined before calling
# prereq: ${tensile_msgpack_backend} must be defined before calling
install_packages( )
{
  if [ -z ${ID+foo} ]; then
    printf "install_packages(): \$ID must be set\n"
    exit 2
  fi

  if [ -z ${build_clients+foo} ]; then
    printf "install_packages(): \$build_clients must be set\n"
    exit 2
  fi

  # dependencies needed to build the rocblas library
  local library_dependencies_ubuntu=( "make"
                                      "python3" "python3-yaml" "python3-venv" "python3*-pip" )
  local library_dependencies_centos_rhel=( "epel-release"
                                      "make" "rpm-build"
                                      "python34" "python3*-PyYAML" "python3-virtualenv"
                                      "gcc-c++" )
  local library_dependencies_centos_8=( "epel-release"
                                      "make" "rpm-build"
                                      "python3" "python3*-PyYAML" "python3-virtualenv"
                                      "gcc-c++" )
  local library_dependencies_rhel_8=( "epel-release"
                                      "make" "rpm-build"
                                      "python36" "python3*-PyYAML" "python3-virtualenv"
                                      "gcc-c++" )
  local library_dependencies_rhel_9=( "epel-release" "openssl-devel"
                                      "make" "rpm-build"
                                      "python39" "python3*-PyYAML" "python3-virtualenv"
                                      "gcc-c++" )
  local library_dependencies_fedora=( "make" "rpm-build"
                                      "python34" "python3*-PyYAML" "python3-virtualenv"
                                      "gcc-c++" "libcxx-devel" )
  local library_dependencies_sles=(   "make" "python3-PyYAML" "python3-virtualenv"
                                      "gcc-c++" "libcxxtools9" "rpm-build" )

  if [[ "${tensile_msgpack_backend}" == true ]]; then
    library_dependencies_ubuntu+=("libmsgpack-dev")
    library_dependencies_fedora+=("msgpack-devel")
  fi

  # wget is needed for msgpack in this case
  if [[ ("${ID}" == "ubuntu") && ("${VERSION_ID}" == "16.04") && "${tensile_msgpack_backend}" == true ]]; then
    if ! $(dpkg -s "libmsgpackc2" &> /dev/null) || $(dpkg --compare-versions $(dpkg-query -f='${Version}' --show libmsgpackc2) lt 2.1.5-1); then
      library_dependencies_ubuntu+=("wget")
    fi
  fi

  # wget and openssl are needed for cmake
  if [ -z "$CMAKE_VERSION" ] || $(dpkg --compare-versions $CMAKE_VERSION lt 3.16.8); then
    if $update_cmake == true; then
      library_dependencies_ubuntu+=("wget" "libssl-dev")
      library_dependencies_centos_rhel+=("wget" "openssl-devel")
      library_dependencies_centos_8+=("wget" "openssl-devel")
      library_dependencies_rhel_8+=("wget" "openssl-devel")
      library_dependencies_rhel_9+=("wget" "openssl-devel")
      library_dependencies_fedora+=("wget")
      library_dependencies_sles+=("wget" "libopenssl-devel")
    fi
  fi

  if [[ "${build_clients}" == true ]]; then
    # dependencies to build the client
    library_dependencies_ubuntu+=( "gfortran" "libomp-dev" )
    library_dependencies_centos_rhel+=( "devtoolset-7-gcc-gfortran" "libgomp" )
    library_dependencies_centos_8+=( "gcc-gfortran" "libgomp" )
    library_dependencies_rhel_8+=( "gcc-gfortran" "libgomp" )
    library_dependencies_rhel_9+=( "gcc-gfortran" "libgomp" )
    library_dependencies_fedora+=( "gcc-gfortran" "libgomp" )
    library_dependencies_sles+=( "gcc-fortran" "libgomp1" )

    # wget is needed for blis
    if [[ ! -e "${build_dir}/deps/blis/lib/libblis.a" ]]; then
      library_dependencies_ubuntu+=("wget")
      library_dependencies_centos_rhel+=("wget")
      library_dependencies_centos_8+=("wget")
      library_dependencies_rhel_8+=("wget")
      library_dependencies_rhel_9+=("wget")
      library_dependencies_fedora+=("wget")
      library_dependencies_sles+=("wget")
    fi
  fi

  case "${ID}" in
    ubuntu)
      elevate_if_not_root apt-get update
      install_apt_packages "${library_dependencies_ubuntu[@]}"
      ;;

    centos)
      if (( "${VERSION_ID%%.*}" >= "8" )); then
        install_yum_packages "${library_dependencies_centos_8[@]}"
      else
  #     yum -y update brings *all* installed packages up to date
  #     without seeking user approval
  #     elevate_if_not_root yum -y update
        install_yum_packages "${library_dependencies_centos_rhel[@]}"
      fi
      ;;

    rhel)
      if (( "${VERSION_ID%%.*}" >= "9" )); then
        install_yum_packages "${library_dependencies_rhel_9[@]}"
      elif (( "${VERSION_ID%%.*}" >= "8" )); then
        install_yum_packages "${library_dependencies_rhel_8[@]}"
      else
  #     yum -y update brings *all* installed packages up to date
  #     without seeking user approval
  #     elevate_if_not_root yum -y update
        install_yum_packages "${library_dependencies_centos_rhel[@]}"
      fi
      ;;

    fedora)
#     elevate_if_not_root dnf -y update
      install_dnf_packages "${library_dependencies_fedora[@]}"
      ;;

    sles|opensuse-leap)
       install_zypper_packages "${library_dependencies_sles[@]}"
        ;;
    *)
      echo "This script is currently supported on Ubuntu, CentOS, RHEL, SLES, OpenSUSE-Leap, and Fedora"
      exit 2
      ;;
  esac

  if [[ ("${ID}" == "ubuntu") && ("${VERSION_ID}" == "16.04") && "${tensile_msgpack_backend}" == true ]]; then
    # On Ubuntu 16.04, the version of msgpack provided in the repository is outdated, so a newer version
    # must be manually downloaded and installed.  Trying to match or exceed Ubuntu 18 default
    if ! $(dpkg -s "libmsgpackc2" &> /dev/null) || $(dpkg --compare-versions $(dpkg-query -f='${Version}' --show libmsgpackc2) lt 2.1.5-1); then
      wget -nv -P ./ "http://ftp.us.debian.org/debian/pool/main/m/msgpack-c/libmsgpackc2_3.0.1-3_amd64.deb"
      wget -nv -P ./ "http://ftp.us.debian.org/debian/pool/main/m/msgpack-c/libmsgpack-dev_3.0.1-3_amd64.deb"
      elevate_if_not_root dpkg -i ./libmsgpackc2_3.0.1-3_amd64.deb ./libmsgpack-dev_3.0.1-3_amd64.deb
      rm libmsgpack-dev_3.0.1-3_amd64.deb libmsgpackc2_3.0.1-3_amd64.deb
    fi
  fi
}

# #################################################
# Pre-requisites check
# #################################################
# Exit code 0: alls well
# Exit code 1: problems with getopt
# Exit code 2: problems with supported platforms

# check if getopt command is installed
type getopt > /dev/null
if [[ $? -ne 0 ]]; then
  echo "This script uses getopt to parse arguments; try installing the util-linux package";
  exit 1
fi

# /etc/*-release files describe the system
if [[ -e "/etc/os-release" ]]; then
  source /etc/os-release
elif [[ -e "/etc/centos-release" ]]; then
  ID=$(cat /etc/centos-release | awk '{print tolower($1)}')
  VERSION_ID=$(cat /etc/centos-release | grep -oP '(?<=release )[^ ]*' | cut -d "." -f1)
else
  echo "This script depends on the /etc/*-release files"
  exit 2
fi

# The following function exits script if an unsupported distro is detected
supported_distro

# #################################################
# global variables
# #################################################
install_package=false
install_dependencies=false
install_prefix=rocblas-install
tensile_logic=asm_full
gpu_architecture=all
tensile_cov=
tensile_fork=
tensile_separate_architectures=true
tensile_tag=
tensile_test_local_path=
tensile_version=
tensile_lazy_library_loading=true
build_jobs=$(nproc)
build_library=true
build_cleanup=false
build_clients=false
build_clients_no_fortran=false
build_tensile=true
build_release=true
build_hip_clang=true
#use readlink rather than realpath for CentOS 6.10 support
build_dir=$(readlink -m ./build)
build_relocatable=false
skip_ld_conf_entry=false
static_lib=false
tensile_msgpack_backend=true
update_cmake=false
build_codecoverage=false
build_release_debug=false
build_address_sanitizer=false
build_freorg_bkwdcomp=true
declare -a cmake_options
declare -a cmake_client_options

rocm_path=/opt/rocm
if ! [ -z ${ROCM_PATH+x} ]; then
    rocm_path=${ROCM_PATH}
fi

library_dir_installed=${rocm_path}/rocblas

# #################################################
# Parameter parsing
# #################################################

# check if we have a modern version of getopt that can handle whitespace and long parameters
getopt -T
if [[ $? -eq 4 ]]; then
  GETOPT_PARSE=$(getopt --name "${0}" --longoptions help,install,jobs:,cleanup,clients,clients_no_fortran,clients-only,dependencies,debug,no_tensile,no-tensile,upgrade_tensile_venv_pip,msgpack,no-msgpack,library-path:,logic:,architecture:,cov:,fork:,branch:,build_dir:,test_local_path:,use-custom-version:,skipldconf,static,relocatable,cmake_install,codecoverage,relwithdebinfo,address-sanitizer,cmake-arg:,rm-legacy-include-dir,merge-architectures,no-merge-architectures,lazy-library-loading,no-lazy-library-loading --options rnhij:cdgkl:a:o:f:b:t:su: -- "$@")
else
  echo "Need a new version of getopt"
  exit 1
fi

if [[ $? -ne 0 ]]; then
  echo "getopt invocation failed; could not parse the command line";
  exit 1
fi

eval set -- "${GETOPT_PARSE}"

while true; do
  case "${1}" in
    -h|--help)
        display_help
        exit 0
        ;;
    -i|--install)
        install_package=true
        shift ;;
    -j|--jobs)
        build_jobs=${2}
        shift 2 ;;
    -d|--dependencies)
        install_dependencies=true
        shift ;;
    --cleanup)
        build_cleanup=true
        shift ;;
    -c|--clients)
        build_clients=true
        shift ;;
    --clients-only)
        build_library=false
        build_clients=true
        shift ;;
    --clients_no_fortran)
        build_clients_no_fortran=true
        shift ;;
    --library-path)
        library_dir_installed=${2}
        shift 2 ;;
    -g|--debug)
        build_release=false
        shift ;;
    -l|--logic)
        tensile_logic=${2}
        shift 2 ;;
    --lazy-library-loading)
        tensile_lazy_library_loading=true
        shift ;;
    --no-lazy-library-loading)
        tensile_lazy_library_loading=false
        shift ;;
    -a|--architecture)
        gpu_architecture=${2}
        shift 2 ;;
    -o|--cov)
        tensile_cov=${2}
        shift 2 ;;
    -f|--fork)
        tensile_fork=${2}
        shift 2 ;;
    -b|--branch)
        tensile_tag=${2}
        shift 2 ;;
    -t|--test_local_path)
        tensile_test_local_path=${2}
        shift 2 ;;
    -n|--no_tensile|--no-tensile)
        build_tensile=false
        shift ;;
    --upgrade_tensile_venv_pip)
        upgrade_tensile_pip=true
        shift ;;
    --build_dir)
#use readlink rather than realpath for CentOS 6.10 support
        build_dir=$(readlink -m ${2})
        shift 2;;
    -r|--relocatable)
      skip_ld_conf_entry=true
      build_relocatable=true
      shift ;;
    --merge-architectures)
        tensile_separate_architectures=false
        tensile_lazy_library_loading=false
        shift ;;
    --no-merge-architectures)
        tensile_separate_architectures=true
        shift ;;
    --skipldconf)
        skip_ld_conf_entry=true
        shift ;;
    -s|--static)
        static_lib=true
        shift ;;
    -u|--use-custom-version)
        tensile_version=${2}
        shift 2;;
    --prefix)
        install_prefix=${2}
        shift 2 ;;
    --msgpack)
        tensile_msgpack_backend=true
        shift ;;
    --no-msgpack)
        tensile_msgpack_backend=false
        shift ;;
    --cmake_install)
        update_cmake=true
        shift ;;
    --codecoverage)
        build_codecoverage=true
        shift ;;
    -k|--relwithdebinfo)
        build_release=false
        build_release_debug=true
        shift ;;
    --address-sanitizer)
        build_address_sanitizer=true
        shift ;;
    --rm-legacy-include-dir)
        build_freorg_bkwdcomp=false
        shift ;;
    --cmake-arg)
        cmake_options+=("${2}")
        shift 2 ;;
    --) shift ; break ;;
    *)  echo "Unexpected command line parameter received; aborting";
        exit 1
        ;;
  esac
done

if [[ -z $tensile_cov ]]; then
    if [[ $build_hip_clang == true ]]; then
        tensile_cov=V3
    else
        echo "Currently the only supported code object version is V3"
        exit 2
    fi
elif [[ $tensile_cov != V3 ]]; then
  echo "Currently the only supported code object version is V3"
        exit 2
fi

set -x

printf "\033[32mCreating project build directory in: \033[33m${build_dir}\033[0m\n"

install_blis()
{
    #Download prebuilt AMD multithreaded blis
    if [[ ! -e "./blis/lib/libblis.a" ]]; then
      case "${ID}" in
          centos|rhel|sles|opensuse-leap)
              wget -nv -O blis.tar.gz https://github.com/amd/blis/releases/download/2.0/aocl-blis-mt-centos-2.0.tar.gz
              ;;
          ubuntu)
              wget -nv -O blis.tar.gz https://github.com/amd/blis/releases/download/2.0/aocl-blis-mt-ubuntu-2.0.tar.gz
              ;;
          *)
              echo "Unsupported OS for this script"
              wget -nv -O blis.tar.gz https://github.com/amd/blis/releases/download/2.0/aocl-blis-mt-ubuntu-2.0.tar.gz
              ;;
      esac

      tar -xvf blis.tar.gz
      rm -rf blis/amd-blis-mt
      mv amd-blis-mt blis
      rm blis.tar.gz
      cd blis/lib
      ln -sf libblis-mt.a libblis.a
    fi
}

# #################################################
# prep
# #################################################
# ensure a clean build environment
if [[ "${build_release}" == true ]]; then
  rm -rf ${build_dir}/release
elif [[ "${build_release_debug}" == true ]]; then
  rm -rf ${build_dir}/release-debug
else
  rm -rf ${build_dir}/debug
fi

# Default cmake executable is called cmake
cmake_executable=cmake

if [[ "${build_hip_clang}" == true ]]; then
  cxx="hipcc"
  cc="hipcc"
  fc="gfortran"
else
  echo "Currently the only suppported compiler is hip-clang."
  exit 2
fi

# If user provides custom ${rocm_path} path for hcc it has lesser priority,
# but with hip-clang existing path has lesser priority to avoid use of installed clang++ by tensile
if [[ "${build_hip_clang}" == true ]]; then
  export PATH=${rocm_path}/bin:${rocm_path}/hip/bin:${rocm_path}/llvm/bin:${PATH}
fi

# #################################################
# dependencies
# #################################################
if [[ "${install_dependencies}" == true ]]; then
  CMAKE_VERSION=$(cmake --version | grep -oP '(?<=version )[^ ]*' )

  install_packages

  if [ -z "$CMAKE_VERSION" ] || $(dpkg --compare-versions $CMAKE_VERSION lt 3.16.8); then
      if $update_cmake == true; then
        pushd
        printf "\033[32mBuilding \033[33mcmake\033[32m from source; installing into \033[33m/usr/local\033[0m\n"
        CMAKE_REPO="https://github.com/Kitware/CMake/releases/download/v3.16.8/"
        mkdir -p ${build_dir}/deps && cd ${build_dir}/deps
        wget -nv ${CMAKE_REPO}/cmake-3.16.8.tar.gz
        tar -xvf cmake-3.16.8.tar.gz
        rm cmake-3.16.8.tar.gz
        cd cmake-3.16.8
        ./bootstrap --no-system-curl --parallel=16
        make -j16
        sudo make install
        popd
      else
          echo "rocBLAS requires CMake version >= 3.16.8 and CMake version ${CMAKE_VERSION} is installed. Run install.sh again with --cmake_install flag and CMake version 3.16.8 will be installed to /usr/local"
          exit 2
      fi
  fi

  # cmake is needed to install msgpack
  case "${ID}" in
    centos|rhel|sles|opensuse-leap)
      if [[ "${tensile_msgpack_backend}" == true ]]; then
        install_msgpack_from_source
      fi
      ;;
  esac

  if [[ "${build_clients}" == true ]]; then
    # The following builds googletest from source, installs into cmake default /usr/local
    pushd .
    printf "\033[32mBuilding \033[33mgoogletest; installing into \033[33m/usr/local\033[0m\n"
    mkdir -p ${build_dir}/deps && cd ${build_dir}/deps
    CXX=${cxx} CC=${cc} FC=${fc} ${cmake_executable} ${ROCBLAS_SRC_PATH}/deps
    make build_deps
    elevate_if_not_root make install_deps
    install_blis
    popd
  fi
elif [[ "${build_clients}" == true ]]; then
  pushd .
  mkdir -p ${build_dir}/deps && cd ${build_dir}/deps
  install_blis
  popd
fi

pushd .
  # #################################################
  # configure & build
  # #################################################

  cmake_options+=(
    "-DCMAKE_TOOLCHAIN_FILE=toolchain-linux.cmake"
    "-DROCM_PATH=${rocm_path}"
    "-DAMDGPU_TARGETS=${gpu_architecture}"
  )

  # build type
  if [[ "${build_release}" == true ]]; then
    mkdir -p ${build_dir}/release/clients && cd ${build_dir}/release
    cmake_options+=("-DCMAKE_BUILD_TYPE=Release")
  elif [[ "${build_release_debug}" == true ]]; then
    mkdir -p ${build_dir}/release-debug/clients && cd ${build_dir}/release-debug
    cmake_options+=("-DCMAKE_BUILD_TYPE=RelWithDebInfo")
  else
    mkdir -p ${build_dir}/debug/clients && cd ${build_dir}/debug
    cmake_options+=("-DCMAKE_BUILD_TYPE=Debug")
  fi

  # code coverage
  if [[ "${build_codecoverage}" == true ]]; then
      if [[ "${build_release}" == true ]]; then
          echo "Code coverage is disabled in Release mode, to enable code coverage select either Debug mode (-g|--debug) or RelWithDebInfo mode (-k|--relwithdebinfo); aborting";
          exit 1
      fi
      cmake_options+=("-DBUILD_CODE_COVERAGE=ON")
  fi

  if [[ "${static_lib}" == true ]]; then
    cmake_options+=("-DBUILD_SHARED_LIBS=OFF")
  fi


  # tensile options
  if [[ -n "${tensile_fork}" ]]; then
    cmake_options+=("-Dtensile_fork=${tensile_fork}")
  fi

  if [[ -n "${tensile_tag}" ]]; then
    cmake_options+=("-Dtensile_tag=${tensile_tag}")
  fi

  if [[ -n "${tensile_test_local_path}" ]]; then
    cmake_options+=("-DTensile_TEST_LOCAL_PATH=${tensile_test_local_path}")
  fi

  if [[ "${skip_ld_conf_entry}" == true ]]; then
    cmake_options+=("-DROCM_DISABLE_LDCONFIG=ON")
  fi

  if [[ -n "${tensile_version}" ]]; then
    cmake_options+=("-DTENSILE_VERSION=${tensile_version}")
  fi

  if [[ "${build_tensile}" == false ]]; then
    cmake_options+=("-DBUILD_WITH_TENSILE=OFF")
   else
    cmake_options+=("-DTensile_LOGIC=${tensile_logic}" "-DTensile_CODE_OBJECT_VERSION=${tensile_cov}")
    if [[ ${build_jobs} != $(nproc) ]]; then
      cmake_options+=("-DTensile_CPU_THREADS=${build_jobs}")
    fi
  fi

  if [[ "${upgrade_tensile_pip}" == true ]]; then
    cmake_options+=("-DTENSILE_VENV_UPGRADE_PIP=ON")
  fi

  if [[ "${tensile_separate_architectures}" == true ]]; then
    cmake_options+=("-DTensile_SEPARATE_ARCHITECTURES=ON")
  fi

  if [[ "${tensile_lazy_library_loading}" == true ]]; then
    cmake_options+=("-DTensile_LAZY_LIBRARY_LOADING=ON")
  fi

  if [[ "${tensile_msgpack_backend}" == true ]]; then
    cmake_options+=("-DTensile_LIBRARY_FORMAT=msgpack")
  else
    cmake_options+=("-DTensile_LIBRARY_FORMAT=yaml")
  fi


  if [[ "${build_clients}" == true ]]; then
    cmake_client_options+=(
      "-DBUILD_CLIENTS_SAMPLES=ON"
      "-DBUILD_CLIENTS_TESTS=ON"
      "-DBUILD_CLIENTS_BENCHMARKS=ON"
      "-DLINK_BLIS=ON"
      "-DBUILD_DIR=${build_dir}"
    )
    if [[ "${build_clients_no_fortran}" == true ]]; then
      cmake_client_options+=("-DBUILD_FORTRAN_CLIENTS=OFF")
    fi
  fi

  if [[ "${build_library}" == false ]]; then
    cmake_client_options+=("-DSKIP_LIBRARY=ON" "-DROCBLAS_LIBRARY_DIR=${library_dir_installed}")
  fi

  rocm_rpath=""
  if [[ "${build_relocatable}" == true ]]; then
      rocm_rpath=" -Wl,--enable-new-dtags -Wl,--rpath,/opt/rocm/lib:/opt/rocm/lib64"
      if ! [ -z ${ROCM_RPATH+x} ]; then
          rocm_rpath=" -Wl,--enable-new-dtags -Wl,--rpath,${ROCM_RPATH}"
      fi
  fi

  if [[ "${build_hip_clang}" == true ]]; then
      cmake_options+=("-DRUN_HEADER_TESTING=OFF")
  fi

  if [[ "${build_address_sanitizer}" == true ]]; then
    cmake_options+=("-DBUILD_ADDRESS_SANITIZER=ON")
  fi
  if [[ "${build_freorg_bkwdcomp}" == true ]]; then
    cmake_options+=("-DBUILD_FILE_REORG_BACKWARD_COMPATIBILITY=ON")
  else
    cmake_options+=("-DBUILD_FILE_REORG_BACKWARD_COMPATIBILITY=OFF")
  fi

  # Uncomment for cmake debugging
  # cmake_options+=("-Wdev" "--debug-output" "--trace")

  if [[ "${build_clients}" == true ]]; then
    cmake_options+=(${cmake_client_options[@]})
  fi

  # Build library with AMD toolchain because of existence of device kernels
  CXX=${cxx} CC=${cc} FC=${fc} ${cmake_executable} ${cmake_options[@]} -DCPACK_SET_DESTDIR=OFF -DCMAKE_INSTALL_PREFIX=rocblas-install -DCPACK_PACKAGING_INSTALL_PREFIX=${rocm_path} -DCMAKE_SHARED_LINKER_FLAGS="${rocm_rpath}" ${ROCBLAS_SRC_PATH}
  check_exit_code "$?"

  if [[ "${build_library}" == true ]]; then
    make -j${build_jobs} install
  else
    make -j${build_jobs}
  fi

  # pushd .
  # cd ../..
  # python3 ./rmake.py ${input_args}
  # popd

  check_exit_code "$?"

  # #################################################
  # install
  # #################################################
  # installing through package manager, which makes uninstalling easy
  if [[ "${install_package}" == true ]]; then
    make package
    check_exit_code "$?"

    case "${ID}" in
      ubuntu)
        elevate_if_not_root dpkg -i rocblas[_\-]*.deb
      ;;
      centos|rhel)
        elevate_if_not_root yum -y localinstall rocblas-*.rpm
      ;;
      fedora)
        elevate_if_not_root dnf install rocblas-*.rpm
      ;;
      sles|opensuse-leap)
        elevate_if_not_root zypper -n --no-gpg-checks install rocblas-*.rpm
      ;;
    esac

  fi
  check_exit_code "$?"

  if [[ "${build_cleanup}" == true ]]; then
      find -name '*.o' -delete
      find -type d -name '*build_tmp*' -exec rm -rf {} +
      find -type d -name '*_CPack_Packages*' -exec rm -rf {} +
  fi
  check_exit_code "$?"

popd
