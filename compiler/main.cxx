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
	
	auto MainGroup = new GroupT(HARDPOSITION);
	auto MakeString = [&](PositionT const Position, std::string const &Value)
	{
		auto Type = new StringTypeT(Position);
		Type->Static = false;
		auto Out = new Core::StringT(Position);
		Out->Data = Value;
		Out->Type = Type;
		return Out;
	};
	auto MakeInt = [&](PositionT const Position, int Value)
	{
		auto Type = new NumericTypeT(Position);
		Type->DataType = NumericTypeT::DataTypeT::Int;
		Type->Constant = true;
		Type->Static = false;
		auto Out = new NumericT<int>(Position);
		Out->Data = Value;
		return Out;
	};
	auto MakeFloat = [&](PositionT const Position, float Value)
	{
		auto Out = new NumericT<float>(Position);
		Out->Data = Value;
		return Out;
	};
	auto MakeElement = [&](PositionT const Position, std::string const &Key)
	{
		auto Out = new ElementT(Position);
		Out->Key = MakeString(Position, Key);
		return Out;
	};
	auto MakeDynamic = [&](PositionT const Position, NucleusT *&&Value)
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
	};
	auto MakeAssignment = [&](PositionT const Position, std::string const &Key, NucleusT *&&Value)
	{
		auto Assignment = new AssignmentT(Position);
		Assignment->Left = MakeElement(Position, Key);
		Assignment->Right = Value;
		MainGroup->Statements.push_back(Assignment);
	};
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
	MainGroup->Simplify({LLVM, Module, Block, nullptr, HARDPOSITION});
	
	Module->dump();
	
	return 0;
}

