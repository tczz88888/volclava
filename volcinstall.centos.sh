#!/bin/bash

function usage() {
    echo "Usage: volcinstall.centos.sh [--help] [--type=code|rpm] [--prefix=/opt/volclava]"
}


TYPE="code"
PACKAGE_NAME="volclava-1.0"
PREFIX="/opt/${PACKAGE_NAME}"
setPrefix=0

while [ $# -gt 0 ]; do
    case $1 in
        --type=*)
            TYPE=$(echo $1 | awk -F "=" '{print $2}')
            if [ -z $TYPE ]; then
                usage
                exit 1
            fi
            if [ $TYPE == "rpm" -a $setPrefix -eq 0 ]; then
                PREFIX="/opt"
            fi
            ;;
        --prefix=*)
            PREFIX=$(echo $1 | awk -F "=" '{print $2}')
            if [ -z $PREFIX ]; then
                usage
                exit 1
            fi
            setPrefix=1
            ;;
        --help)
            usage
            exit 0
            ;;
        *)
            usage
            exit 1
            ;;
    esac
    shift
done

if [ $TYPE = "code" ]; then
    # install volclava from source code
    #add user
    if ! id -u "volclava" > /dev/null 2>&1; then
        useradd "volclava"
    fi

    #install compile library
    if which yum > /dev/null 2>&1; then
        if ! yum list installed tcl-devel >/dev/null 2>&1; then
            yum install -y tcl-devel    
        fi
        if ! yum list installed ncurses-devel > /dev/null 2>&1; then
            yum install -y ncurses-devel
        fi
        yum groupinstall -y "Development Tools"
    else
        echo "Failed to find yum ..."
        exit 1
    fi

    #setup automake
    ./bootstrap.sh --prefix=$PREFIX

    #make and install
    if [ $? -eq 0 ]; then
        make
    else
        echo "Failed to setup autoconf and automake ..."
        exit 1 
    fi
    if [ $? -eq 0 ]; then
        make install
    else
        echo "Failed to execute make ..."
        exit 1
    fi

    #set up volclava service and shell environment
    chown volclava:volclava -R $PREFIX
    chmod 755 -R $PREFIX

    cp $PREFIX/etc/volclava /etc/init.d/
    cp $PREFIX/etc/volclava.* /etc/profile.d/

    chkconfig volclava on
    chkconfig --add volclava    
    echo -e "Congratulates, the volclava is installed under ${PREFIX}.\nYou can source environment by: source ${PREFIX}/etc/volclava.sh \nGo on to configure master/compute node and enjoy journey!"
elif [ $TYPE = "rpm" ]; then
   
    #rpm way to intall volclava 
    if which yum > /dev/null 2>&1; then
        if ! yum list installed rpm-build >/dev/null 2>&1; then
            yum install -y rpm-build
        fi
        if ! yum list installed rpmdevtools >/dev/null 2>&1; then
            yum install -y rpmdevtools
        fi
    else
        echo "Failed to find yum ..."
        exit 1
    fi
    
    chmod 755 rpm.sh
    chmod 755 bootstrap.sh

    #create rpm under ~/rpmbuild/RPMS/x86-64
    ./rpm.sh
    if [ $? -ne 0 ]; then
        echo "Failed to create volclava rpm package. Please check."
        exit 1
    fi

    #install volclava from rpm package
    cd ~/rpmbuild/RPMS/x86_64/
    chmod 755 volclava-1.0*
    if rpm -qa | grep volclava-1.0* > /dev/null 2>&1; then
        rpm -e volclava-1.0*
    fi
    rpm -ivh --prefix $PREFIX volclava-1.0*
    echo -e "Congratulates, the volclava is installed under ${PREFIX}/${PACKAGE_NAME}.\nYou can source environment by: source ${PREFIX}/${PACKAGE_NAME}/etc/volclava.sh \nGo on to configure master/compute node and enjoy journey!"
else
    usage
    exit 1
fi
