DoOnce 'ren-cxx-filesystem/Tupfile.lua'
DoOnce 'ren-cxx-serialjson/Tupfile.lua'

local Compiler = Define.Executable
{
	Name = 'kkedit',
	Sources = Item 'main.cxx' + 'core.cxx' + 'composite.cxx' + 'protoatom.cxx',
	Objects = FilesystemObjects + SerialJSONObjects,
	BuildFlags = tup.getconfig 'EDITORBUILDFLAGS' .. ' -fPIC',
	LinkFlags = tup.getconfig 'EDITORLINKFLAGS'
}
