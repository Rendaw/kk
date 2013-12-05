#include <string>
#include <functional>
#include <map>
#include <iostream>
#include <cassert>
#include <memory>
#include <json/json.h>
#include <json/json_tokener.h>
#include <json/json_object.h>
#include <fstream>
#include <sstream>
#include <vector>
#include <list>

#include "type.h"
#include "extrastandard.h"

namespace
{

// Common types
struct PositionPartT
{
	std::string Key;
	int Index;

	PositionPartT(void) : Index(-1) {}

	bool operator !(void) const { return Key.empty() && (Index < 1); }

	PositionPartT(std::string const &Key) : Key(Key), Index(-1) { assert(!Key.empty()); }

	PositionPartT(int const &Index) : Index(Index) { assert(Index >= 0); }

	json_object *Get(json_object *Source) const
	{
		if (!*this) { assert(false); return nullptr; }
		if (Key.empty())
		{
			assert(json_object_is_type(Source, json_type_array));
			return json_object_array_get_idx(Source, Index);
		}
		else
		{
			assert(json_object_is_type(Source, json_type_object));
			return json_object_object_get(Source, Key.c_str());
		}
	}

	std::string AsString(void) const
	{
		if (!*this) return "(invalid)";
		else if (Key.empty()) return String() << Index;
		else return Key;
	}
};

struct TruePositionT : std::enable_shared_from_this<TruePositionT>
{
	std::shared_ptr<TruePositionT> const Parent;
	PositionPartT const Part;
	std::shared_ptr<TruePositionT> operator +(PositionPartT const &NewPart) const { return std::make_shared<TruePositionT>(shared_from_this(), Part); }
	std::string AsString(void) const
	{
		std::list<std::string> Parts;
		for (auto Part = this; Part; Part = Part->Parent.get()) Parts.push_front(Part->Part.AsString());
		String Out;
		for (auto const &Part : Parts) Out << "/" << Part;
		return Out;
	}
	private:
		friend std::shared_ptr<TruePositionT> NewPosition(PositionPartT const &FirstPart);
		TruePositionT(PositionPartT const &Part) : Part(Part) {}
		TruePositionT(std::shared_ptr<TruePositionT> &&Parent, PositionPartT const &Part) : Parent(std::move(Parent)), Part(Part) {}
};
typedef std::shared_ptr<TruePositionT> PositionT;
PositionT NewPosition(void) { return std::make_shared<TruePositionT>(); }

typedef std::string ParseErrorMessageT;

struct ParseErrorT
{
	PositionT Position;
	ParseErrorMessageT Message;
	ParseErrorT(void) {}
	ParseErrorT(PositionT const &Position, std::string const &Message) : Position(Position), Message(Message) {}
	bool operator !(void) { return !Position; }
};

// Element construction
struct ElementT;
std::map<std::string, std::function<std::unique_ptr<ElementT>(PositionT, json_object *)>> Constructors;

template <typename SpecificT> void AddConstructor(std::string const &Type)
	{ Constructors[Type] = [](PositionT const &Position, json_object *JSON) { return std::unique_ptr<ElementT>{new SpecificT(Position, JSON)}; }; }

ErrorOr<ParseErrorT, std::unique_ptr<ElementT>> Construct(std::string const Type, PositionT const &Position, json_object *JSON)
{
	auto Found = Constructors.find(Type);
	if (Found == Constructors.end()) return {Fail{}, {Position, String() << "Unknown element type '" << Type << "'"}};
	return Found->second(Position, JSON);
}

// Element tree construction
enum struct ParseStateResultT { StopE, ContinueE };

struct ParseStackElementT
{
	json_object *JSON;
	ElementT &Element;
};

typedef std::list<ParseStackElementT> ParseStackT;

struct ParseStateT
{
	virtual ~ParseStateT(void) {}
	virtual ErrorOr<ParseErrorT, ParseStateResultT> Execute(ParseStackT &Stack) = 0;
};

struct ElementT
{
	std::list<std::unique_ptr<ParseStateT>> ParseStates;

	ElementT(std::initializer_list<std::unique_ptr<ParseStateT>> DeepStates) : ParseStates(std::move(DeepStates)) {}
	virtual ~ElementT(void) {}

	Optional<ParseErrorT> Parse(ParseStackT &Stack)
	{
		if (ParseStates.empty())
		{
			Stack.pop_back();
			return {};
		}
		auto Result = ParseStates.front()->Execute(Stack);
		if (!Result) return Result.Error;
		if (*Result == ParseStateResultT::StopE)
			ParseStates.pop_front();
		return {};
	}
	
	virtual bool IsResultStatement(void) { return false; }
	virtual bool IsResultExpression(void) { return false; }
	virtual bool IsResultType(void) { return false; }
};

using SingleT = std::unique_ptr<ElementT>;
using MultipleT = std::vector<SingleT>;

template <Optional<ParseErrorMessageT> (*Check)(std::unique_ptr<ElementT> &Element)> struct ParseSingleT : ParseStateT
{
	PositionT const Position;
	std::string const Field;
	SingleT &Output;

	ParseSingleT(PositionT const &Position, std::string const &Field, SingleT &Output) :
		Position(Position), Field(Field), Output(Output) {}
	ErrorOr<ParseErrorT, ParseStateResultT> Execute(ParseStackT &Stack) override
	{
		auto &Top = Stack.back();

		json_object *ElementJSON = json_object_object_get(Top.JSON, Field.c_str());
		if (!ElementJSON) return {Position, String() << "Missing '" << Field << "'"};
		if (!json_object_is_type(ElementJSON, json_type_object)) return {Position, String() << "Element '" << Field << "' is not an object"};

		PositionT ElementPosition = *Position + Field;
		auto TypeJSON = json_object_object_get(ElementJSON, "type");
		if (!TypeJSON) return {ElementPosition, String() << "Missing element type"};
		if (!json_object_is_type(TypeJSON, json_type_string)) return {ElementPosition, String() << "Type is not a JSON string."};
		std::string const Type{json_object_get_string(TypeJSON), json_object_get_string_len(TypeJSON)};
		auto Element = Construct(Type, ElementPosition, ElementJSON);
		if (!Element) return Element.Error;
		auto CheckResult = Check(*Element);
		if (CheckResult) return {ElementPosiiton, *CheckResult};
		Output.reset(*Element);
		Stack.emplace_back(ElementJSON, Element);

		return ParseStateResultT::StopE;
	}
};

template <Optional<ParseErrorMessageT> (*Check)(std::unique_ptr<ElementT> &Element)> struct ParseMultipleT : ParseStateT
{
	PositionT const Position;
	std::string const Field;
	MultipleT &Output;
	json_object *FieldJSON;
	int Index;

	ParseMultipleT(PositionT Position, std::string const &Field, MultipleT &Output) :
		Position(Position), Field(Field), Output(Output), Index(-1) {}
	ErrorOr<ParseErrorT, ParseStateResultT> Execute(ParseStackT &Stack) override
	{
		auto &Top = Stack.back();

		if (Index == -1)
		{
			FieldJSON = json_object_object_get(Top.JSON, Field.c_str());
			if (!FieldJSON) return {Position, String() << "Missing '" << Field << "'"};
			if (!json_object_is_type(FieldJSON, json_type_array)) return {Position, String() << "Field '" << Field << "' is not an array"};
		}

		PositionT FieldPosition = *Position + Field;
		auto ElementJSON = json_object_array_get_idx(FieldJSON, Index);
		if (!ElementJSON) return ParseStateResultT::StopE;
		if (!json_object_is_type(ElementJSON, json_type_object)) return {FieldPosition, String() << "Index " << Index << " is not an object."};
		PositionT ElementPosition = *FieldPosition + Index;
		auto TypeJSON = json_object_object_get(ElementJSON, "type");
		if (!TypeJSON) return {ElementPosition, String() << "Missing element type"};
		if (!json_object_is_type(TypeJSON, json_type_string)) return {ElementPosition, String() << "Type is not a JSON string."};
		std::string const Type{json_object_get_string(TypeJSON), json_object_get_string_len(TypeJSON)};
		auto Element = Construct(Type, ElementPosition, ElementJSON);
		if (!Element) return Element.Error;
		auto CheckResult = Check(*Element);
		if (CheckResult) return {ElementPosiiton, *CheckResult};
		Output.push_back(*Element);
		Stack.emplace_back(ElementJSON, Element);

		return ParseStateResultT::ContinueE;
	}
};

Optional<ParseErrorMessageT> CheckStatement(std::unique_ptr<ElementT> &Element)
{
	
}

struct StatementT : ElementT
{
	using ElementT::ElementT;
	bool IsResultStatement(void) { return true; }
};

struct BlockT : StatementT
{
	MultipleT Statements;
	BlockT(PositionT const &Position, json_object *JSON) : StatementT{{make_unique<ParseMultipleT<CheckStatement>>(Position, "statements", Statements)}} {}
};

struct ExpressionT : ElementT
{
	using ElementT::ElementT;
	bool IsResultStatement(void) { return true; }
	bool IsResultExpression(void) { return true; }
};

struct TranslationUnitT : ElementT
{
	MultipleT Statements;
	TranslationUnitT(void) : ElementT{{make_unique<ParseMultipleT<CheckStatement>>(PositionT(), "statements", Statements)}} {}
	bool IsResultStatement(void) { return false; }
};

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
		std::function<void(void)> Start;
		std::function<bool(std::string const &Argument)> Handle;
		Option(void) {}
		Option(std::string Help, std::function<bool(std::string const &Argument)> Handle, std::function<void(void)> Start = {}) : Help(Help), Start(Start), Handle(Handle) {}
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
					std::cout << "\t" << Option.first << "\n";
				std::cout << std::endl;
			};
			if (Argument.empty()) { DisplayAll(); return true; }
			auto Found = Options.find(Argument);
			if (Found == Options.end()) { std::cerr << "Unknown flag '" << Argument << "'." << std::endl; return false; }
			std::cout << "\t" << Argument << " " << Found->second.Help << std::endl;
			return true;
		}
	};

	decltype(Options)::iterator OptionState = Options.end();
	for (int ArgumentIndex = 1; ArgumentIndex < ArgumentCount; ++ArgumentIndex)
	{
		std::string Argument = Arguments[ArgumentIndex];
		auto Found = Options.find(Argument);
		if (Found != Options.end())
		{
			OptionState = Found;
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
	std::unique_ptr<json_object, decltype(json_object_put)*> Root(&json_object_put);
	{
		std::unique_ptr<json_tokener, decltype(json_tokener_free)*> Tokener(json_tokener_new(), &json_tokener_free);
		std::stringstream Buffer;
		if (Input.empty())
		{
			if (!std::cin)
			{
				std::cerr << "Error: stdin is unreadable." << std::endl;
				return 1;
			}
			Buffer << std::cin.rdbuf();
		}
		else
		{
			std::ifstream Source(Input);
			if (!Source)
			{
				std::cerr << "Error: Couldn't open " << Input << "." << std::endl;
				return 1;
			}
			Buffer << Source.rdbuf();
		}
		Root.reset(json_tokener_parse_ex(Tokener.get(), Buffer.str().c_str(), Buffer.str().length()), &json_object_put);
		if (!Root)
		{
			std::cerr << "Error: Source file '" << Input << "' is an incomplete JSON file." << std::endl;
			return 1;
		}

	}

	std::unique_ptr<Element> TopElement{new TranslationUnit};
	Stack.emplace_back(TopElement.get(), Root);

	while (!Stack.empty())
	{
		auto Error = Stack.back().Element.Parse(Stack);
		if (Error)
		{
			std::cerr << "Error (" << Error->Position << "): " << Error->Message << std::endl;
			return 1;
		}
	}

	return 0;
}

}
