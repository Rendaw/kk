Info =
{
	ProjectName = 'kk',
	Company = 'Zarbosoft',
	ShortDescription = 'JSON-based high level syntax tree specification and LLVM compiler.', -- One sentence
	ExtendedDescription = 'JSON-based high level syntax tree specification and LLVM compiler.', -- Three sentences
	Version = 0,
	Website = 'http://www.zarbosoft.com/PACKAGENAME',
	Forum = 'http://www.zarbosoft.com/forum/index.php?board=BOARDNUMBER',
	CompanyWebsite = 'http://www.zarbosoft.com/',
	Author = 'Rendaw',
	EMail = 'spoo@zarbosoft.com'
}

if arg and arg[1]
then
	print(Info[arg[1]])
end

