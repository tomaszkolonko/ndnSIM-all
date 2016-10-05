ndnSIM
======

## Prerequisites

You need to have a working ns-3 installation with pybindgen. If you have it already just replace the ndnSIM folder within ns-3/src/ by cloning this repository into the correct location.

Otherwise install everything.

## installation

Go to: http://mohittahiliani.blogspot.ch/2015/10/ns-3-installing-ndnsim-on-ubuntu.html

Download the script and change line 51 to:

git clone --recursive https://github.com/tomaszkolonko/ndnSIM.git ns-3/src/ndnSIM

If you need NS_LOG output run ./waf within ns-3 with the following:

`./waf configure -d debug --enable-examples`

## Goal for this project

The goal for this project is to find a new multipath forwarding strategy for Content-Centric Networking in Vehicular ad-hoc Networks 

---------------------------------------------

For more information, including downloading and compilation instruction, please refer to
http://ndnsim.net or documentation in `docs/` folder.
