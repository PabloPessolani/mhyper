Installing and using MHYPER
============================

WARNING: Current version of MHYPER is a prototype. The following step allow you to test it.
We recommend you to use to run MHYPER as a Guest of another Virtualization product like VMWARE or VIRTUAL BOX. Download VM-MYPER.exe (auto-extract) the VMWare virtual machine with the MHYPER.

If you want to install MHYPER from scratch:

1) Install MINIX 3.1.2a
2) rename /usr/src as /usr/src-minix
3) copy file  mhyper-20150308-FUNKA.tar.Z on /usr
4) decompress with: compress -d mhyper-20150308-FUNKA.tar.Z 
5) detar with: tar xvf mhyper-20150308-FUNKA.tar
6) cd src
7) make clean
8) make world
9) sync; sync; halt
10) restart Minix using boot menu option 3 
11 ) uname -a  must show 
Minix xxxxx.xxxxx.xxxxx 3 1.2H- iXXX
The "H" means the Hypervisor code is running.
12) set | grep VM
VM=VM0
means that the running VM is VM0
13) sync; sync; halt

If you are using VMWARE or VIRTUAL BOX
copy the virtual disk with MHYPER installed and configure it as the second IDE drive.
VM0 will use IDE0, and VM1 will use VM1

14) Restart Minix
15) cd /usr/src/test
16) ./vmmcmd LOAD MYMINIX & 
This command load a virtual machine named MYMINIX

16) ./vmmcmd START MYMINIX 32
This command start virtual machine MYMINIX allocating 32 tokens for its bucket (from 255)

Now you can see VM1 booting sequence, then the login prompt.

17) set | grep VM
VM=VM1
To test that you are running on VM1

18) mount 
You can see that VM1 present /dev/c0d0p0 (IDE0) as its boot device, but it was deceived by the Hypervisor.

To return to VM0 press ALT-DOWN
To return to VM1 press ALT-UP

On VM0 press F1 to see the list of proccess running on VM0 
you must see a process named VM1 which is VM1`s proxy/loader.

Pressing F12 the Information Server change to VM1
Now, press F1 to see the list of proccess running on VM1
Every Fx key refer to VM1

Up to now there is not a command to stop a VM.






 




