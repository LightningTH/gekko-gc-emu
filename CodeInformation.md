The code that is being made public was a large work between Lightning and Shizzy with a few others like Chrono chipping in.

Shizzy did most of the work on the graphic code and quite a bit of the original underlying code for low level and the interpreter cpu.

Lightning's main focus was the dynarec. He also rewrote the HLE code while also reimplementing and reworking other parts of the code for speed.

Here are some rough notes about the code.

  * The dynarec has implemented most functions. It could use a few other enhancements to increase speed
  * There is commented code in the gx\_vertex file to quickly do a vertex list. It is disabled as it doesn't care about memory changes and did not increase the current speed of the emulator
  * The HLE function names were generated by the zelda map file. The same info was used to generate the HLE crc values
  * Sound does not work at all
  * There is a bug in the code somewhere resulting in the memory cards showing up as damaged that hasn't been solved
  * The dynarec uses an internal memory manager to avoid the slowness of window's memory manager

One idea for the dynarec was to re-scan thru the internal instruction list and make use of instructions like LEA to combine values instead of dealing with mov/add. Jumps were never implemented into the cache code so benefits could be seen by SHL and SHR if a known shift amount was being used.

The dynarec could also compile in a jump instruction to jump to the next code block to run. The issue that this would cause in the current code is no method of the wndproc to be checked for user interaction and the current compile function auto-executes the code. If the wndproc issue was solved, threading the cpu for instance, then the compile function can just compile and exit without needing to execute. It would just have to be called from a common compiled block that calls the compile function then re-executes the block. Any block compiled would just jump to another compiled block or jump to the function in memory that calls the compile function then re-jumps to the compiled block.

I have no issues with others using this code. I do ask that people let me know what they think or areas they use as I'm curious if any of this is useful to anyone. If you do use it in a project, please give credit where it is due.

-Lightning