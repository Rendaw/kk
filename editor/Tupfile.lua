DoOnce 'shared/Tupfile.lua'

local Compiler = Define.Executable
{
	Name = 'kkedit',
	Sources = Item 'main.cxx' + 'core.cxx',
	Objects = SharedObjects,
	BuildFlags = tup.getconfig 'EDITORBUILDFLAGS' .. ' -fPIC',
	LinkFlags = tup.getconfig 'EDITORLINKFLAGS'
}
