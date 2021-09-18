# OS-Assignment

## Files Description

- The patch contains the code for the kernel space.
- The files in `user` folder contains code for user space.

## Assignment Description

### Background  
Physical memory is often over-committed by operating systems and hypervisors wherein the total memory demand
of applications exceeds the memory capacity available in the system. This assignment is related to two of the
techniques that are used to facilitate memory over-commitment. The first technique (referred to as swapping) uses
pre-configured disk space to swap out (infrequently accessed) pages from memory. Demand paging loads these
pages back into memory when they are accessed by the applications. The second technique (referred to as memory
ballooning) is commonly used in virtualized environments. Under ballooning, the guest OS running inside a virtual
machine is responsible for releasing some of its memory upon hypervisor request. It is important to note that under
ballooning, the virtual machine itself is responsible for saving its data before releasing pages back to the hypervisor.  

### Assignment summary
In this assignment, you will implement application-level ballooning. However, unlike traditional ballooning, you
will seek support from the OS to save your data before releasing memory. At a high-level, we require you to build
two components — one in kernel space and one in user space. The user space component will implement a page
replacement policy of your choice while the kernel space component will implement the necessary mechanisms.
In the kernel space component, you are responsible for checking the state of free memory in the system. When the
amount of free memory falls below the specified threshold, you will send a signal SIGBALLOON to the application.
Note that no such signal exists in the kernel today – you will create one for this assignment. In response to this
signal, the application will issue one or more system calls asking for some of its pages to be swapped. The system
call handler in the kernel will implement the actual page replacement mechanism.
In the user space component, you are free to implement any page replacement policy. Upon receiving the
SIGBALLOON signal, your policy will first decide which page(s) it wants to swap out to disk. To choose an efficient
page replacement policy, you can use the “idle page tracking” and “pagemap” infrastructure of Linux. It will help
you in estimating the access patterns of your applications [3]. While we will not expose you to the source (except for
some simple examples), you can assume that the application adheres to the basic principles of spatial and temporal
locality.
Note: Note that only your main application will participate in ballooning via swapping. You need to disable
swapping for all other applications in the system.

#### Assignment-1 (Due date: 10/04/2021)
Kernel space: Implement a system call that an application can use to register itself with the ballooning
subsystem in the kernel. Your system call is expected to do the following: (1) disable default swapping algorithm of the kernel – no pages from other applications can be swapped after the system call returns. (2) set up a mechanism
that will deliver the SIGBALLOON signal to the registered application. The signal must be delivered when the
amount of free memory falls below 1GB.
User space: In the user space component: (1) register the application with the kernel’s ballooning component
using the system call as discussed above, and (2) set up a handler to receive the SIGBALLOON signal from the
kernel. File “main.c” contains a variable named nr signals – you are expected to increment it each time the signal
is received in the application.

#### Assignment-2 (Due date: 10/05/2021)
User space: In the user space component, implement a page replacement policy to decide page(s) to be swapped.
For this, you will need to introduce one or more system calls to notify the kernel of page(s) that you want to swap.
Keep in mind that swapped out pages can be accessed by the application at any point. You also need to ensure
that demand loading these pages back in memory works as expected. It may require you to send more signals to the
application and swap out other pages if free memory is less than 1GB. Optimize your implementation to minimize I/O
activities. Follow the basic principles of spatial and temporal locality while optimizing your policy. After receiving
the signal, the application has to respond with pages to swap within 10 seconds.
Kernel space: Implement the systems call(s) that will swap the application specified pages. There is no
restriction on the design of system call interface – you can pass as many number or type of arguments as you wish.
Deliverables: A single kernel patch that contains all your changes and “main.c” user space file. The kernel
patch must also include your changes from assignment-1.

#### Assignment-3 (Due date: 10/06/2021)
Kernel space: Extend your kernel implementation from assignment-2 to support 2MB transparent huge pages
(THP). A user can enable THP by setting the flag “/sys/kernel/mm/transparent hugepage/enabled” to “always”.
Depending on system configuration, state of memory fragmentation, and size or alignment of the virtual address
regions, it is possible that some parts of the user memory are mapped with 2MB pages while the others are mapped
with 4KB pages. Your implementation should handle all possible mapping scenarios. Options: While swapping out
a specific address range, you may choose to swap out its 2MB pages as is (coarse-grained) or may split 2MB pages
into 4KB pages to achieve fine-grained swapping at 4KB granularity.
User space: Extend the user-level page replacement policy to work with THP. You can modify the system call
interface in any way you want (e.g., your policy may prefer coarse-grained or fine-grained swapping depending on
access patterns – the system call can be used to pass these policy decisions to the kernel).

