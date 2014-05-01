local Compiler = Define.Executable
{
	Name = 'kk',
	Sources = Item 'main.cxx',
	BuildFlags = ' -I/usr/include/llvm-3.4 -I/usr/include/llvm-c-3.4',
	LinkFlags = ' -lLLVM-3.4'
	--LinkFlags = ' -ljson-c'
}
