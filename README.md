# PhysMemEXE

PhysMemEXE is a program that allows your usermode process to gain access to all of physical memory, without the need of an open handle to "\device\physicalmemory"

## How it works

It works by injecting a set amount (n) of pdptes with page frame numbers ranging from 0 to n and the page size bit set to 1 into our process. These pdptes allow us to access all of physical memory as if it was normal virtual memory our process can access

## Credits

[kdmapper](https://github.com/TheCruZ/kdmapper) by TheCruZ and more
