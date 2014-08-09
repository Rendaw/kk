DoOnce 'shared/Tupfile.lua'

local Compiler = Define.Executable
{
	Name = 'kkedit',
	Sources = Item 'main.cxx' + 'core.cxx' + 'atoms.cxx' + 'composite.cxx' + 'protoatom.cxx',
	Objects = SharedObjects,
	BuildFlags = tup.getconfig 'EDITORBUILDFLAGS' .. ' -fPIC',
	LinkFlags = tup.getconfig 'EDITORLINKFLAGS'
}
