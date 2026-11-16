# Environment for the rho-based UE subtraction until coresoftware PR #4465 is in
# a release. SDCC only. Source it, do not execute it:
#
#   source setup_rho.sh [install dir]      (default: <this repository>/install)
#
# It sets up the fixed ana build the PR libraries were built against and puts two local
# install areas in front of it:
#   1. your install dir, where you install src/myana
#   2. RHO_INSTALL: jetbase and jetbackground built with PR #4465
# Once the PR is in the "new" build, use the plain setup from the README instead.

RHO_RELEASE=ana.585
RHO_INSTALL=/sphenix/user/tmengel/fun4all_auau_example/install

if [ ! -f "$RHO_INSTALL/include/jetbackground/TowerRhoToBackground.h" ]; then
    echo "setup_rho.sh: $RHO_INSTALL not found or without the rho subtraction"
    return 1
fi

MYINSTALL="${1:-$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/install}"
# lib and include must exist now, or setup_local.sh leaves them out of the paths
mkdir -p "$MYINSTALL/lib" "$MYINSTALL/include" || return 1
MYINSTALL="$(cd "$MYINSTALL" && pwd)"
export MYINSTALL

source /opt/sphenix/core/bin/sphenix_setup.sh -n $RHO_RELEASE
# the first directory wins: libmyana must come from MYINSTALL
if [ "$MYINSTALL" -ef "$RHO_INSTALL" ]; then
    source $OPT_SPHENIX/bin/setup_local.sh "$MYINSTALL"
else
    source $OPT_SPHENIX/bin/setup_local.sh "$MYINSTALL" "$RHO_INSTALL"
fi

echo "setup_rho.sh: release $RHO_RELEASE, myana install $MYINSTALL"
echo "setup_rho.sh: rho subtraction libraries from $RHO_INSTALL"
