
This file documents the runtime conventions and requirements for coyotos.

Model-independent
-----------------

	Use of Capability registers:

		 0  [K]  Null
		 1  [RT,IDL,PS] Endpoint capability to reply endpoint
		 2  [RT,PS]     Process capability to self
		 3  [RT,PS]     SpaceBank
		 4  [RT,PS]     Tools space (replaces the KeyKOS/EROS
                                "constituents" node)
            
		 5  [PS]	Endpoint capability to initial invoke endpt
	         6  [C]         Creator-provided extra capability,
		                used for runtime environment.
		 7  [PS,CONS]	Entry capability to Address Space handler

	         7  [RT]        May be clobbered by low-level startup
	         8  [RT]        May be clobbered by low-level startup
	         9  [RT]        May be clobbered by low-level startup
	         10 [RT]        May be clobbered by low-level startup

	         5-23 registers available to the application

		 24 [IDL] Reply cap arg 0
		 25 [IDL] Reply cap arg 1
		 26 [IDL] Reply cap arg 2
		 27 [IDL] Reply cap arg 3

		 28 [IDL] Entry capability to return to (cap arg 0)
		 29 [IDL] Cap arg 1
		 30 [IDL] Cap arg 2
		 31 [IDL] Cap arg 3

                 Note that in some runtime models, the early startup
                 code requires capability registers for low-level
                 setup. When setting up a process in mkimage, it
                 should be assumed that capability registers 7 through
                 10 may be clobbered by the time main() receives control.

        Startup Conventions

                 At program startup time, it is assumed that an
		 initial reply endpoint for this program has already
		 been generated. An endpoint capability to this
		 endpoint has been placed in CR1. The endpoint has
		 it's PM bit set, an initial PP value of zero, an
		 endpoint ID of zero, and its receiver field contains
		 a process capability to this process.

		 An initial invokable endpoint has also been generated,
		 and an Endpoint capability to it is placed in CR5.  Its
		 endpoint PM bit is *not* set, its endpoint ID is one, and
		 its receiver field contains a process capability to this
		 process.

		 A Process capability for the process is placed in CR2.
		 
		 The Bank for the process is placed in CR3.  All objects
		 constituting the process should be derived from this
		 bank.  Upon destruction, the process will destroy the
		 bank to clean up its storage.  

        Tool Space Conventions

                 Slot     Use
		 0	  Discrim capability
		 1	  Sensory capability to ProtoSpace address space. 
		 2	  Space bank verifier [provisional]
		 3	  Constructor verifier
		 4	  Audit log capability [if present]

        C Runtime Environment Space Conventions

                 Offset   Use
		 0	  stdin
		 16	  stdout
		 32	  stderr
			  ??? command line and environment ???

Small-model
-----------

	A small-model process is a process whose address space fits in
	a single GPT.  Text starts at 1*PAGESIZE. Data and BSS
	follow. Data is NOT aligned to the next page boundary.  The
	heap starts at the end of BSS. The data stack is a single page
	at 15*PAGESIZE. The capability stack, if present, is a single
	capability page at 14*PAGESIZE.

	The heap is only allowed to grow to end at 14*PAGESIZE, to
	leave a guard page between the heap and the stack. [Note: the
	small model malloc *could* be designed to grow further by
	extending above the stack, abandoning the small space
	optimization.]

	A small-model process starts with a weak GPT, with an
	l2v of PAGEL2V, an l2g of PAGEL2V+4, and a zero guard.  The
	libsmall-domain.a library gains control early by means of a
	model startup hook in crt0. It allocates a writable GPT and
	page from the spacebank, copies the existing address space GPT
	into the new GPT, install the page into slot 15, and install
	the new GPT as the address space root.  At this point, it sets
	up the stack pointer, and proceeds to copy the data pages and
	set up the BSS.  Once that is done, control passes to the
	standard C runtime setup, which calls main().

Large-model
-----------

	XXX to be written

