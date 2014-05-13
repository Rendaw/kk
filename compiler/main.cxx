#include "core.h"

using namespace Core;

//================================================================================================================
// Main
int main(int, char **)
{
	//llvm::ReturnInst::Create(LLVM, Block);
	auto &LLVM = llvm::getGlobalContext();
	auto Module = new llvm::Module("asdf", LLVM);
	auto MainType = llvm::FunctionType::get(llvm::Type::getVoidTy(LLVM), false);
	auto Main = llvm::Function::Create(MainType, llvm::Function::ExternalLinkage, "main", Module);
	auto Block = llvm::BasicBlock::Create(LLVM, "entrypoint", Main);
	
	uint16_t TypeIDCounter = 1;
	
	auto MainGroup = new GroupT(HARDPOSITION);
	auto MakeString_ = [&](PositionT const Position, std::string const &Value)
	{
		auto Type = new StringTypeT(Position);
		Type->Static = false;
		auto Out = new Core::StringT(Position);
		Out->Initialized = true;
		Out->Data = Value;
		Out->Type = Type;
		return Out;
	};
	#define MakeString(...) MakeString_(HARDPOSITION, __VA_ARGS__)
	auto MakeInt_ = [&](PositionT const Position, int Value)
	{
		auto Type = new NumericTypeT(Position);
		Type->DataType = NumericTypeT::DataTypeT::Int;
		Type->Constant = true;
		Type->Static = false;
		auto Out = new NumericT<int>(Position);
		Out->Type = Type;
		Out->Initialized = true;
		Out->Data = Value;
		return Out;
	};
	#define MakeInt(...) MakeInt_(HARDPOSITION, __VA_ARGS__)
	/*auto MakeFloat = [&](PositionT const Position, float Value)
	{
		auto Out = new NumericT<float>(Position);
		Out->Data = Value;
		return Out;
	};*/
	auto MakeElement_ = [&](PositionT const Position, std::string const &Key)
	{
		auto Out = new ElementT(Position);
		Out->Key = MakeString_(Position, Key);
		return Out;
	};
	#define MakeElement(...) MakeElement_(HARDPOSITION, __VA_ARGS__)
	/*auto MakeDynamic = [&](PositionT const Position, NucleusT *&&Value)
	{
		auto Type = new NumericTypeT(Position);
		auto ValueType = Value->GetType({LLVM, Module, Block, nullptr, HARDPOSITION}).As<NumericTypeT>();
		Type->DataType = ValueType->DataType;
		Type->Constant = false;
		//Type->Static = ValueType->Static;
		Type->Static = false;
		auto Out = new ImplementT(Position);
		Out->Type = Type;
		Out->Value = Value;
		return Out;
	};
	auto MakeIntCast = [&](PositionT const Position, NucleusT *&&Value)
	{
		auto ValueType = Value->GetType({LLVM, Module, Block, nullptr, HARDPOSITION}).As<NumericTypeT>();
		auto Type = new NumericTypeT(Position);
		Type->DataType = NumericTypeT::DataTypeT::Int;
		Type->Constant = ValueType->Constant;
		Type->Static = ValueType->Static;
		auto Out = new ImplementT(Position);
		Out->Type = Type;
		Out->Value = Value;
		return Out;
	};*/
	auto MakeAssignment_ = [&](PositionT const Position, std::string const &Key, NucleusT *&&Value)
	{
		auto Assignment = new AssignmentT(Position);
		Assignment->Left = MakeElement_(Position, Key);
		Assignment->Right = Value;
		//MainGroup->Statements.push_back(Assignment);
		return Assignment;
	};
	#define MakeAssignment(...) MakeAssignment_(HARDPOSITION, __VA_ARGS__)
	
	#if 0
	/*MakeAssignment(HARDPOSITION, "a", 
		MakeString(HARDPOSITION, "hi"));
	MakeAssignment(HARDPOSITION, "b", 
		MakeString(HARDPOSITION, "bye"));
	MakeAssignment(HARDPOSITION, "b", 
		MakeElement(HARDPOSITION, "a"));
	MakeAssignment(HARDPOSITION, "c", 
		MakeInt(HARDPOSITION, 40));*/
	MakeAssignment(HARDPOSITION, "d", 
		MakeDynamic(HARDPOSITION, 
			MakeInt(HARDPOSITION, 35)));
	/*MakeAssignment(HARDPOSITION, "d", 
		MakeElement(HARDPOSITION, "c"));
	MakeAssignment(HARDPOSITION, "c", 
		MakeIntCast(HARDPOSITION, 
			MakeFloat(HARDPOSITION, 17.3)));
	MakeAssignment(HARDPOSITION, "d", 
		MakeElement(HARDPOSITION, "c"));*/
	MakeAssignment(HARDPOSITION, "d", 
		MakeIntCast(HARDPOSITION,
			//MakeFloat(HARDPOSITION, 202.4)));
			MakeFloat(HARDPOSITION, 7)));
	MakeAssignment(HARDPOSITION, "e",
		MakeDynamic(HARDPOSITION,
			MakeFloat(HARDPOSITION, 7)));
	#endif
	
	auto MakeGroup_ = [&](PositionT const Position, std::list<AtomT> Atoms)
	{
		auto Out = new GroupT(Position);
		for (auto &Atom : Atoms)
			Out->Statements.push_back(Atom);
		return Out;
	};
	#define MakeGroup(...) MakeGroup_(HARDPOSITION, __VA_ARGS__)

	auto MakeBlock_ = [&](PositionT const Position, std::list<AtomT> Atoms)
	{
		auto Out = new BlockT(Position);
		for (auto &Atom : Atoms)
			Out->Statements.push_back(Atom);
		return Out;
	};
	#define MakeBlock(...) MakeBlock_(HARDPOSITION, __VA_ARGS__)
	
	auto MakeImplement_ = [&](PositionT const Position, AtomT Type, AtomT Value)
	{
		auto Implement = new ImplementT(Position);
		Implement->Type = Type;
		Implement->Value = Value;
		return Implement;
	};
	#define MakeImplement(...) MakeImplement_(HARDPOSITION, __VA_ARGS__)
	
	auto MakeFunctionType_ = [&](PositionT const Position, AtomT Signature)
	{
		auto Type = new FunctionTypeT(Position);
		Type->ID = TypeIDCounter++;
		Type->Signature = Signature;
		return Type;
	};
	#define MakeFunctionType(...) MakeFunctionType_(HARDPOSITION, __VA_ARGS__)
	
	auto MakeIntType_ = [&](PositionT const Position)
	{
		auto Type = new NumericTypeT(Position);
		return Type;
	};
	#define MakeIntType() MakeIntType_(HARDPOSITION)
	
	auto MakeStringType_ = [&](PositionT const Position)
	{
		auto Type = new StringTypeT(Position);
		return Type;
	};
	#define MakeStringType() MakeStringType_(HARDPOSITION)
	
	auto MakeDynamic_ = [&](PositionT const Position, AtomT Value)
	{
		auto Out = new AsDynamicTypeT(Position);
		Out->Type = Value;
		return Out;
	};
	#define MakeDynamic(...) MakeDynamic_(HARDPOSITION, __VA_ARGS__)
	
	auto MakeCall_ = [&](PositionT const Position, AtomT Function, AtomT Input)
	{
		auto Out = new CallT(Position);
		Out->Function = Function;
		Out->Input = Input;
		return Out;
	};
	#define MakeCall(...) MakeCall_(HARDPOSITION, __VA_ARGS__)
	
	MainGroup->Statements.push_back(
		MakeAssignment("a",
			MakeImplement(
				MakeFunctionType(
					MakeGroup({
						MakeAssignment("input",
							MakeGroup({
								MakeAssignment("q", MakeIntType()),
								MakeAssignment("l", MakeStringType())
							})),
						MakeAssignment("output",
							MakeGroup({
								MakeAssignment("m", MakeDynamic(MakeIntType())),
								MakeAssignment("r", MakeDynamic(MakeIntType()))
							}))
					})),
				MakeBlock({
					MakeAssignment("output",
						MakeGroup({
							MakeAssignment("m", MakeInt(7)),
							MakeAssignment("r", MakeInt(13))
						}))
					}))));
	MainGroup->Statements.push_back(
		MakeAssignment("b",
			MakeCall(
				MakeElement("a"),
				MakeGroup({
					MakeAssignment("l", MakeString("normative")),
					MakeAssignment("q", MakeInt(44))
				}))));
	MainGroup->Statements.push_back(
		MakeAssignment("c",
			MakeCall(
				MakeElement("a"),
				MakeGroup({
					MakeAssignment("l", MakeString("normative")),
					MakeAssignment("q", MakeInt(44))
				}))));
	/*MainGroup->Statements.push_back(
		MakeAssignment("a",
			MakeImplement(
				MakeDynamic(
					MakeFunctionType(
						MakeGroup({
							MakeAssignment("input",
								MakeGroup({
									MakeAssignment("q", MakeDynamic(MakeIntType()))
								})),
							MakeAssignment("output",
								MakeGroup({
									MakeAssignment("m", MakeDynamic(MakeIntType())),
									MakeAssignment("r", MakeDynamic(MakeIntType()))
								}))
						}))),
				MakeBlock({
					MakeAssignment("output",
						MakeGroup({
							MakeAssignment("m", MakeInt(7)),
							MakeAssignment("r", MakeInt(13))
						}))
					}))));
	MainGroup->Statements.push_back(
		MakeAssignment("b",
			MakeCall(
				MakeElement("a"),
				MakeGroup({
					MakeAssignment("l", MakeString("normative")),
					MakeAssignment("q", MakeInt(44))
				}))));*/
	MainGroup->Simplify({LLVM, Module, Block, nullptr, HARDPOSITION, true});
	
	Module->dump();
	
	return 0;
}

