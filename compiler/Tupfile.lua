local Compiler = Define.Executable
{
	Name = 'kk',
	Sources = Item 'main.cxx',
	LinkFlags = ' -ljson-c'
}
