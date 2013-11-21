
int main(int ArgumentCount, char **Arguments)
{
	// Execution options
	bool OptionsOnly = false;
	std::string Input;
	std::string Output;

	// Command line option parsing
	struct Option
	{
		std::string Help;
		std::function<bool(std::string const &Argument)> Handle;
		std::function<void(void)> Start;
		Option(std::string Help, std::function<bool(std::string const &Argument)>, std::function<void(void)> Start = {}) : Help(Help), Start(Start), Handle(Handle) {}
	};
	std::map<std::string, Option> Options;

	Options["--in"] =
	{
		"FILENAME\tSpecify files to compile.  By default uses stdin.",
		[&](std::string const &Argument)
		{
			if (!Input.empty())
			{
				std::cerr << "You may only specify one input filename." << std::endl;
				return false;
			}
			Input = Argument;
			return true;
		}
	};
	Options["-i"] = Options["--in"];

	Options["--out"] =
	{
		"FILENAME\tSpecify compiled output filename.  By default uses stdout.",
		[&](std::string const &Argument)
		{
			if (!Output.empty())
			{
				std::cerr << "You may only specify one output filename." << std::endl;
				return false;
			}
			Output = Argument;
			return true;
		}
	};

	Options["--help"] =
	{
		"[FLAG]\tIf FLAG is specified, shows help for FLAG.  Otherwise displays all help options.",
		[&](std::string const &Argument)
		{
			OptionsOnly = true;
			auto DisplayAll = [&](void)
			{
				std::cout << "All options: \n";
				for (auto Option : Options)
					std::cout << "\t" << Option->first << "\n";
				std::cout << std::endl;
			};
			if (Argument.empty()) { DisplayAll(); return true; }
			auto Found = Options.find(Argument);
			if (Found == Options.end()) { std::cerr << "Unknown flag '" << Argument << "'." << std::endl; return false; }
			std::cout << "\t" << Argument << " " << Option->second.Help << std::endl;
			return true;
		}
	};

	decltype(Options)::iterator OptionState = Options.end();
	for (int ArgumentIndex = 0; ArgumentIndex < ArgumentCount; ++ArgumentIndex)
	{
		std::string Argument = Arguments[ArgumentIndex];
		auto Found = Options.find(Argument);
		if (Found != Options.end())
		{
			OptionState = Found->second.get();
			if (OptionState->second.Start)
				OptionState->second.Start();
			assert(OptionState->second.Handle);
		}
		else if ((Argument.size() > 1) && (Argument[0] == '-'))
		{
			std::cerr << "Unknown option '" << Argument << "'" << std::endl;
			return 1;
		}
		else if (OptionState == Options.end())
		{
			std::cerr << "Don't know how to handle argument '" << Argument << "'.  Please specify an option starting with - or -- first." << std::endl;
			return 1;
		}
		else
		{
			auto Result = OptionState->second.Handle(Argument);
			if (!Result)
			{
				std::cerr << "Error handling '" << OptionState->first << "' argument '" << Argument << "'\n"
					"\n\nHelp for '" << OptionState->first << "':\n"
					<< OptionState->first << " " << OptionState->second.Help << "\n" << std::endl;
				return 1;
			}
		}
	}

	if (OptionsOnly) return 0;

	// Execution
	{
		std::unique_ptr<json_tokener, decltype(json_tokener_free)> Tokener(json_tokener_new(), json_tokener_free);
		std::ifstream Source(Input);
		std::stringstream Buffer;
		Buffer << Source.rdbuf();
		std::unique_ptr<json_object, decltype(json_object_free)>
			JSON(
				json_tokener_parse_ex(Tokener.get(), Source.str().c_str(), Source.str().length()),
				json_object_free);
		if (!JSON)
		{
			std::cerr << "Error: Source file '" << Input << "' is an incomplete JSON file." << std::endl;
			return 1;
		}
	}

	return 0;
}

