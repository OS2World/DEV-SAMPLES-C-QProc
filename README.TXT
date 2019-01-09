The files contained here are sample code to a PM Queue Processor
under OS2 1.2.  There have been requests by many folks on information
on how one would write a queue processor under the os2 1.2 spooler.
Queue processors are used to manipulate the output of spooled
jobs (ie see the text of the job and do some actions on it
prior to it being spooled to the device).  This code will not
build as supplied.  There are supporting header files and such
that are not suppiled.  This code should be used as an example
only and is not supported.  So someone wishing to write their
own queue processor should use this code as a starting base
(ie a shell to their queue processor), and rip out the 
guts of it and supply their own code to do whatever processing
they require of the spooled data.  For more information
on queue processors, you should consult the OS2 DDK.

11/17/90
