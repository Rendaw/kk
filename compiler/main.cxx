#include "type.h"
#include "extrastandard.h"

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
#include <llvm/Module.h>
#include <llvm/Function.h>
#include <llvm/Type.h>
#include <llvm/DerivedTypes.h>
#include <llvm/LLVMContext.h>
#include <llvm/PassManager.h>
#include <llvm/Instructions.h>
#include <llvm/CallingConv.h>
#include <llvm/Bitcode/ReaderWriter.h>
#include <llvm/Analysis/Verifier.h>
#include <llvm/Assembly/PrintModulePass.h>
#include <llvm/Support/IRBuilder.h>
#include <llvm/ModuleProvider.h>
#include <llvm/Target/TargetSelect.h>
#include <llvm/ExecutionEngine/GenericValue.h>
#include <llvm/ExecutionEngine/JIT.h>
#include <llvm/Support/raw_ostream.h>

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

struct FullPositionT : std::enable_shared_from_this<FullPositionT>
{
	std::shared_ptr<FullPositionT> const Parent;
	PositionPartT const Part;
	std::shared_ptr<FullPositionT> operator +(PositionPartT const &NewPart) const { return std::make_shared<FullPositionT>(shared_from_this(), Part); }
	std::string AsString(void) const
	{
		std::list<std::string> Parts;
		for (auto Part = this; Part; Part = Part->Parent.get()) Parts.push_front(Part->Part.AsString());
		String Out;
		for (auto const &Part : Parts) Out << "/" << Part;
		return Out;
	}
	private:
		friend std::shared_ptr<FullPositionT> NewPosition(PositionPartT const &FirstPart);
		FullPositionT(PositionPartT const &Part) : Part(Part) {}
		FullPositionT(std::shared_ptr<FullPositionT> &&Parent, PositionPartT const &Part) : Parent(std::move(Parent)), Part(Part) {}
};
typedef std::shared_ptr<FullPositionT> PositionT;
PositionT NewPosition(void) { return std::make_shared<FullPositionT>(); }

typedef std::string RecurseErrorMessageT;

struct RecurseErrorT
{
	PositionT Position;
	RecurseErrorMessageT Message;
	RecurseErrorT(void) {}
	RecurseErrorT(PositionT const &Position, std::string const &Message) : Position(Position), Message(Message) {}
	bool operator !(void) { return !Position; }
};

// Element construction
struct ElementT;
std::map<std::string, std::function<std::unique_ptr<ElementT>(PositionT, json_object *)>> Constructors;

template <typename SpecificT> void AddConstructor(std::string const &Type)
	{ Constructors[Type] = [](PositionT const &Position, json_object *JSON) { return std::unique_ptr<ElementT>{new SpecificT(Position, JSON)}; }; }

ErrorOr<RecurseErrorT, std::unique_ptr<ElementT>> Construct(std::string const Type, PositionT const &Position, json_object *JSON)
{
	auto Found = Constructors.find(Type);
	if (Found == Constructors.end()) return {Fail{}, {Position, String() << "Unknown element type '" << Type << "'"}};
	try { return Found->second(Position, JSON); }
	catch (RecurseErrorT const &Error) { return {Fail{}, Error}; }
}

// Element tree construction
struct RecurseStopT {};
struct RecurseStayT {};
typedef RecurseReplaceT StrictType(ElementT *); // Only used for simplification?
typedef RecursePushT StrictType(std::tuple<ElementT &, json_object *>);

struct ParseStackElementT
{
	json_object *JSON;
	ElementT &Element;
};

typedef std::list<ParseStackElementT> ParseStackT;

typedef std::function<ErrorOr<RecurseErrorT, RecurseStateResultT>(ParseStackT &Stack)> RecurseStateT;

template <typename StackT> Recurse(StackT &&Stack)
{
	while (!Stack.empty())
	{
		auto Error = Stack.back().Element.Parse(Stack);
		if (Error)
		{
			std::cerr << "Error (" << Error->Position << "): " << Error->Message << std::endl;
			return 1;
		}
	}
}

struct RecursableT
{
	std::list<RecurseStateT> States;
	RecursableT(std::list<RecurseStateT> &&States) : States(std::move(States)) {}
	Optional<RecurseErrorT> operator(StackT &Stack)
	{
		if (ParseStates.empty())
		{
			Stack.pop_back();
			return {};
		}
		auto Result = ParseStates.front()->Execute(Stack);
		if (!Result) return Result.Error;
		if (*Result == RecurseStateResultT::StopE)
			ParseStates.pop_front();
		return {};
	}
};

struct ElementT
{
	RecursableT<ParseStackT> Parse;
	RecursableT<SimplifyStackT> Simplify;

	ElementT(std::list<RecurseStateT> &&ParseStates, std::list<RecurseStateT> &&SimplifyStates) : Parse(std::move(ParseStates)), Simplify(std::move(SimplifyStates)) {}
	virtual ~ElementT(void) {}
};

using SingleT = std::unique_ptr<ElementT>;
using MultipleT = std::vector<SingleT>;

RecurseStateT ParseSingle(PositionT const &Position, std::string const &Field, SingleT &Output)
{
	return [=, &Output](ParseStackT &Stack)
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
		Output.reset(*Element);
		Stack.emplace_back(ElementJSON, Element);

		return RecurseStateResultT::StopE;
	};
}

RecurseStateT ParseMultiple(PositionT Position, std::string const &Field, MultipleT &Output)
{
	json_object *FieldJSON = nullptr;
	int Index = -1;
	return [=, &Output](ParseStackT &Stack)
	{
		auto &Top = Stack.back();

		if (Index == -1)
		{
			if (Field.empty())
				FieldJSON = Top.JSON;
			else
			{
				FieldJSON = json_object_object_get(Top.JSON, Field.c_str());
				if (!FieldJSON) return {Position, String() << "Missing '" << Field << "'"};
				if (!json_object_is_type(FieldJSON, json_type_array)) return {Position, String() << "Field '" << Field << "' is not an array"};
			}
			Index = 0;
		}

		PositionT FieldPosition = *Position + Field;
		auto ElementJSON = json_object_array_get_idx(FieldJSON, Index);
		if (!ElementJSON) return RecurseStateResultT::StopE;
		if (!json_object_is_type(ElementJSON, json_type_object)) return {FieldPosition, String() << "Index " << Index << " is not an object."};
		PositionT ElementPosition = *FieldPosition + Index;
		auto TypeJSON = json_object_object_get(ElementJSON, "type");
		if (!TypeJSON) return {ElementPosition, String() << "Missing element type"};
		if (!json_object_is_type(TypeJSON, json_type_string)) return {ElementPosition, String() << "Type is not a JSON string."};
		std::string const Type{json_object_get_string(TypeJSON), json_object_get_string_len(TypeJSON)};
		auto Element = Construct(Type, ElementPosition, ElementJSON);
		if (!Element) return Element.Error;
		Output.push_back(*Element);
		Stack.emplace_back(ElementJSON, Element);

		return RecurseStateResultT::ContinueE;
	};
}

Optional<RecurseErrorMessageT> FailNotType(ElementT *Element);
Optional<RecurseErrorMessageT> FailNotExpression(ElementT *Element);
Optional<RecurseErrorMessageT> FailNotStatement(ElementT *Element);

// Types
struct StringTypeT : ElementT { IntegerTypeT(PositionT const &Position, json_object *JSON) {} };

struct IntegerTypeT : ElementT { IntegerTypeT(PositionT const &Position, json_object *JSON) {} };

struct MemoryTypeT : ElementT { MemoryTypeT(PositionT const &Position, json_object *JSON) {} };

struct RecordTypeElementT : ElementT
{
	SingleT Name;
	SingleT Type;
	RecordTypeElementT(PositionT const &Position, json_object *JSON) : ElementT
	{
		ParseSingle(Position, "name", Name),
		ParseSingle(Position, "type", Type)
	} {}
};

struct RecordTypeT : ElementT
{
	MultipleT Elements;
	RecordTypeT(PositionT const &Position, json_object *JSON) : ElementT
	{
		ParseMultiple(Position, "elements", Elements)
	} {}
};

struct FunctionTypeT : ElementT
{
	SingleT Input;
	SingleT Output;
	FunctionTypeT(PositionT const &Position, json_object *JSON) : ElementT
	{
		ParseSingle(Position, "input", Input),
		ParseSingle(Position, "output", Output)
	} {}
};

// Type values
struct IntegerT : ElementT
{
	int Value;
	IntegerT(PositionT const &Position, json_object *JSON)
	{
		auto JSONValue = json_object_object_get(JSON, "value");
		if (!JSONValue) throw RecurseErrorT{Position, "'value' missing."};
		if (!json_object_is_type(Value, json_type_int)) throw RecurseErrorT{Position, "'value' is not an integer."};
		Value = json_object_get_int(JSONValue);
	}
};

struct StringT : ElementT
{
	std::string Value;
	StringT(PositionT const &Position, json_object *JSON)
	{
		auto JSONValue = json_object_object_get(JSON, "value");
		if (!JSONValue) throw RecurseErrorT{Position, "'value' missing."};
		if (!json_object_is_type(Value, json_type_string)) throw RecurseErrorT{Position, "'value' is not an string."};
		Value = {json_object_get_string(JSONValue), json_object_get_string_len(JSONValue)};
	}
};

struct RecordElementT : ElementT
{
	SingleT Name;
	SingleT Value;
	RecordElementT(PositionT const &Position, json_object *JSON) : ElementT
	{
		ParseSingle(Position, "name", Name),
		ParseSingle(Position, "value", Value)
	} {}
};

struct RecordT : ElementT
{
	MultipleT Elements; // Assignments
	RecordT(PositionT const &Position, json_object *JSON) : ElementT
	{
		ParseMultiple(Position, "elements", Elements)
	} {}

};

struct FunctionT : ElementT
{
	MultipleT Statements;
	FunctionT(PositionT const &Position, json_object *JSON) : ElementT
	{
		ParseMultiple(Position, "statements", Statements)
	} {}
};

// General Expressions
struct NameT : ElementT
{
	SingleT Name;
	NameT(PositionT const &Position, json_object *JSON) : ElementT
	{
		ParseSingle(Position, "name", Name)
	} {}
};

struct ValueT : ElementT
{
	SingleT Type;
	SingleT Value;
	ValueT(PositionT const &Position, json_object *JSON) : ElementT
	{
		ParseSingle(Position, "type", Type),
		ParseSingle(Position, "value", Value)
	} {}
};

struct AddT : ElementT
{
	SingleT Left;
	SingleT Right;
	
	template <typename PrimitiveT> auto AddHelper(ElementT *Left, ElementT *Right) -> PrimitiveT *
	{
		auto ActualLeft = dynamic_cast<PrimitiveT *>(Left->Inner);
		auto ActualRight = dynamic_cast<PrimitiveT *>(Right->Inner);
		return new PrimitiveT(ActualLeft->Value + ActualRight->Value);
	}
	
	ValueT(PositionT const &Position, json_object *JSON) : ElementT
	(
		{
			ParseSingle(Position, "left", Left),
			ParseSingle(Position, "right", Right)
		},
		{
			SimplifySingle(Left),
			SimplifySingle(Right),
			[this](SimplifyStack &Stack)
			{
				if (IsPrimitive(Left) && IsPrimitive(Right))
				{
					auto LeftType = Type(Left);
					auto RightType = Type(Right);
					if (LeftType == RightType)
					{
						switch (LeftType.BaseID())
						{
							case IntegerT::ID: return RecurseReplaceT(AddHelper<IntegerT>(Left, Right));
							default: return RecurseErrorT(Position, String() << "Add is undefined on arguments of type " << LeftType.Name());
						}
					}
					return RecurseStopT{};
				}
			}
		}
	), Position(Position) {}
};

// Statements
struct AssignT : ElementT
{
	SingleT Name;
	SingleT Value;
	AssignT(PositionT const &Position, json_object *JSON) : ElementT
	(
		{ 
			make_unique<ParseSingleT(Position, "name", Name),
			make_unique<ParseSingleT(Position, "value", Value) 
		},
		{}
	) {}
};

// Program
struct ModuleT : ElementT
{
	MultipleT Statements;
	ModuleT(void) : ElementT
	(
		{ ParseMultiple(PositionT(), "statements", Statements) },
		{ SimplifyMultiple(Statements) }
	) {}
};

// Element classification
bool Is(ElementT *Element) { return false; }
template <typename... TypeT, typename... TypeTs> bool Is(ElementT *Element)
{
	if (typeid(*Element) == typeid(TypeT)) return true;
	return Is<TypeTs...>(Element);
}

Optional<RecurseErrorMessageT> FailNotType(ElementT *Element)
{
	if (Is<
		IntegerTypeT,
		MemoryTypeT,
		RecordTypeT,
		FunctionTypeT
	>(Element)) return {};
	return "Element must be a TYPE.";
}

Optional<RecurseErrorMessageT> FailNotExpression(ElementT *Element)
{
	if (!FailType(Element)) return {};
	if (Is<
		ValueT,
		FunctionT
	>(Element)) return {};
	return "Element must be an EXPRESSION.";
}

Optional<RecurseErrorMessageT> FailNotStatement(ElementT *Element)
{
	if (!FailExpression(Element)) return {};
	return "Element must be a STATEMENT.";
}

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

	// Read source
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

	// Parse
	std::unique_ptr<ModuleT> TopElement{new ModuleT};
	Stack.emplace_back(*TopElement->Definition, Root);

	while (!Stack.empty())
	{
		auto Error = Stack.back().Element.Parse(Stack);
		if (Error)
		{
			std::cerr << "Error (" << Error->Position << "): " << Error->Message << std::endl;
			return 1;
		}
	}

	// Massage tree
	TopElement->Simplify();

	// Generate IR
	auto Module = llvm::makeLLVMModule();
	auto TopIR = TopElement->Translate();

	{
		std::string Error;
		if (!llvm::verifyModule(Module, llvm::AbortProcessAction, &Error))
		{
			std::cerr << "Error: Generated invalid llvm module.  " << Error << std::endl;
			return 1;
		}
	}

	// Optimize and output
	llvm::PassManager OutputPasses;
	OutputPasses.add(new llvm::TargetData(TopIR));
	if (Output.empty())
	{
		if (!std::cout)
		{
			std::cerr << "Error: stdout is unwritable." << std::endl;
			return 1;
		}
		OutputPasses.add(llvm::createPrintModulePass(&llvm::fouts()));
	}
	else
	{
		std::string Error;
		llvm::tool_output_file *OutputStream = new llvm::tool_output_file(Output.c_str(), Error, 0);
		if (!OutputStream)
		{
			std::cerr << "Error: Unable to open output file.  " << Error << std::endl;
			return 1;
		}
		OutputPasses.add(llvm::createPrintModulePass(new llvm::formatted_raw_ostream(OutputStream->os())), true);
	}
	OutputPasses.run(*module);

	return 0;
}

}
